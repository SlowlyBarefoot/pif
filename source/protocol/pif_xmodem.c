// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_ring_buffer.h"
#include "protocol/pif_xmodem.h"


// Time in ms to wait for ACK/NAK after sending one packet or EOT.
// The time to send the packet on the transport is added to it, as the packet may still be
// in the transmit buffer of the transport when the wait starts.
#ifndef PIF_XMODEM_RESPONSE_TIMEOUT
#define PIF_XMODEM_RESPONSE_TIMEOUT		500
#endif

// Time in ms allowed to receive one complete packet from the remote peer.
// The time to receive the packet on the transport is added to it.
#ifndef PIF_XMODEM_RECEIVE_TIMEOUT
#define PIF_XMODEM_RECEIVE_TIMEOUT		300
#endif

// Quiet time in ms that ends the purge after a bad packet. The NAK goes out only then, so that
// the rest of the bad packet is not read as SOH, EOT or CAN.
#ifndef PIF_XMODEM_PURGE_TIME
#define PIF_XMODEM_PURGE_TIME			100
#endif

// Interval in ms at which the receiver repeats the start request until the first packet comes.
#ifndef PIF_XMODEM_START_INTERVAL
#define PIF_XMODEM_START_INTERVAL		1000
#endif

// How many times EOT is sent again after a NAK or no answer before the transfer is given up.
#ifndef PIF_XMODEM_EOT_RETRY
#define PIF_XMODEM_EOT_RETRY			3
#endif

#define XMODEM_DATA_SIZE				128

static const uint8_t c_cancel[] = { ASCII_CAN, ASCII_CAN };


/**
 * @brief Triggers the task for when the nearer of the running timeouts expires.
 * @details A pending trigger is replaced, so the task runs only for the nearest one. A timeout
 *          that is stopped needs nothing here: the task finds nothing due and does nothing.
 * @param p_owner Pointer to the XMODEM protocol instance.
 */
static void _schedule(PifXmodem* p_owner)
{
	PifXmodemTimeout* p_waits[2] = { &p_owner->__tx.wait, &p_owner->__rx.wait };
	uint32_t now = (*pif_act_timer1us)();
	uint32_t elapsed, remain, delay = 0;
	BOOL running = FALSE;
	int i;

	for (i = 0; i < 2; i++) {
		if (!p_waits[i]->time) continue;
		elapsed = now - p_waits[i]->start;
		remain = elapsed >= p_waits[i]->time ? 0 : p_waits[i]->time - elapsed;
		if (!running || remain < delay) delay = remain;
		running = TRUE;
	}
	if (running) pifTask_SetTrigger(p_owner->__p_task, delay);
}

/**
 * @brief Starts or restarts a timeout.
 * @param p_owner Pointer to the XMODEM protocol instance.
 * @param p_wait Timeout to start.
 * @param timeout Time in ms.
 * @param bytes Number of bytes whose transfer time on the transport is added.
 */
static void _startTimeout(PifXmodem* p_owner, PifXmodemTimeout* p_wait, uint16_t timeout, uint16_t bytes)
{
	p_wait->start = (*pif_act_timer1us)();
	p_wait->time = timeout * 1000UL + (uint32_t)bytes * pifTransport_GetByteTime(p_owner->__p_transport);
	if (!p_wait->time) p_wait->time = 1;
	_schedule(p_owner);
}

/**
 * @brief Checks a timeout, and stops it when it has expired.
 * @param p_wait Timeout to check.
 * @return TRUE if it was running and has expired.
 */
static BOOL _isExpired(PifXmodemTimeout* p_wait)
{
	if (!p_wait->time || (*pif_act_timer1us)() - p_wait->start < p_wait->time) return FALSE;
	p_wait->time = 0;
	return TRUE;
}

/**
 * @brief Requests a control byte or packet transmission from the transport.
 * @param p_owner Pointer to the XMODEM protocol instance.
 * @param state Transmit state to enter.
 */
