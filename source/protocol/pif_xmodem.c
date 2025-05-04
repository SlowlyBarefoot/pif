#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_ring_buffer.h"
#include "protocol/pif_xmodem.h"


// 한 packet을 보내고 응답을 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 500이고 타이머 단위가 1ms이기에 500 * 1ms = 500ms이다.
#define PIF_XMODEM_RESPONSE_TIMEOUT		500

// 한 packet을 전부 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 300이고 타이머 단위가 1ms이기에 300 * 1ms = 300ms이다.
#define PIF_XMODEM_RECEIVE_TIMEOUT		300


static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	PifXmodem* p_owner = (PifXmodem*)p_issuer;

	switch (p_owner->__tx.state) {
	default:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "XM(%u) RxTimeout State:%u Cnt:%u", p_owner->_id,
				p_owner->__rx.state, p_owner->__rx.count);
#endif
		p_owner->__rx.state = XRS_IDLE;
		p_owner->__tx.state = XTS_NAK;
		pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
		break;
	}
}

static void _evtTimerTxTimeout(PifIssuerP p_issuer)
{
	PifXmodem* p_owner = (PifXmodem*)p_issuer;

	switch (p_owner->__tx.state) {
	case XTS_WAIT_RESPONSE:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u Count=%u", p_owner->_id,
				p_owner->__tx.state, p_owner->__rx.count);
#endif
		p_owner->__tx.state = XTS_IDLE;
		if (p_owner->__tx.evt_receive) {
			(*p_owner->__tx.evt_receive)(ASCII_NAK, p_owner->__p_data[1]);
		}
		break;

	default:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u", p_owner->_id, p_owner->__tx.state);
#endif
		break;
	}
}

#ifndef PIF_NO_LOG

#define PKT_ERR_INVALID_PACKET_NO	0
#define PKT_ERR_WRONG_CRC    		1

static const char *c_cPktErr[] = {
		"Invalid Packet No",
		"Wrong CRC"
};

#endif

static BOOL _parsingPacket(PifXmodem* p_owner, PifActUartReceiveData act_receive_data)
{
	PifXmodemPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
	BOOL rtn = FALSE;
	uint16_t crc;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx.state)	{
		case XRS_IDLE:
			switch (data) {
			case 'C':
				p_owner->__rx.state = XRS_C;
				break;

			case ASCII_SOH:
				p_owner->__tx.state = XTS_IDLE;

				p_owner->__rx.count = 1;
				p_owner->__rx.state = XRS_GET_HEADER;
				if (!pifTimer_Start(p_owner->__rx.p_timer, p_owner->__rx.timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_WARN, "XM(%u) Not start timer", p_owner->_id);
#endif
				}
				rtn = TRUE;
				break;

			case ASCII_EOT:
				p_owner->__rx.state = XRS_EOT;

				p_owner->__tx.state = XTS_ACK;
				rtn = TRUE;

				p_owner->__finish = TRUE;
				break;

			case ASCII_CAN:
				p_owner->__rx.state = XRS_CAN;
				break;
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
					pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", p_owner->_id,
							c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)p_packet->packet_no[0],
							(unsigned int)p_packet->packet_no[1]);
#endif
					goto fail;
				}
			}
			break;

		case XRS_GET_DATA:
			p_packet->p_data[p_owner->__rx.count] = data;
			p_owner->__rx.count++;
			if (p_owner->__rx.count >= 128) {
				p_owner->__rx.state = XRS_GET_CRC;
				p_owner->__rx.count = 0;
			}
			break;

		case XRS_GET_CRC:
			switch (p_owner->__type) {
			case XT_ORIGINAL:
				p_owner->__rx.crc = data;
				crc = pifCheckSum(p_packet->p_data, 128);
				if (p_owner->__rx.crc == crc) {
					pifTimer_Stop(p_owner->__rx.p_timer);
					p_owner->__rx.state = XRS_SOH;

					p_owner->__tx.state = XTS_ACK;
					return TRUE;
				}
				else {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", p_owner->_id,
							c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)p_owner->__rx.crc, (unsigned int)crc);
