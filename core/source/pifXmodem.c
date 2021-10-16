#include "pifLog.h"
#include "pifRingBuffer.h"
#include "pifXmodem.h"


// 한 packet을 보내고 응답을 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 500이고 타이머 단위가 1ms이기에 500 * 1ms = 500ms이다.
#define PIF_XMODEM_RESPONSE_TIMEOUT		500

// 한 packet을 전부 받는 시간 제한
// pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 300이고 타이머 단위가 1ms이기에 300 * 1ms = 300ms이다.
#define PIF_XMODEM_RECEIVE_TIMEOUT		300


static void _evtTimerRxTimeout(void* p_issuer)
{
	PifXmodem* p_owner = (PifXmodem*)p_issuer;

	switch (p_owner->__tx.state) {
	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_ERROR, "XM(%u) ParsingPacket(Timeout) State:%u Cnt:%u", p_owner->_id,
				p_owner->__rx.state, p_owner->__rx.count);
#endif
		p_owner->__tx.state = XTS_NAK;
		p_owner->__rx.state = XRS_IDLE;
		break;
	}
}

static void _evtTimerTxTimeout(void* p_issuer)
{
	PifXmodem* p_owner = (PifXmodem*)p_issuer;

	switch (p_owner->__tx.state) {
	case XTS_WAIT_RESPONSE:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u Count=%u", p_owner->_id,
				p_owner->__tx.state, p_owner->__rx.count);
#endif
		p_owner->__tx.state = XTS_IDLE;
		if (p_owner->__tx.evt_receive) {
			(*p_owner->__tx.evt_receive)(ASCII_NAK, p_owner->__p_data[1]);
		}
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) TxTimeout State:%u", p_owner->_id, p_owner->__tx.state);
#endif
		break;
	}
}

#ifndef __PIF_NO_LOG__

#define PKT_ERR_INVALID_PACKET_NO	0
#define PKT_ERR_WRONG_CRC    		1

static const char *c_cPktErr[] = {
		"Invalid Packet No",
		"Wrong CRC"
};

#endif

static void _parsingPacket(PifXmodem* p_owner, PifActCommReceiveData act_receive_data)
{
	PifXmodemPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
	uint16_t crc;

	while ((*act_receive_data)(p_owner->__p_comm, &data)) {
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
				if (!pifPulse_StartItem(p_owner->__rx.p_timer, p_owner->__rx.timeout * 1000L / p_owner->__p_timer->_period1us)) {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_WARN, "XM(%u) Not start timer", p_owner->_id);
#endif
				}
				break;

			case ASCII_EOT:
				p_owner->__rx.state = XRS_EOT;

				p_owner->__tx.state = XTS_ACK;
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
#ifndef __PIF_NO_LOG__
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
					pifPulse_StopItem(p_owner->__rx.p_timer);
					p_owner->__rx.state = XRS_SOH;

					p_owner->__tx.state = XTS_ACK;
				}
				else {
#ifndef __PIF_NO_LOG__
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
			            pifPulse_StopItem(p_owner->__rx.p_timer);
			            p_owner->__rx.state = XRS_SOH;

			            p_owner->__tx.state = XTS_ACK;
					}
					else {
#ifndef __PIF_NO_LOG__
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
	return;

fail:
	pifPulse_StopItem(p_owner->__rx.p_timer);
	p_owner->__rx.state = XRS_IDLE;

	p_owner->__tx.state = XTS_NAK;
}

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint8_t data;

	if (p_owner->__tx.state == XTS_WAIT_RESPONSE) {
		if ((*act_receive_data)(p_owner->__p_comm, &data)) {
			switch (data) {
			case ASCII_ACK:
			case ASCII_NAK:
			case ASCII_CAN:
				pifPulse_StopItem(p_owner->__tx.p_timer);
				p_owner->__tx.state = XTS_IDLE;
				if (p_owner->__tx.evt_receive) {
					(*p_owner->__tx.evt_receive)(data, p_owner->__p_data[1]);
				}
				break;
			}
		}
	}
	else {
		_parsingPacket(p_owner, act_receive_data);

		switch (p_owner->__rx.state) {
		case XRS_C:
			if (p_owner->__tx.evt_receive) {
				(*p_owner->__tx.evt_receive)(p_owner->__rx.state, 0);
			}
			p_owner->__rx.state = XRS_IDLE;
#ifndef __PIF_NO_LOG__
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
	}
}

