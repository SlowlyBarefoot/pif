// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_can_stream.h"

#include <stddef.h>


// The stream that embeds a PifCanClient.
#define STREAM_OF(P_CLIENT)		((PifCanStream *)((uint8_t *)(P_CLIENT) - offsetof(PifCanStream, __can_client)))


static BOOL _isValidFrameSize(uint8_t frame_size, uint8_t flags)
{
	if (!frame_size || frame_size > PIF_CAN_MAX_DATA_SIZE) return FALSE;
	if (frame_size <= 8) return TRUE;
	if (!(flags & CAN_F_FD)) return FALSE;
	return pifCan_DlcToLength(pifCan_LengthToDlc(frame_size)) == frame_size;
}

// Largest length a frame can have that is not above length.
static uint8_t _floorLength(uint8_t length)
{
	uint8_t dlc = pifCan_LengthToDlc(length);

	if (pifCan_DlcToLength(dlc) > length) dlc--;
	return pifCan_DlcToLength(dlc);
}

static uint16_t _actReceive(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
	PifCanStream *p_owner = (PifCanStream *)p_transport;

	return pifRingBuffer_GetBytes(&p_owner->_rx_buffer, p_data, size);
}

/**
 * @brief Queues the first length bytes of the pending frame and keeps the rest pending.
 * @return FALSE when the send queue is full, with nothing changed.
 */
static BOOL _queueFrame(PifCanStream *p_owner, uint8_t length)
{
	PifCanFrame *p_frame = &p_owner->__tx_frame;
	uint8_t remain = p_frame->length - length;

	if (!pifCan_GetTxRemain(p_owner->_p_can)) return FALSE;

	p_frame->length = length;
	if (!pifCan_SendFrame(p_owner->_p_can, p_frame)) {
		p_frame->length = length + remain;
		return FALSE;
	}
	if (remain) memmove(p_frame->data, p_frame->data + length, remain);
	p_frame->length = remain;
	return TRUE;
}

static uint16_t _actSend(PifTransport *p_transport, uint8_t *p_data, uint16_t size)
{
	PifCanStream *p_owner = (PifCanStream *)p_transport;
	PifCanFrame *p_frame = &p_owner->__tx_frame;
	uint16_t sent = 0, chunk;

	while (sent < size) {
		if (p_frame->length >= p_owner->_frame_size && !_queueFrame(p_owner, p_owner->_frame_size)) break;

		chunk = p_owner->_frame_size - p_frame->length;
		if (chunk > size - sent) chunk = size - sent;
		memcpy(p_frame->data + p_frame->length, p_data + sent, chunk);
		p_frame->length += chunk;
		sent += chunk;
	}
	p_owner->__tx_written += sent;
	return sent;
}

// Queues what is pending, split into lengths a CAN FD frame can have, until the send queue is full.
static void _flush(PifCanStream *p_owner)
{
	while (p_owner->__tx_frame.length) {
		if (!_queueFrame(p_owner, _floorLength(p_owner->__tx_frame.length))) break;
	}
}

// Calls evt_parsing while it makes progress. Returns TRUE when the receive buffer has been emptied.
static BOOL _parse(PifCanStream *p_owner)
{
	int i;

	if (!p_owner->__evt_parsing) return TRUE;

	// Bounded, in case a client reports progress without reading anything.
	for (i = 0; i < 16 && !pifRingBuffer_IsEmpty(&p_owner->_rx_buffer); i++) {
		if (!(*p_owner->__evt_parsing)(p_owner->__p_client, _actReceive)) break;
	}
	return pifRingBuffer_IsEmpty(&p_owner->_rx_buffer);
}

static BOOL _evtCanReceive(PifCanClient *p_client, const PifCanFrame *p_frame)
{
	PifCanStream *p_owner = STREAM_OF(p_client);

	if ((p_frame->flags & CAN_F_RTR) || p_frame->id != p_owner->_rx_id ||
			((p_owner->_flags ^ p_frame->flags) & CAN_F_EXTENDED)) return FALSE;

	// Without a client nobody would read the bytes, so they are not kept.
	if (!p_owner->__evt_parsing || !p_frame->length) return TRUE;

	if (pifRingBuffer_GetRemainSize(&p_owner->_rx_buffer) < p_frame->length) _parse(p_owner);
	if (!pifRingBuffer_PutData(&p_owner->_rx_buffer, (uint8_t *)p_frame->data, p_frame->length)) {
		p_owner->_rx_lost += p_frame->length;
	}
	return TRUE;
}

static uint32_t _evtCanRxDone(PifCanClient *p_client)
{
	PifCanStream *p_owner = STREAM_OF(p_client);

	if (_parse(p_owner)) return 0;
	// The client left bytes, so it is called again after about one frame.
	return pifCan_GetFrameTime(p_owner->_p_can, p_owner->_flags, p_owner->_frame_size);
}