#endif
					goto fail;
				}
				break;

			case XT_CRC:
				if (!p_owner->__rx.count) {
					p_owner->__rx.count++;
					p_owner->__rx.crc = data << 8;
				}
				else {
					p_owner->__rx.crc |= data;
					crc = pifCrc16(p_packet->p_data, 128);
					if (p_owner->__rx.crc == crc) {
			            pifTimer_Stop(p_owner->__rx.p_timer);
			            p_owner->__rx.state = XRS_SOH;

			            p_owner->__tx.state = XTS_ACK;
						return TRUE;
					}
					else {
#ifndef PIF_NO_LOG
						pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(%s) %u!=%u", p_owner->_id,
								c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)p_owner->__rx.crc, (unsigned int)crc);
#endif
						goto fail;
					}
				}
				break;
			}
			break;

		default:
			break;
		}
	}
	return rtn ? TRUE : (p_owner->__rx.state > XRS_IDLE);

fail:
	pifTimer_Stop(p_owner->__rx.p_timer);
	p_owner->__rx.state = XRS_IDLE;

	p_owner->__tx.state = XTS_NAK;
	return TRUE;
}

static BOOL _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint8_t data;

	if (p_owner->__tx.state == XTS_WAIT_RESPONSE) {
		if ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
			switch (data) {
			case ASCII_ACK:
				pifTimer_Stop(p_owner->__tx.p_timer);
				p_owner->__tx.state = XTS_IDLE;
				if (p_owner->__finish) {
					if (p_owner->__evt_finish) (*p_owner->__evt_finish)(0);
				}
				else {
					if (p_owner->__tx.evt_receive) {
						(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
					}
				}
				break;

			case ASCII_NAK:
			case ASCII_CAN:
				pifTimer_Stop(p_owner->__tx.p_timer);
				p_owner->__tx.state = XTS_IDLE;
				if (p_owner->__tx.evt_receive) {
					(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
				}
				break;
			}
			return TRUE;
		}
	}
	else {
		if (_parsingPacket(p_owner, act_receive_data)) {
			pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
		}

		switch (p_owner->__rx.state) {
		case XRS_C:
			if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(p_owner->__rx.state, 0);
			}
			p_owner->__rx.state = XRS_IDLE;
			p_owner->__finish = FALSE;
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_NONE, "C");
#endif
			break;

		case XRS_EOT:
		case XRS_CAN:
			if (p_owner->__rx.evt_receive) {
				(*p_owner->__rx.evt_receive)(p_owner->__rx.state, NULL);
			}
			p_owner->__rx.state = XRS_IDLE;
			break;

		case XRS_SOH:
			if (p_owner->__rx.evt_receive) {
				(*p_owner->__rx.evt_receive)(p_owner->__rx.state, &p_owner->__rx.packet);
			}
			p_owner->__rx.state = XRS_IDLE;
			break;

		default:
			break;
		}
		return TRUE;
	}
	return FALSE;
}

static uint16_t _evtSending(void* p_client, PifActUartSendData act_send_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint16_t length = 0;
	uint8_t data;

	if (!p_owner->__p_uart->_fc_state) return 0;

	switch (p_owner->__tx.state) {
	case XTS_SEND_C:
		data = 'C';
		length = (*act_send_data)(p_owner->__p_uart, &data, 1);
		if (length) {
			p_owner->__finish = FALSE;
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_NONE, "C");
#endif
			length = 1000000 / p_owner->__p_uart->_transfer_time;	// 1000000us = 1sec
		}
		break;

	case XTS_SENDING:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__p_data + p_owner->__tx.data_pos,
    			p_owner->__packet_size - p_owner->__tx.data_pos);
		p_owner->__tx.data_pos += length;
		if (p_owner->__tx.data_pos >= p_owner->__packet_size) {
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		break;

	case XTS_EOT:
		length = (*act_send_data)(p_owner->__p_uart, (uint8_t *)&p_owner->__tx.state, 1);
		if (length) {
			p_owner->__finish = TRUE;
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		break;

	case XTS_CAN:
		length = (*act_send_data)(p_owner->__p_uart, (uint8_t *)&p_owner->__tx.state, 1);
		if (length) {
			if (p_owner->__tx.evt_receive) {
				p_owner->__tx.state = XTS_WAIT_RESPONSE;
			}
			else {
				p_owner->__tx.state = XTS_IDLE;
			}
		}
		break;

	case XTS_ACK:
		length = (*act_send_data)(p_owner->__p_uart, (uint8_t *)&p_owner->__tx.state, 1);
		if (length) {
			if (p_owner->__finish && p_owner->__evt_finish) {
				(*p_owner->__evt_finish)(length * p_owner->__p_uart->_transfer_time);
			}
			p_owner->__tx.state = XTS_IDLE;
		}
		break;

	case XTS_NAK:
		length = (*act_send_data)(p_owner->__p_uart, (uint8_t *)&p_owner->__tx.state, 1);
		if (length) p_owner->__tx.state = XTS_IDLE;
		break;

	default:
		break;
	}
	return length;
}

BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifTimerManager* p_timer_manager, PifXmodemType type)
{
    if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifXmodem));

    p_owner->__p_timer_manager = p_timer_manager;
    switch (type) {
    case XT_ORIGINAL:
    	p_owner->__packet_size = 3 + 128 + 1;
    	break;

    case XT_CRC:
    	p_owner->__packet_size = 3 + 128 + 2;
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

    p_owner->__tx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__tx.p_timer) goto fail;

    p_owner->__rx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;

    p_owner->__type = type;

    pifTimer_AttachEvtFinish(p_owner->__tx.p_timer, _evtTimerTxTimeout, p_owner);
    p_owner->__tx.timeout = PIF_XMODEM_RESPONSE_TIMEOUT;

    pifTimer_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
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
	if (p_owner->__p_data) {
		free(p_owner->__p_data);
		p_owner->__p_data = NULL;
	}
	if (p_owner->__rx.p_timer) {
		pifTimerManager_Remove(p_owner->__rx.p_timer);
		p_owner->__rx.p_timer = NULL;
	}
	if (p_owner->__tx.p_timer) {
		pifTimerManager_Remove(p_owner->__tx.p_timer);
		p_owner->__tx.p_timer = NULL;
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

void pifXmodem_AttachUart(PifXmodem* p_owner, PifUart* p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifXmodem_DetachUart(PifXmodem* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

void pifXmodem_AttachEvtTx(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive, PifEvtXmodemFinish evt_finish)
{
	p_owner->__tx.evt_receive = evt_tx_receive;
	p_owner->__evt_finish = evt_finish;
}

void pifXmodem_AttachEvtRx(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive, PifEvtXmodemFinish evt_finish)
{
	p_owner->__rx.evt_receive = evt_rx_receive;
	p_owner->__evt_finish = evt_finish;
}

BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size)
{
	uint16_t i, p, crc;

	if (!p_owner->__tx.evt_receive) {
		pif_error = E_NOT_SET_EVENT;
		return FALSE;
	}

	if (!data_size || data_size > 128) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_owner->__tx.state != XTS_IDLE) {
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
	if (data_size < 128) {
		for (i = 0; i < 128 - data_size; i++) {
			p_owner->__p_data[p++] = ASCII_SUB;
		}
	}
	switch (p_owner->__type) {
	case XT_ORIGINAL:
		crc = pifCheckSum(&p_owner->__p_data[3], 128);
		p_owner->__p_data[p++] = crc;
		break;

	case XT_CRC:
		crc = pifCrc16(&p_owner->__p_data[3], 128);
		p_owner->__p_data[p++] = crc >> 8;
		p_owner->__p_data[p++] = crc & 0xFF;
		break;
	}

	p_owner->__tx.data_pos = 0;
	p_owner->__tx.state = XTS_SENDING;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);

	if (!pifTimer_Start(p_owner->__tx.p_timer, p_owner->__tx.timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "XM(%u) Not start timer", p_owner->_id);
#endif
	}
	return TRUE;
}

void pifXmodem_SendEot(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_EOT;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
}

void pifXmodem_SendCancel(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_CAN;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
}

void pifXmodem_ReadyReceive(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_SEND_C;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
}