static void _setTxState(PifXmodem* p_owner, PifXmodemTxState state)
{
	p_owner->__tx.state = state;
	pifTransport_TriggerSend(p_owner->__p_transport);
}

/**
 * @brief Sends EOT again after a NAK or no answer, or gives the transfer up.
 * @param p_owner Pointer to the XMODEM protocol instance.
 */
static void _retryEot(PifXmodem* p_owner)
{
	if (p_owner->__tx.retry) {
		p_owner->__tx.retry--;
		_setTxState(p_owner, XTS_EOT);
	}
	else {
		p_owner->__tx.state = XTS_IDLE;
		p_owner->__finish = FALSE;
		if (p_owner->__tx.evt_receive) {
			(*p_owner->__tx.evt_receive)(ASCII_CAN, p_owner->__p_data[1]);
		}
	}
}

/**
 * @brief Handles the receive timeout: a stalled packet or the end of a purge leads to a NAK.
 * @param p_owner Pointer to the XMODEM protocol instance.
 */
static void _rxTimeout(PifXmodem* p_owner)
{
	switch (p_owner->__rx.state) {
	case XRS_GET_HEADER:
	case XRS_GET_DATA:
	case XRS_GET_CRC:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "XM(%u) RxTimeout State:%u Cnt:%u", p_owner->_id,
				p_owner->__rx.state, p_owner->__rx.count);
#endif
		// fall through
	case XRS_PURGE:
		p_owner->__rx.state = XRS_IDLE;
		_setTxState(p_owner, XTS_NAK);
		break;

	default:
		break;
	}
}

/**
 * @brief Handles the transmit timeout: repeats the start request, or handles a missing answer.
 * @param p_owner Pointer to the XMODEM protocol instance.
 */
static void _txTimeout(PifXmodem* p_owner)
{
	switch (p_owner->__tx.state) {
	case XTS_WAIT_START:
		_setTxState(p_owner, XTS_SEND_C);
		break;

	case XTS_WAIT_RESPONSE:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u", p_owner->_id, p_owner->__tx.state);
#endif
		if (p_owner->__finish) {
			_retryEot(p_owner);
		}
		else {
			p_owner->__tx.state = XTS_IDLE;
			if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(ASCII_NAK, p_owner->__p_data[1]);
			}
		}
		break;

	default:
		break;
	}
}

/**
 * @brief Protocol task: handles the timeouts that have expired and waits for the next one.
 * @param p_task Task that resolves to the owning XMODEM instance.
 * @return Always 0: the task runs only when triggered.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifXmodem* p_owner = (PifXmodem*)p_task->_p_client;

	if (!p_owner->__p_transport) return 0;

	if (_isExpired(&p_owner->__tx.wait)) _txTimeout(p_owner);
	if (_isExpired(&p_owner->__rx.wait)) _rxTimeout(p_owner);
	_schedule(p_owner);
	return 0;
}

/**
 * @brief Consumes received bytes and parses one XMODEM packet according to the current RX state.
 * @details Returns as soon as an event state (XRS_C, XRS_EOT, XRS_CAN, XRS_SOH) is reached, so the
 *          caller handles it before the next byte is read.
 * @param p_owner Pointer to the XMODEM protocol instance.
 * @param act_receive_data Callback used to fetch received bytes.
 * @return TRUE when packet processing produced immediate transmit work; otherwise FALSE.
 */