static uint32_t _evtCanSending(PifCanClient *p_client)
{
	PifCanStream *p_owner = STREAM_OF(p_client);

	if (!p_owner->__evt_sending) return 0;

	// What did not fit last time goes first, so the bytes stay in order.
	_flush(p_owner);
	if (p_owner->__tx_frame.length || !pifCan_GetTxRemain(p_owner->_p_can)) return 0;

	// What was written is counted rather than taken from the return value, which some clients leave 0.
	p_owner->__tx_written = 0;
	(*p_owner->__evt_sending)(p_owner->__p_client, _actSend);
	_flush(p_owner);
	// A client that wrote something may have more, so it is asked again soon.
	return p_owner->__tx_written ? 1 : 0;
}

static void _evtAttachClient(PifTransport *p_transport, void *p_client, PifEvtTransportParsing evt_parsing,
		PifEvtTransportSending evt_sending)
{
	PifCanStream *p_owner = (PifCanStream *)p_transport;

	pifRingBuffer_Empty(&p_owner->_rx_buffer);
	p_owner->__tx_frame.length = 0;
	p_owner->__p_client = p_client;
	p_owner->__evt_parsing = evt_parsing;
	p_owner->__evt_sending = evt_sending;
	pifCan_TriggerTx(p_owner->_p_can);
}

static void _evtDetachClient(PifTransport *p_transport)
{
	PifCanStream *p_owner = (PifCanStream *)p_transport;

	p_owner->__evt_parsing = NULL;
	p_owner->__evt_sending = NULL;
	p_owner->__p_client = NULL;
	pifRingBuffer_Empty(&p_owner->_rx_buffer);
	p_owner->__tx_frame.length = 0;
}

static void _evtTriggerSend(PifTransport *p_transport)
{
	pifCan_TriggerTx(((PifCanStream *)p_transport)->_p_can);
}

static SWITCH _evtGetFlowState(PifTransport *p_transport)
{
	return ((PifCanStream *)p_transport)->_p_can->_bus_state != CBS_BUS_OFF;
}

static uint16_t _evtGetByteTime(PifTransport *p_transport)
{
	PifCanStream *p_owner = (PifCanStream *)p_transport;
	uint32_t time;

	time = pifCan_GetFrameTime(p_owner->_p_can, p_owner->_flags, p_owner->_frame_size);
	time = (time + p_owner->_frame_size - 1) / p_owner->_frame_size;
	return time > UINT16_MAX ? UINT16_MAX : time;
}

// The stream sends bytes as they come, so get_frame_size is left at its default of 1.
static const PifTransportOps c_transport_ops = {
	_evtAttachClient,
	_evtDetachClient,
	_evtTriggerSend,
	_evtGetFlowState,
	NULL,
	_evtGetByteTime
};


BOOL pifCanStream_Init(PifCanStream *p_owner, PifId id, PifCan *p_can, uint32_t tx_id, uint32_t rx_id,
		uint8_t flags, uint8_t frame_size, uint16_t rx_size)
{
	uint32_t mask;

	if (!p_owner || !p_can || !rx_size || rx_size == UINT16_MAX) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	mask = (flags & CAN_F_EXTENDED) ? PIF_CAN_EXT_ID_MASK : PIF_CAN_STD_ID_MASK;
	if ((flags & ~(CAN_F_EXTENDED | CAN_F_FD | CAN_F_BRS)) || ((flags & CAN_F_BRS) && !(flags & CAN_F_FD)) ||
			tx_id > mask || rx_id > mask || !_isValidFrameSize(frame_size, flags)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifCanStream));

	// The ring buffer keeps one byte free, so one more holds rx_size bytes.
	if (!pifRingBuffer_InitHeap(&p_owner->_rx_buffer, PIF_ID_AUTO, rx_size + 1)) return FALSE;
	pifRingBuffer_SetName(&p_owner->_rx_buffer, "CRB");

	pifTransport_Init(&p_owner->parent, &c_transport_ops);
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_p_can = p_can;
	p_owner->_tx_id = tx_id;
	p_owner->_rx_id = rx_id;
	p_owner->_flags = flags;
	p_owner->_frame_size = frame_size;
	p_owner->__tx_frame.id = tx_id;
	p_owner->__tx_frame.flags = flags;

	p_owner->__can_client.evt_receive = _evtCanReceive;
	p_owner->__can_client.evt_rx_done = _evtCanRxDone;
	p_owner->__can_client.evt_sending = _evtCanSending;
	pifCan_AddClient(p_can, &p_owner->__can_client);
	return TRUE;
}

void pifCanStream_Clear(PifCanStream *p_owner)
{
	if (!p_owner->_p_can) return;

	_evtDetachClient(&p_owner->parent);
	pifCan_RemoveClient(p_owner->_p_can, &p_owner->__can_client);
	pifRingBuffer_Clear(&p_owner->_rx_buffer);
	p_owner->_p_can = NULL;
}
