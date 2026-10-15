// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_device_stream.h"


// How soon the task looks again at a transfer that is running or at bytes the client left, in us.
#define RECHECK_PERIOD1US	1000


static uint16_t _actReceive(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
	PifDeviceStream *p_owner = (PifDeviceStream *)p_transport;
	uint16_t remain = p_owner->__rx_len - p_owner->__rx_pos;

	if (size > remain) size = remain;
	memcpy(p_data, p_owner->__p_rx + p_owner->__rx_pos, size);
	p_owner->__rx_pos += size;
	return size;
}

static uint16_t _actSend(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
	PifDeviceStream *p_owner = (PifDeviceStream *)p_transport;
	uint16_t remain = p_owner->__tx_size - p_owner->__tx_len;

	if (size > remain) size = remain;
	memcpy(p_owner->__p_tx + p_owner->__tx_len, p_data, size);
	p_owner->__tx_len += size;
	return size;
}

static BOOL _isAttached(PifDeviceStream *p_owner)
{
	return p_owner->__evt_parsing || p_owner->__evt_sending;
}

static BOOL _isValidProtocol(PifDeviceStreamProtocol *p_protocol)
{
	return p_protocol && p_protocol->__p_ops && p_protocol->__p_ops->start && p_protocol->__p_ops->finish;
}

// Starts the protocol's next transfer. Returns FALSE when none was started.
static BOOL _start(PifDeviceStream *p_owner)
{
	PifDeviceStreamProtocol *p_protocol = p_owner->_p_protocol;

	// The port held by another device is tried again later.
	if (pifDeviceReg_IsBusy(p_owner->_p_device)) return FALSE;
	if (!(*p_protocol->__p_ops->start)(p_protocol, p_owner)) return FALSE;
	p_owner->__running = TRUE;
	return TRUE;
}

/**
 * @brief Follows the transfer that is running to its end, and the one the protocol starts right after it.
 * @return FALSE while a transfer is still running.
 */
static BOOL _poll(PifDeviceStream *p_owner)
{
	PifDeviceStreamProtocol *p_protocol = p_owner->_p_protocol;
	PifDeviceRegState state;
	PifDeviceStreamNext next;

	while (p_owner->__running) {
		state = pifDeviceReg_CheckTransfer(p_owner->_p_device);
		if (state == DRS_RUN) return FALSE;

		p_owner->__running = FALSE;
		next = (*p_protocol->__p_ops->finish)(p_protocol, p_owner, state == DRS_COMPLETE);
		p_owner->__more = next == DSN_NEXT_RUN;
		if (next == DSN_NOW) _start(p_owner);
	}
	return TRUE;
}

// Calls evt_parsing while it makes progress. Returns TRUE when the client has read every byte.
static BOOL _parse(PifDeviceStream *p_owner)
{
	int i;

	if (!p_owner->__evt_parsing) {
		p_owner->__rx_pos = p_owner->__rx_len;
		return TRUE;
	}

	// Bounded, in case a client reports progress without reading anything.
	for (i = 0; i < 16 && p_owner->__rx_pos < p_owner->__rx_len; i++) {
		if (!(*p_owner->__evt_parsing)(p_owner->__p_client, _actReceive)) break;
	}
	return p_owner->__rx_pos >= p_owner->__rx_len;
}

/**
 * @brief Collects what the client has to send, and writes it for a protocol with a write.
 * @return FALSE when the bytes have to wait for the port.
 */
static BOOL _send(PifDeviceStream *p_owner)
{
	PifDeviceStreamProtocol *p_protocol = p_owner->_p_protocol;
	int i;

	if (!p_owner->__evt_sending) return TRUE;

	// What was not sent yet goes first, so the bytes stay in order.
	if (!p_owner->__tx_len) {
		// A client may answer without writing while it moves on to its next message, so it is asked
		// again while it says it has more. Bounded, in case it never says it is done.
		for (i = 0; i < 16 && p_owner->__tx_len < p_owner->__tx_size; i++) {
			if (!(*p_owner->__evt_sending)(p_owner->__p_client, _actSend)) break;
		}
	}
	if (!p_owner->__tx_len || !p_protocol->__p_ops->write) return TRUE;

	// The port held by another device is tried again later.
	if (pifDeviceReg_IsBusy(p_owner->_p_device)) return FALSE;
	(*p_protocol->__p_ops->write)(p_protocol, p_owner);
	p_owner->__tx_len = 0;
	return TRUE;
}

static uint32_t _doTask(PifTask *p_task)
{
	PifDeviceStream *p_owner = p_task->_p_client;
	BOOL was_running = p_owner->__running;

	if (!_poll(p_owner)) return RECHECK_PERIOD1US;

	// The transfer a detached client left running is over, so the task can wait for the next one.
	if (!_isAttached(p_owner)) {
		p_task->pause = TRUE;
		return 0;
	}

	if (!_send(p_owner)) return RECHECK_PERIOD1US;
	if (!_parse(p_owner)) return RECHECK_PERIOD1US;

	// One transfer a run, so a long message does not hold the CPU, and one a period when the device has
	// nothing. A transfer that ended in this run is followed only as the protocol asked.
	if (!was_running) {
		if (_start(p_owner)) {
			if (!_poll(p_owner)) return RECHECK_PERIOD1US;
		}
		// Bytes to send wait for the port only briefly.
		else if (p_owner->__tx_len && pifDeviceReg_IsBusy(p_owner->_p_device)) {
			return RECHECK_PERIOD1US;
		}
		if (!_parse(p_owner)) return RECHECK_PERIOD1US;
	}
	if (p_owner->__more) pifTask_SetTrigger(p_task, 0);
	return 0;
}