static BOOL _parsingPacket(PifXmodem* p_owner, PifActTransportReceive act_receive_data)
{
	PifXmodemPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data, packet_no;
	BOOL rtn = FALSE;
	uint16_t crc;

	while ((*act_receive_data)(p_owner->__p_transport, &data, 1)) {
		switch (p_owner->__rx.state)	{
		case XRS_IDLE:
			if (data == ASCII_SOH) {
				_startTimeout(p_owner, &p_owner->__rx.wait, p_owner->__rx.timeout, p_owner->__packet_size);
				if (p_owner->__tx.state == XTS_SEND_C || p_owner->__tx.state == XTS_WAIT_START) {
					// The first packet answers the start request.
					p_owner->__tx.wait.time = 0;
					p_owner->__tx.state = XTS_IDLE;
				}
				p_owner->__rx.count = 1;
				p_owner->__rx.state = XRS_GET_HEADER;
				rtn = TRUE;
			}
			else if (data == (p_owner->__type == XT_CRC ? 'C' : ASCII_NAK)) {
				p_owner->__rx.state = XRS_C;
				return TRUE;
			}
			else if (data == ASCII_EOT) {
				p_owner->__rx.state = XRS_EOT;
				p_owner->__rx.last_packet_no = 0;
				p_owner->__finish = TRUE;
				p_owner->__tx.state = XTS_ACK;
				return TRUE;
			}
			else if (data == ASCII_CAN) {
				p_owner->__rx.state = XRS_CAN;
				p_owner->__rx.last_packet_no = 0;
				return TRUE;
			}
			break;

		case XRS_GET_HEADER:
			p_packet->packet_no[p_owner->__rx.count - 1] = data;
			p_owner->__rx.count++;
			if (p_owner->__rx.count >= 3) {
				if (p_packet->packet_no[0] + p_packet->packet_no[1] == 0xFF) {
					p_owner->__rx.state = XRS_GET_DATA;
					p_packet->p_data = p_owner->__p_data;
					p_owner->__rx.count = 0;
				}
				else {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(" PIF_LOG_MSG_INVALID_PACKET_NO ") %u!=%u", p_owner->_id,
							(unsigned int)p_packet->packet_no[0], (unsigned int)p_packet->packet_no[1]);
#endif
					goto fail;
				}
			}
			break;

		case XRS_GET_DATA:
			p_packet->p_data[p_owner->__rx.count] = data;
			p_owner->__rx.count++;
			if (p_owner->__rx.count >= XMODEM_DATA_SIZE) {
				p_owner->__rx.state = XRS_GET_CRC;
				p_owner->__rx.count = 0;
			}
			break;

		case XRS_GET_CRC:
			if (p_owner->__type == XT_ORIGINAL) {
				p_owner->__rx.crc = data;
				crc = (uint8_t)pifCheckSum(p_packet->p_data, XMODEM_DATA_SIZE);
			}
			else if (!p_owner->__rx.count) {
				p_owner->__rx.count++;
				p_owner->__rx.crc = data << 8;
				break;
			}
			else {
				p_owner->__rx.crc |= data;
				crc = pifCrc16(p_packet->p_data, XMODEM_DATA_SIZE);
			}
			if (p_owner->__rx.crc != crc) {
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(" PIF_LOG_MSG_WRONG_CRC ") %u!=%u", p_owner->_id,
						(unsigned int)p_owner->__rx.crc, (unsigned int)crc);
#endif
				goto fail;
			}

			p_owner->__rx.wait.time = 0;
			packet_no = p_packet->packet_no[0];
			if (packet_no == p_owner->__rx.last_packet_no) {
				// Sent again because our ACK was lost: acknowledged, but not passed on twice.
				p_owner->__rx.state = XRS_IDLE;
				p_owner->__tx.state = XTS_ACK;
			}
			else if (packet_no == (uint8_t)(p_owner->__rx.last_packet_no + 1)) {
				p_owner->__rx.last_packet_no = packet_no;
				p_owner->__rx.state = XRS_SOH;
				p_owner->__tx.state = XTS_ACK;
			}
			else {
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(" PIF_LOG_MSG_OUT_OF_SEQUENCE ") %u!=%u", p_owner->_id,
						(unsigned int)packet_no, (unsigned int)(uint8_t)(p_owner->__rx.last_packet_no + 1));
#endif
				// A packet lost for good cannot be recovered, so the transfer is cancelled.
				p_owner->__rx.state = XRS_CAN;
				p_owner->__rx.last_packet_no = 0;
				p_owner->__tx.data_pos = 0;
				p_owner->__tx.state = XTS_CAN;
			}
			return TRUE;

		case XRS_PURGE:
			// Every byte pushes the end of the purge back.
			_startTimeout(p_owner, &p_owner->__rx.wait, PIF_XMODEM_PURGE_TIME, 0);
			break;

		default:
			break;
		}
	}
	return rtn ? TRUE : (p_owner->__rx.state > XRS_IDLE);