static BOOL _evtSending(void* p_client, PifActCommSendData act_send_data)
{
	PifXmodem* p_owner = (PifXmodem*)p_client;
	uint16_t length;
	uint8_t data;
	static uint32_t timer1ms;

	switch (p_owner->__tx.state) {
	case XTS_SEND_C:
		data = 'C';
		if ((*act_send_data)(p_owner->__p_comm, &data, 1)) {
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_NONE, "C");
#endif
			timer1ms = pif_cumulative_timer1ms;
			p_owner->__tx.state = XTS_DELAY_C;
			return TRUE;
		}
		break;

	case XTS_DELAY_C:
		if (PIF_CHECK_ELAPSE_TIME_1MS(timer1ms, 3000)) {			// 3000ms
			p_owner->__tx.state = XTS_SEND_C;
		}
		break;

	case XTS_SENDING:
    	length = (*act_send_data)(p_owner->__p_comm, p_owner->__p_data + p_owner->__tx.data_pos,
    			p_owner->__packet_size - p_owner->__tx.data_pos);
		p_owner->__tx.data_pos += length;
		if (p_owner->__tx.data_pos >= p_owner->__packet_size) {
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		return TRUE;

	case XTS_EOT:
		if ((*act_send_data)(p_owner->__p_comm, (uint8_t *)&p_owner->__tx.state, 1)) {
			p_owner->__tx.state = XTS_WAIT_RESPONSE;
		}
		return TRUE;

	case XTS_CAN:
		if ((*act_send_data)(p_owner->__p_comm, (uint8_t *)&p_owner->__tx.state, 1)) {
			if (p_owner->__tx.evt_receive) {
				p_owner->__tx.state = XTS_WAIT_RESPONSE;
			}
			else {
				p_owner->__tx.state = XTS_IDLE;
			}
		}
		return TRUE;

	case XTS_ACK:
	case XTS_NAK:
		if ((*act_send_data)(p_owner->__p_comm, (uint8_t *)&p_owner->__tx.state, 1)) {
			p_owner->__tx.state = XTS_IDLE;
		}
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/**
 * @fn pifXmodem_Create
 * @brief
 * @param id
 * @param p_timer
 * @param type
 * @return
 */
PifXmodem* pifXmodem_Create(PifId id, PifPulse* p_timer, PifXmodemType type)
{
    PifXmodem *p_owner = NULL;

    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

    p_owner = calloc(sizeof(PifXmodem), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    p_owner->__p_timer = p_timer;
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

    p_owner->__tx.p_timer = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__tx.p_timer) goto fail;

    p_owner->__rx.p_timer = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;

    p_owner->__type = type;

    p_owner->__tx.state = XTS_IDLE;
    pifPulse_AttachEvtFinish(p_owner->__tx.p_timer, _evtTimerTxTimeout, p_owner);
    p_owner->__tx.timeout = PIF_XMODEM_RESPONSE_TIMEOUT;

    p_owner->__rx.state = XRS_IDLE;
    pifPulse_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
    p_owner->__rx.timeout = PIF_XMODEM_RECEIVE_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return p_owner;

fail:
	pifXmodem_Destroy(&p_owner);
	return NULL;
}

/**
 * @fn pifXmodem_Destroy
 * @brief
 * @param pp_owner
 */
void pifXmodem_Destroy(PifXmodem** pp_owner)
{
    if (*pp_owner) {
    	PifXmodem* p_owner = *pp_owner;
		if (p_owner->__p_data) {
			free(p_owner->__p_data);
			p_owner->__p_data = NULL;
		}
		if (p_owner->__rx.p_timer) {
			pifPulse_RemoveItem(p_owner->__rx.p_timer);
		}
		if (p_owner->__tx.p_timer) {
			pifPulse_RemoveItem(p_owner->__tx.p_timer);
		}
    	free(*pp_owner);
    	*pp_owner = NULL;
    }
}

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief
 * @param p_owner
 * @param response_timeout
 */
void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout)
{
	p_owner->__tx.timeout = response_timeout;
}

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief
 * @param p_owner
 * @param receive_timeout
 */
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout)
{
	p_owner->__rx.timeout = receive_timeout;
}

/**
 * @fn pifXmodem_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifXmodem_AttachComm(PifXmodem* p_owner, PifComm* p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner);
	p_comm->evt_parsing = _evtParsing;
	p_comm->evt_sending = _evtSending;
}

/**
 * @fn pifXmodem_AttachEvtTxReceive
 * @brief
 * @param p_owner
 * @param evt_tx_receive
 */
void pifXmodem_AttachEvtTxReceive(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive)
{
	p_owner->__tx.evt_receive = evt_tx_receive;
}

/**
 * @fn pifXmodem_AttachEvtRxReceive
 * @brief
 * @param p_owner
 * @param evt_rx_receive
 */
void pifXmodem_AttachEvtRxReceive(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive)
{
	p_owner->__rx.evt_receive = evt_rx_receive;
}

/**
 * @fn pifXmodem_SendData
 * @brief
 * @param p_owner
 * @param packet_no
 * @param p_data
 * @param data_size
 * @return
 */
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
	if (!pifPulse_StartItem(p_owner->__tx.p_timer, p_owner->__tx.timeout * 1000L / p_owner->__p_timer->_period1us)) {
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "XM(%u) Not start timer", p_owner->_id);
#endif
	}
	return TRUE;
}

/**
 * @fn pifXmodem_SendEot
 * @brief
 * @param p_owner
 */
void pifXmodem_SendEot(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_EOT;
}

/**
 * @fn pifXmodem_SendCancel
 * @brief
 * @param p_owner
 */
void pifXmodem_SendCancel(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_CAN;
}

/**
 * @fn pifXmodem_ReadyReceive
 * @brief
 * @param p_owner
 */
void pifXmodem_ReadyReceive(PifXmodem* p_owner)
{
	p_owner->__tx.state = XTS_SEND_C;
}