static void _evtAttachClient(PifTransport *p_transport, void *p_client, PifEvtTransportParsing evt_parsing,
		PifEvtTransportSending evt_sending, PifEvtTransportAbortRx evt_abort_rx)
{
	PifDeviceStream *p_owner = (PifDeviceStream *)p_transport;

	p_owner->__rx_pos = p_owner->__rx_len = 0;
	p_owner->__tx_len = 0;
	p_owner->__p_client = p_client;
	p_owner->__evt_parsing = evt_parsing;
	p_owner->__evt_sending = evt_sending;
	p_owner->__evt_abort_rx = evt_abort_rx;
	p_owner->_p_task->pause = FALSE;
}

static void _evtDetachClient(PifTransport *p_transport)
{
	PifDeviceStream *p_owner = (PifDeviceStream *)p_transport;

	p_owner->__evt_parsing = NULL;
	p_owner->__evt_sending = NULL;
	p_owner->__evt_abort_rx = NULL;
	p_owner->__p_client = NULL;
	p_owner->__rx_pos = p_owner->__rx_len = 0;
	p_owner->__tx_len = 0;
	p_owner->__more = FALSE;
	// A transfer that is running keeps the task going until it is over.
	if (!p_owner->__running) p_owner->_p_task->pause = TRUE;
}

static void _evtTriggerSend(PifTransport *p_transport)
{
	pifTask_SetTrigger(((PifDeviceStream *)p_transport)->_p_task, 0);
}

// Bytes go out as they come, and how long one takes depends on the port, so only these are given.
static const PifTransportOps c_transport_ops = {
	_evtAttachClient,
	_evtDetachClient,
	_evtTriggerSend,
	NULL,
	NULL,
	NULL
};


void pifDeviceStreamProtocol_Init(PifDeviceStreamProtocol *p_owner, const PifDeviceStreamProtocolOps *p_ops)
{
	p_owner->__p_ops = p_ops;
}

BOOL pifDeviceStream_Init(PifDeviceStream *p_owner, PifId id, PifDeviceReg *p_device,
		PifDeviceStreamProtocol *p_protocol, uint16_t rx_size, uint16_t tx_size, uint16_t period1ms, const char *name)
{
	if (!p_owner || !p_device || !_isValidProtocol(p_protocol) || !rx_size || !period1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifDeviceStream));

	p_owner->__p_rx = calloc(sizeof(uint8_t), (size_t)rx_size + tx_size);
	if (!p_owner->__p_rx) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
	p_owner->__p_tx = p_owner->__p_rx + rx_size;
	p_owner->__rx_size = rx_size;
	p_owner->__tx_size = tx_size;

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1ms * 1000UL, _doTask, p_owner, FALSE);
	if (!p_owner->_p_task) {
		free(p_owner->__p_rx);
		memset(p_owner, 0, sizeof(PifDeviceStream));
		return FALSE;
	}
	p_owner->_p_task->name = name ? name : "DevStream";

	pifTransport_Init(&p_owner->parent, &c_transport_ops);
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_p_device = p_device;
	p_owner->_p_protocol = p_protocol;
	if (p_protocol->__p_ops->reset) (*p_protocol->__p_ops->reset)(p_protocol);
	return TRUE;
}

void pifDeviceStream_Clear(PifDeviceStream *p_owner)
{
	if (!p_owner->_p_task) return;

	_evtDetachClient(&p_owner->parent);
	// The device cannot leave the port while its transfer holds it. The transfer ends by itself at the timeout.
	while (!_poll(p_owner));
	pifTaskManager_Remove(p_owner->_p_task);
	free(p_owner->__p_rx);
	memset(p_owner, 0, sizeof(PifDeviceStream));
}

BOOL pifDeviceStream_SetProtocol(PifDeviceStream *p_owner, PifDeviceStreamProtocol *p_protocol)
{
	if (!_isValidProtocol(p_protocol)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	// The transfer belongs to the protocol that started it.
	if (p_owner->__running) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__rx_pos = p_owner->__rx_len = 0;
	p_owner->__tx_len = 0;
	p_owner->__more = FALSE;
	p_owner->_p_protocol = p_protocol;
	if (p_protocol->__p_ops->reset) (*p_protocol->__p_ops->reset)(p_protocol);
	return TRUE;
}

void pifDeviceStream_SetRx(PifDeviceStream *p_owner, uint16_t size)
{
	p_owner->__rx_pos = 0;
	p_owner->__rx_len = size;
}

void pifDeviceStream_TakeTx(PifDeviceStream *p_owner, uint16_t size)
{
	if (size >= p_owner->__tx_len) {
		p_owner->__tx_len = 0;
		return;
	}
	p_owner->__tx_len -= size;
	memmove(p_owner->__p_tx, p_owner->__p_tx + size, p_owner->__tx_len);
}

void pifDeviceStream_AbortRx(PifDeviceStream *p_owner)
{
	p_owner->__rx_pos = p_owner->__rx_len = 0;
	if (p_owner->__evt_abort_rx) (*p_owner->__evt_abort_rx)(p_owner->__p_client);
}