fail:
	// The rest of the packet is still coming: the NAK goes out once the line is quiet.
	p_owner->__rx.state = XRS_PURGE;
	_startTimeout(p_owner, &p_owner->__rx.wait, PIF_XMODEM_PURGE_TIME, 0);
	return TRUE;
}

/**
 * @brief Receive callback that dispatches ACK/NAK handling and packet parsing.
 * @param p_client Opaque client pointer resolved to the owning XMODEM instance.
 * @param act_receive_data Callback used to fetch received bytes.
 * @return TRUE when input bytes were consumed or a state transition occurred; otherwise FALSE.
 */
static BOOL _evtParsing(void* p_client, PifActTransportReceive act_receive_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint8_t data, code;
    BOOL ret;

	if (p_owner->__tx.state == XTS_WAIT_RESPONSE) {
		if (!(*act_receive_data)(p_owner->__p_transport, &data, 1)) return FALSE;

		switch (data) {
		case ASCII_ACK:
			p_owner->__tx.wait.time = 0;
			p_owner->__tx.state = XTS_IDLE;
			if (p_owner->__finish) {
				p_owner->__finish = FALSE;
				if (p_owner->__tx.evt_finish) (*p_owner->__tx.evt_finish)(0);
			}
			else if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
			}
			break;

		case ASCII_NAK:
			p_owner->__tx.wait.time = 0;
			if (p_owner->__finish) {
				// Some receivers NAK the first EOT: it is sent again, not the last packet.
				_retryEot(p_owner);
			}
			else {
				p_owner->__tx.state = XTS_IDLE;
				if (p_owner->__tx.evt_receive) {
					(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
				}
			}
			break;

		case ASCII_CAN:
			p_owner->__tx.wait.time = 0;
			p_owner->__tx.state = XTS_IDLE;
			p_owner->__finish = FALSE;
			if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
			}
			break;
		}
		return TRUE;
	}

	ret = _parsingPacket(p_owner, act_receive_data);
	if (ret) {
		pifTransport_TriggerSend(p_owner->__p_transport);
	}

	// The state is cleared before the event, so that the event can start the next step.
	code = p_owner->__rx.state;
	switch (code) {
	case XRS_C:
		p_owner->__rx.state = XRS_IDLE;
		// The receiver repeats the request until the first packet comes, so one arriving while a
		// packet is being sent is not a new start.
		if (p_owner->__tx.state == XTS_IDLE) {
			p_owner->__finish = FALSE;
			if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(code, 0);
			}
		}
		break;

	case XRS_EOT:
	case XRS_CAN:
		p_owner->__rx.state = XRS_IDLE;
		if (p_owner->__rx.evt_receive) {
			(*p_owner->__rx.evt_receive)(code, NULL);
		}
		break;

	case XRS_SOH:
		p_owner->__rx.state = XRS_IDLE;
		if (p_owner->__rx.evt_receive) {
			(*p_owner->__rx.evt_receive)(code, &p_owner->__rx.packet);
		}
		break;

	default:
		break;
	}
	return ret;
}

/**
 * @brief Sends one control byte.
 * @param p_owner Pointer to the XMODEM protocol instance.
 * @param act_send_data Callback used to transmit bytes.
 * @param code Control byte to send.
 * @return Number of bytes transmitted.
 */
static uint16_t _sendControl(PifXmodem* p_owner, PifActTransportSend act_send_data, uint8_t code)
{
	// Kept in the instance rather than on the stack, in case the transport sends from the pointer later.
	p_owner->__tx.ctrl = code;
	return (*act_send_data)(p_owner->__p_transport, &p_owner->__tx.ctrl, 1);
}

/**
 * @brief Transmit callback that emits control bytes or packet payload data based on TX state.
 * @param p_client Opaque client pointer resolved to the owning XMODEM instance.
 * @param act_send_data Callback used to transmit bytes.
 * @return Number of bytes transmitted during this callback invocation.
 */
static uint16_t _evtSending(void* p_client, PifActTransportSend act_send_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint16_t length = 0;

	if (!pifTransport_GetFlowState(p_owner->__p_transport)) return 0;

	switch (p_owner->__tx.state) {
	case XTS_SEND_C:
		length = _sendControl(p_owner, act_send_data, p_owner->__type == XT_CRC ? 'C' : ASCII_NAK);
		if (length) {
			p_owner->__finish = FALSE;
			_startTimeout(p_owner, &p_owner->__tx.wait, PIF_XMODEM_START_INTERVAL, 0);
			p_owner->__tx.state = XTS_WAIT_START;
		}
		break;

	case XTS_SENDING:
    	length = (*act_send_data)(p_owner->__p_transport, p_owner->__p_data + p_owner->__tx.data_pos,
    			p_owner->__packet_size - p_owner->__tx.data_pos);
		p_owner->__tx.data_pos += length;
		if (p_owner->__tx.data_pos >= p_owner->__packet_size) {
			_startTimeout(p_owner, &p_owner->__tx.wait, p_owner->__tx.timeout, p_owner->__packet_size);
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		break;

	case XTS_EOT:
		length = _sendControl(p_owner, act_send_data, ASCII_EOT);
		if (length) {
			p_owner->__finish = TRUE;
			_startTimeout(p_owner, &p_owner->__tx.wait, p_owner->__tx.timeout, 1);
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		break;

	case XTS_CAN:
		// No answer comes to a cancel, so there is nothing to wait for.
		length = (*act_send_data)(p_owner->__p_transport, (uint8_t *)c_cancel + p_owner->__tx.data_pos,
				sizeof(c_cancel) - p_owner->__tx.data_pos);
		p_owner->__tx.data_pos += length;
		if (p_owner->__tx.data_pos >= sizeof(c_cancel)) p_owner->__tx.state = XTS_IDLE;
		break;

	case XTS_ACK:
		length = _sendControl(p_owner, act_send_data, ASCII_ACK);
		if (length) {
			p_owner->__tx.state = XTS_IDLE;
			if (p_owner->__finish) {
				p_owner->__finish = FALSE;
				if (p_owner->__rx.evt_finish) {
					(*p_owner->__rx.evt_finish)(length * pifTransport_GetByteTime(p_owner->__p_transport));
				}
			}
		}
		break;

	case XTS_NAK:
		length = _sendControl(p_owner, act_send_data, ASCII_NAK);
		if (length) p_owner->__tx.state = XTS_IDLE;
		break;

	default:
		break;
	}
	return length;
}


BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifXmodemType type)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifXmodem));

    switch (type) {
    case XT_ORIGINAL:
    	p_owner->__packet_size = 3 + XMODEM_DATA_SIZE + 1;
    	break;

    case XT_CRC:
    	p_owner->__packet_size = 3 + XMODEM_DATA_SIZE + 2;
    	break;

    default:
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    p_owner->__p_data = calloc(sizeof(uint8_t), p_owner->__packet_size);
    if (!p_owner->__p_data) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    // Runs only when a timeout is due, through pifTask_SetTrigger().
    p_owner->__p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, p_owner, FALSE);
    if (!p_owner->__p_task) goto fail;
    p_owner->__p_task->name = "Xmodem";

    p_owner->__type = type;
    p_owner->__tx.timeout = PIF_XMODEM_RESPONSE_TIMEOUT;
    p_owner->__rx.timeout = PIF_XMODEM_RECEIVE_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifXmodem_Clear(p_owner);
	return FALSE;
}

void pifXmodem_Clear(PifXmodem* p_owner)
{
	if (!p_owner) return;

	// Detached first, so that no transport callback reaches the buffer after it is freed.
	pifXmodem_DetachTransport(p_owner);

	if (p_owner->__p_data) {
		free(p_owner->__p_data);
		p_owner->__p_data = NULL;
	}
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
}

void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout)
{
	p_owner->__tx.timeout = response_timeout;
}

void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout)
{
	p_owner->__rx.timeout = receive_timeout;
}

BOOL pifXmodem_AttachTransport(PifXmodem *p_owner, PifTransport *p_transport)
{
	if (!p_owner || !p_transport) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_transport = p_transport;
	pifTransport_AttachClient(p_transport, p_owner, _evtParsing, _evtSending, NULL);
	return TRUE;
}

void pifXmodem_DetachTransport(PifXmodem *p_owner)
{
	if (!p_owner || !p_owner->__p_transport) return;

	pifTransport_DetachClient(p_owner->__p_transport);
	p_owner->__p_transport = NULL;

	// A trigger still pending finds no transport and does nothing.
	p_owner->__tx.wait.time = 0;
	p_owner->__rx.wait.time = 0;
	p_owner->__tx.state = XTS_IDLE;
	p_owner->__rx.state = XRS_IDLE;
	p_owner->__finish = FALSE;
}

void pifXmodem_AttachEvtTx(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive, PifEvtXmodemFinish evt_finish)
{
	p_owner->__tx.evt_receive = evt_tx_receive;
	p_owner->__tx.evt_finish = evt_finish;
}

void pifXmodem_AttachEvtRx(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive, PifEvtXmodemFinish evt_finish)
{
	p_owner->__rx.evt_receive = evt_rx_receive;
	p_owner->__rx.evt_finish = evt_finish;
}

BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size)
{
	uint16_t i, p, crc;

	if (!p_owner || !p_data || !data_size || data_size > XMODEM_DATA_SIZE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->__tx.evt_receive) {
		pif_error = E_NOT_SET_EVENT;
		return FALSE;
	}

	if (!p_owner->__p_transport || p_owner->__tx.state != XTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_data[0] = ASCII_SOH;
	p_owner->__p_data[1] = packet_no;
	p_owner->__p_data[2] = 0xFF - packet_no;
	p = 3;
	for (i = 0; i < data_size; i++) {
		p_owner->__p_data[p++] = p_data[i];
	}
	for (; i < XMODEM_DATA_SIZE; i++) {
		p_owner->__p_data[p++] = ASCII_SUB;
	}
	switch (p_owner->__type) {
	case XT_ORIGINAL:
		crc = pifCheckSum(&p_owner->__p_data[3], XMODEM_DATA_SIZE);
		p_owner->__p_data[p++] = crc;
		break;

	case XT_CRC:
		crc = pifCrc16(&p_owner->__p_data[3], XMODEM_DATA_SIZE);
		p_owner->__p_data[p++] = crc >> 8;
		p_owner->__p_data[p++] = crc & 0xFF;
		break;
	}

	p_owner->__tx.data_pos = 0;
	_setTxState(p_owner, XTS_SENDING);
	return TRUE;
}

BOOL pifXmodem_SendEot(PifXmodem* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->__p_transport || p_owner->__tx.state != XTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__tx.retry = PIF_XMODEM_EOT_RETRY;
	_setTxState(p_owner, XTS_EOT);
	return TRUE;
}

BOOL pifXmodem_SendCancel(PifXmodem* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->__p_transport) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__tx.wait.time = 0;
	p_owner->__rx.wait.time = 0;
	p_owner->__rx.state = XRS_IDLE;
	p_owner->__rx.last_packet_no = 0;
	p_owner->__finish = FALSE;
	p_owner->__tx.data_pos = 0;
	_setTxState(p_owner, XTS_CAN);
	return TRUE;
}

BOOL pifXmodem_ReadyReceive(PifXmodem* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->__p_transport) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__rx.state = XRS_IDLE;
	p_owner->__rx.last_packet_no = 0;
	p_owner->__finish = FALSE;
	_setTxState(p_owner, XTS_SEND_C);
	return TRUE;
}
