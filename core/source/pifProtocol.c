#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifProtocol.h"


#if PIF_PROTOCOL_RECEIVE_TIMEOUT

static void _evtTimerRxTimeout(void* p_issuer)
{
	PifProtocol* p_owner = (PifProtocol*)p_issuer;

#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_ERROR, "PTC(%u) ParsingPacket(Timeout) State:%u Len:%u Cnt:%u", p_owner->_id,
			p_owner->__rx.state, p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x : %x : %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7], p_owner->__rx.packet.crc,
			p_owner->__rx.header_count);
#endif
#endif
	pifRingBuffer_PutByte(p_owner->__tx.p_answer_buffer, ASCII_NAK);
	p_owner->__rx.state = PRS_IDLE;
}

#endif

static void _evtTimerTxTimeout(void* p_issuer)
{
	PifProtocol* p_owner = (PifProtocol*)p_issuer;

	switch (p_owner->__tx.state) {
	case PTS_WAIT_RESPONSE:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "PTC(%u) TxTimeout State:%u Len:%u Cnt:%u", p_owner->_id, p_owner->__tx.state,
				p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#endif
		p_owner->__tx.state = PTS_RETRY;
		break;

	case PTS_RETRY_DELAY:
		p_owner->__tx.state = PTS_RETRY;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "PTC(%u) TxTimeout State:%u", p_owner->_id, p_owner->__tx.state);
#endif
		break;
	}
}

#define PKT_ERR_BIG_LENGHT		0
#define PKT_ERR_INVALID_DATA    1
#define PKT_ERR_WRONG_COMMAND	2
#define PKT_ERR_WRONG_CRC    	3
#define PKT_ERR_WRONG_ETX    	4

#ifndef __PIF_NO_LOG__

static const char* kPktErr[5] = {
		"Big Length",
		"Invalid Data",
		"Wrong Command",
		"Wrong CRC",
		"Wrong ETX"
};

#endif

static void _parsingPacket(PifProtocol* p_owner, PifActCommReceiveData act_receive_data)
{
	PifProtocolPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
	uint8_t pkt_err;

	while ((*act_receive_data)(p_owner->__p_comm, &data)) {
		switch (p_owner->__rx.state)	{
		case PRS_IDLE:
			if (data == ASCII_STX) {
				p_owner->__rx.p_packet[0] = data;
				p_owner->__rx.header_count = 1;
				p_owner->__rx.state = PRS_GET_HEADER;
				pifCrc7_Init();
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
				if (p_owner->__tx.state == PTS_IDLE) {
					pifPulse_StartItem(p_owner->__rx.p_timer, PIF_PROTOCOL_RECEIVE_TIMEOUT);
				}
#endif
			}
			else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
				if (data == ASCII_ACK) {
					p_owner->__rx.state = PRS_ACK;
				}
				else {
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_WARN, "PTC(%u):Receive NAK(%xh)", p_owner->_id, data);
#endif
#if PIF_PROTOCOL_RETRY_DELAY
					pifPulse_StartItem(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
					p_owner->__tx.state = PTS_RETRY_DELAY;
#else
					pifPulse_StopItem(p_owner->__tx.p_timer);
					p_owner->__tx.state = PTS_enRetry;
#endif
					p_owner->__rx.state = PRS_IDLE;
				}
			}
			break;

		case PRS_GET_HEADER:
			if (data >= 0x20) {
				pifCrc7_Calcurate(data);
				p_owner->__rx.p_packet[p_owner->__rx.header_count] = data;
				p_owner->__rx.header_count++;
				if (p_owner->__rx.header_count >= p_owner->__header_size) {
					p_packet->flags = p_owner->__rx.p_packet[1];
					p_packet->command = p_owner->__rx.p_packet[2];
					switch (p_owner->_type) {
					case PT_SMALL:
						p_packet->length = p_owner->__rx.p_packet[3] - 0x20;
						break;

					case PT_MEDIUM:
						p_packet->packet_id = p_owner->__rx.p_packet[3];
						p_packet->length = (p_owner->__rx.p_packet[4] & 0x7F) + ((p_owner->__rx.p_packet[5] & 0x7F) << 7);
						break;
					}
					if (!p_packet->length) {
						p_owner->__rx.state = PRS_GET_CRC;
						p_packet->p_data = NULL;
						p_packet->data_count = 0;
					}
					else if (p_packet->length > p_owner->__rx.packet_size - 10) {
						pkt_err = PKT_ERR_BIG_LENGHT;
						goto fail;
					}
					else {
						p_owner->__rx.state = PRS_GET_DATA;
						p_owner->__rx.data_link_escape = FALSE;
						p_packet->p_data = p_owner->__rx.p_packet + p_owner->__header_size;
						p_packet->data_count = 0;
					}
				}
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_GET_DATA:
			pifCrc7_Calcurate(data);
			if (data >= 0x20) {
				if (p_owner->__rx.data_link_escape) {
					p_owner->__rx.data_link_escape = FALSE;
					data &= 0x7F;
				}
				p_packet->p_data[p_packet->data_count] = data;
				p_packet->data_count++;
				if (p_packet->data_count >= p_packet->length) {
					p_owner->__rx.state = PRS_GET_CRC;
				}
			}
			else if (data == ASCII_DLE) {
				p_owner->__rx.data_link_escape = TRUE;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_GET_CRC:
			if (data >= 0x20) {
				p_packet->crc = data;
				if (p_packet->crc == (0x80 | pifCrc7_Result())) {
					p_owner->__rx.state = PRS_GET_TAILER;
				}
				else {
					pkt_err = PKT_ERR_WRONG_CRC;
					goto fail;
				}
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
				goto fail;
			}
			break;

		case PRS_GET_TAILER:
			if (data == ASCII_ETX) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
	            pifPulse_StopItem(p_owner->__rx.p_timer);
#endif
	            p_owner->__rx.state = PRS_DONE;
	            return;
			}
			else {
				pkt_err = PKT_ERR_WRONG_ETX;
				goto fail;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_ERROR, "PTC(%u) ParsingPacket(%s) TS:%u RS:%u Len:%u Cnt:%u", p_owner->_id, kPktErr[pkt_err],
			p_owner->__tx.state, p_owner->__rx.state, p_packet->length, p_packet->data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x : %x %x : %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7], p_owner->__rx.packet.crc, data,	p_owner->__rx.header_count);
#endif
#endif
    if (p_owner->__tx.state == PTS_IDLE) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    	pifPulse_StopItem(p_owner->__rx.p_timer);
#endif
    	pifRingBuffer_PutByte(p_owner->__tx.p_answer_buffer, ASCII_NAK);
    }
    else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
#if PIF_PROTOCOL_RETRY_DELAY
    	pifPulse_StartItem(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
		p_owner->__tx.state = PTS_RETRY_DELAY;
#else
		pifPulse_StopItem(p_owner->__tx.p_timer);
		p_owner->__tx.state = PTS_enRetry;
#endif
    }
	p_owner->__rx.state = PRS_IDLE;
}

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
	PifProtocol* p_owner = (PifProtocol*)p_client;
	PifProtocolPacket* p_packet;

	_parsingPacket(p_owner, act_receive_data);

    if (p_owner->__rx.state == PRS_DONE) {
#ifndef __PIF_NO_LOG__
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x %x : %x", p_owner->_id,
    			p_owner->__rx.p_packet[0],	p_owner->__rx.p_packet[1], p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3],
				p_owner->__rx.p_packet[4],	p_owner->__rx.p_packet[5],	p_owner->__rx.packet.crc);
#endif
#endif

    	p_packet = &p_owner->__rx.packet;

    	if ((p_packet->flags & PF_TYPE_MASK) == PF_TYPE_QUESTION) {
        	const PifProtocolQuestion *pstQuestion = p_owner->__p_questions;
        	while (pstQuestion->command) {
        		if (p_packet->command == pstQuestion->command) {
#ifndef __PIF_NO_LOG__
        	    	if (p_packet->flags & PF_LOG_PRINT_MASK) {
        	    		pifLog_Printf(LT_COMM, "PTC(%u) Qt:%xh F:%xh P:%d L:%d CRC:%xh", p_owner->_id, p_packet->command,
        	    				p_packet->flags, p_packet->packet_id, p_packet->length, p_packet->crc);
        	    	}
#endif
        	    	if (pstQuestion->evt_answer) (*pstQuestion->evt_answer)(p_packet);
        			break;
        		}
        		pstQuestion++;
        	}
    	}
    	else {
    	    if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
#ifndef __PIF_NO_LOG__
    	    	if (p_packet->flags & PF_LOG_PRINT_MASK) {
    	    		pifLog_Printf(LT_COMM, "PTC(%u) Rs:%xh F:%xh P:%d L:%d CRC:%xh", p_owner->_id, p_packet->command,
    	    				p_packet->flags, p_packet->packet_id, p_packet->length, p_packet->crc);
    	    	}
#endif

    	    	if (p_owner->__tx.ui.st.command == p_packet->command && (p_owner->_type == PT_SMALL ||
    	    			p_owner->__tx.ui.st.packet_id == p_packet->packet_id)) {
    				pifRingBuffer_Remove(p_owner->__tx.p_request_buffer, 5 + p_owner->__tx.ui.st.length);
		            pifPulse_StopItem(p_owner->__tx.p_timer);
					(*p_owner->__tx.p_request->evt_response)(p_packet);
					p_owner->__tx.state = PTS_IDLE;
    	    	}
    	    	else {
#if PIF_PROTOCOL_RETRY_DELAY
    	    		pifPulse_StartItem(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
					p_owner->__tx.state = PTS_RETRY_DELAY;
#else
					pifPulse_StopItem(p_owner->__tx.p_timer);
					p_owner->__tx.state = PTS_enRetry;
#endif
    	    	}
    	    }
#ifndef __PIF_NO_LOG__
    	    else {
    	    	pifLog_Printf(LT_WARN, "PTC(%u) Invalid State %d", p_owner->_id, p_owner->__tx.state);
    	    }
#endif
    	}
    	p_owner->__rx.state = PRS_IDLE;
    }
    else if (p_owner->__rx.state == PRS_ACK) {
		pifRingBuffer_Remove(p_owner->__tx.p_request_buffer, 5 + p_owner->__tx.ui.st.length);
        pifPulse_StopItem(p_owner->__tx.p_timer);
		(*p_owner->__tx.p_request->evt_response)(NULL);
		p_owner->__tx.state = PTS_IDLE;
    	p_owner->__rx.state = PRS_IDLE;
    }
}

static BOOL _evtSending(void* p_client, PifActCommSendData act_send_data)
{
	PifProtocol* p_owner = (PifProtocol*)p_client;
	uint16_t length;

	if (p_owner->__rx.state != PRS_IDLE) return FALSE;

	if (!pifRingBuffer_IsEmpty(p_owner->__tx.p_answer_buffer)) {
		switch (p_owner->__tx.state) {
	    case PTS_IDLE:
	    	if (!pifRingBuffer_IsEmpty(p_owner->__tx.p_answer_buffer)) {
				p_owner->__tx.ui.st.length = pifRingBuffer_GetFillSize(p_owner->__tx.p_answer_buffer);
				p_owner->__tx.pos = 0;
				p_owner->__tx.state = PTS_SENDING;
	    	}
	    	break;

	    case PTS_SENDING:
	    	length = (*act_send_data)(p_owner->__p_comm, pifRingBuffer_GetTailPointer(p_owner->__tx.p_answer_buffer, p_owner->__tx.pos),
	    			pifRingBuffer_GetLinerSize(p_owner->__tx.p_answer_buffer, p_owner->__tx.pos));
	    	if (!length) return FALSE;

	    	p_owner->__tx.pos += length;
			if (p_owner->__tx.pos >= p_owner->__tx.ui.st.length) {
				pifRingBuffer_Remove(p_owner->__tx.p_answer_buffer, p_owner->__tx.pos);
				p_owner->__tx.state = PTS_IDLE;
			}
			return TRUE;

		default:
			break;
	    }
	}
	else {
	    switch (p_owner->__tx.state) {
	    case PTS_IDLE:
	    	if (!pifRingBuffer_IsEmpty(p_owner->__tx.p_request_buffer)) {
				pifRingBuffer_CopyToArray(p_owner->__tx.ui.info, 9, p_owner->__tx.p_request_buffer, 0);
				p_owner->__tx.pos = 5;
				p_owner->__tx.state = PTS_SENDING;
	    	}
	    	break;

	    case PTS_SENDING:
	    	length = (*act_send_data)(p_owner->__p_comm, pifRingBuffer_GetTailPointer(p_owner->__tx.p_request_buffer, p_owner->__tx.pos),
	    			pifRingBuffer_GetLinerSize(p_owner->__tx.p_request_buffer, p_owner->__tx.pos));
	    	p_owner->__tx.pos += length;
			if (p_owner->__tx.pos >= 5 + p_owner->__tx.ui.st.length) {
				p_owner->__tx.state = PTS_WAIT_SENDED;
			}
			return TRUE;

	    case PTS_WAIT_SENDED:
			if ((p_owner->__tx.ui.st.flags & PF_RESPONSE_MASK) == PF_RESPONSE_NO) {
				pifRingBuffer_Remove(p_owner->__tx.p_request_buffer, 5 + p_owner->__tx.ui.st.length);
				p_owner->__tx.state = PTS_IDLE;
			}
			else {
				if (!pifPulse_StartItem(p_owner->__tx.p_timer, p_owner->__tx.ui.st.timeout)) {
					pif_error = E_OVERFLOW_BUFFER;
					if (p_owner->evt_error) (*p_owner->evt_error)(p_owner->_id);
					pifRingBuffer_Remove(p_owner->__tx.p_request_buffer, 5 + p_owner->__tx.ui.st.length);
#ifndef __PIF_NO_LOG__
					pifLog_Printf(LT_WARN, "PTC(%u) Not start timer", p_owner->_id);
#endif
					p_owner->__tx.state = PTS_IDLE;
				}
				else {
					p_owner->__tx.state = PTS_WAIT_RESPONSE;
				}
			}
			break;

	    case PTS_RETRY:
	    	p_owner->__tx.ui.st.retry--;
			if (p_owner->__tx.ui.st.retry) {
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_WARN, "PTC(%u) Retry: %d", p_owner->_id, p_owner->__tx.ui.st.retry);
#endif
				p_owner->__rx.state = PRS_IDLE;
				p_owner->__tx.pos = 5;
				p_owner->__tx.state = PTS_SENDING;
			}
			else {
				pif_error = E_TRANSFER_FAILED;
				if (p_owner->evt_error) (*p_owner->evt_error)(p_owner->_id);
				pifRingBuffer_Remove(p_owner->__tx.p_request_buffer, 5 + p_owner->__tx.ui.st.length);
#ifndef __PIF_NO_LOG__
				pifLog_Printf(LT_ERROR, "PTC(%u) Transfer failed", p_owner->_id);
#endif
				p_owner->__tx.state = PTS_IDLE;
			}
	    	break;

		default:
			break;
	    }
	}
	return FALSE;
}

/**
 * @fn pifProtocol_Create
 * @brief
 * @param id
 * @param p_timer
 * @param enType
 * @param p_questions
 * @return
 */
PifProtocol *pifProtocol_Create(PifId id, PifPulse* p_timer, PifProtocolType type,
		const PifProtocolQuestion* p_questions)
{
    PifProtocol* p_owner = NULL;
	const PifProtocolQuestion* p_question = p_questions;

	if (!p_timer) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

	while (p_question->command) {
		if (p_question->command < 0x20) {
	        pif_error = E_INVALID_PARAM;
			goto fail;
		}
		p_question++;
	}

	p_owner = calloc(sizeof(PifProtocol), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    p_owner->__p_timer = p_timer;
    switch (type) {
	case PT_SMALL:
		p_owner->__header_size = 4;
		break;

	case PT_MEDIUM:
		p_owner->__header_size = 6;
		break;

	default:
        pif_error = E_INVALID_PARAM;
        goto fail;
	}

    p_owner->__rx.p_packet = calloc(sizeof(uint8_t), 10 + PIF_PROTOCOL_RX_PACKET_SIZE);
    if (!p_owner->__rx.p_packet) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

#if PIF_PROTOCOL_RECEIVE_TIMEOUT
    p_owner->__rx.p_timer = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;
    pifPulse_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
#endif

    if (id == PIF_ID_AUTO) id = pif_id++;

    p_owner->__tx.p_request_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_PROTOCOL_TX_REQUEST_SIZE);
    if (!p_owner->__tx.p_request_buffer) goto fail;
    pifRingBuffer_SetName(p_owner->__tx.p_request_buffer, "RQB");

    p_owner->__tx.p_answer_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_PROTOCOL_TX_ANSWER_SIZE);
    if (!p_owner->__tx.p_answer_buffer) goto fail;
    pifRingBuffer_SetName(p_owner->__tx.p_answer_buffer, "RSB");

    p_owner->__tx.p_timer = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__tx.p_timer) goto fail;
    pifPulse_AttachEvtFinish(p_owner->__tx.p_timer, _evtTimerTxTimeout, p_owner);

    p_owner->_type = type;
    p_owner->__p_questions = p_questions;
    p_owner->_id = id;
    p_owner->__packet_id = 0x20;
    p_owner->__rx.packet_size = 10 + PIF_PROTOCOL_RX_PACKET_SIZE;
    p_owner->_frame_size = 1;
    return p_owner;

fail:
	pifProtocol_Destroy(&p_owner);
    return NULL;
}

/**
 * @fn pifProtocol_Destroy
 * @brief
 * @param pp_owner
 */
void pifProtocol_Destroy(PifProtocol** pp_owner)
{
    if (*pp_owner) {
    	PifProtocol* p_owner = *pp_owner;
		if (p_owner->__rx.p_packet) {
			free(p_owner->__rx.p_packet);
			p_owner->__rx.p_packet = NULL;
		}
		pifRingBuffer_Exit(p_owner->__tx.p_request_buffer);
		pifRingBuffer_Exit(p_owner->__tx.p_answer_buffer);
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
 * @fn pifProtocol_SetFrameSize
 * @brief
 * @param pvOwner
 * @param frame_size
 * @return
 */
BOOL pifProtocol_SetFrameSize(PifProtocol* p_owner, uint8_t frame_size)
{
	switch (frame_size) {
	case 1:
	case 2:
	case 4:
	case 8:
		p_owner->_frame_size = frame_size;
		return TRUE;
	}
	return FALSE;
}

/**
 * @fn pifProtocol_ResizeRxPacket
 * @brief
 * @param pvOwner
 * @param rx_packet_size
 * @return
 */
BOOL pifProtocol_ResizeRxPacket(PifProtocol* p_owner, uint16_t rx_packet_size)
{
    if (!rx_packet_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->__rx.p_packet = realloc(p_owner->__rx.p_packet, sizeof(uint8_t) * (10 + rx_packet_size));
    if (!p_owner->__rx.p_packet) {
        pif_error = E_OUT_OF_HEAP;
	    return FALSE;
    }

    p_owner->__rx.packet_size = 10 + rx_packet_size;
    return TRUE;
}

/**
 * @fn pifProtocol_ResizeTxRequest
 * @brief
 * @param pvOwner
 * @param tx_request_size
 * @return
 */
BOOL pifProtocol_ResizeTxRequest(PifProtocol* p_owner, uint16_t tx_request_size)
{
    if (!tx_request_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    return pifRingBuffer_ResizeHeap(p_owner->__tx.p_request_buffer, tx_request_size);
}

/**
 * @fn pifProtocol_ResizeTxResponse
 * @brief
 * @param pvOwner
 * @param tx_response_size
 * @return
 */
BOOL pifProtocol_ResizeTxResponse(PifProtocol* p_owner, uint16_t tx_response_size)
{
    if (tx_response_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    return pifRingBuffer_ResizeHeap(p_owner->__tx.p_answer_buffer, tx_response_size);
}

/**
 * @fn pifProtocol_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifProtocol_AttachComm(PifProtocol* p_owner, PifComm* p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner);
	p_comm->evt_parsing = _evtParsing;
	p_comm->evt_sending = _evtSending;
}

/**
 * @fn pifProtocol_MakeRequest
 * @brief
 * @param p_owner
 * @param p_request
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifProtocol_MakeRequest(PifProtocol* p_owner, const PifProtocolRequest* p_request, uint8_t* p_data, uint16_t data_size)
{
	int i;
	uint16_t length;
	uint8_t header[13];
	uint8_t tailer[10];
	uint8_t packet_id = 0, data, lack;

	if (p_owner->__tx.state != PTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__tx.p_request = p_request;
	p_owner->__tx.p_data = p_data;
	p_owner->__tx.data_size = data_size;

	pifRingBuffer_BackupHead(p_owner->__tx.p_request_buffer);

	uint16_t usCount = 0;
	for (i = 0; i < data_size; i++) {
		if (p_owner->__tx.p_data[i] < 0x20) usCount++;
	}

	pifCrc7_Init();

	header[5] = ASCII_STX;
	header[6] = PF_ALWAYS | p_request->flags;
	header[7] = p_request->command;
	switch (p_owner->_type) {
	case PT_SMALL:
		header[8] = 0x20 + data_size;
		break;

	case PT_MEDIUM:
		packet_id = p_owner->__packet_id;
		header[8] = packet_id;
		p_owner->__packet_id++;
		if (!p_owner->__packet_id) p_owner->__packet_id = 0x20;
		header[9] = 0x80 | (data_size & 0x7F);
		header[10] = 0x80 | ((data_size >> 7) & 0x7F);
		break;
	}
	length = p_owner->__header_size + data_size + usCount + 2;
	if (p_owner->_frame_size > 1) {
		lack = length % p_owner->_frame_size;
		if (lack > 0) lack = p_owner->_frame_size - lack;
		length += lack;
	}
	else lack = 0;
	header[0] = length & 0xFF;
	header[1] = (length >> 8) & 0xFF;
	header[2] = p_request->timeout & 0xFF;
	header[3] = (p_request->timeout >> 8) & 0xFF;
	header[4] = p_request->retry;
	if (!pifRingBuffer_PutData(p_owner->__tx.p_request_buffer, header, 5 + p_owner->__header_size)) goto fail;
	for (i = 1; i < p_owner->__header_size; i++) {
		pifCrc7_Calcurate(header[5 + i]);
	}

	for (i = 0; i < data_size; i++) {
		data = p_owner->__tx.p_data[i];
		if (data < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(p_owner->__tx.p_request_buffer, ASCII_DLE)) goto fail;
			data |= 0x80;
		}
		pifCrc7_Calcurate(data);
		if (!pifRingBuffer_PutByte(p_owner->__tx.p_request_buffer, data)) goto fail;
	}

	tailer[0] = 0x80 | pifCrc7_Result();
	tailer[1] = ASCII_ETX;
	for (i = 0; i < lack; i++) tailer[2 + i] = 0;
	if (!pifRingBuffer_PutData(p_owner->__tx.p_request_buffer, tailer, 2 + lack)) goto fail;

#ifndef __PIF_NO_LOG__
	if (p_request->flags & PF_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "PTC(%u) Rq:%xh F:%xh P:%d L:%d=%d CRC:%xh", p_owner->_id, p_request->command,
				p_request->flags, packet_id, data_size, length, tailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x %x %x %x", p_owner->_id,
			header[5], header[6], header[7], header[8], header[9], header[10], tailer[0], tailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(p_owner->__tx.p_request_buffer);
	return FALSE;
}

/**
 * @fn pifProtocol_MakeAnswer
 * @brief
 * @param p_owner
 * @param p_question
 * @param flags
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifProtocol_MakeAnswer(PifProtocol* p_owner, PifProtocolPacket* p_question, uint8_t flags,
		uint8_t* p_data, uint16_t data_size)
{
	int i;
	uint8_t header[8];
	uint8_t tailer[10];
	uint8_t packet_id = 0, data, lack;
	uint16_t length;

	pifRingBuffer_BackupHead(p_owner->__tx.p_answer_buffer);

	pifCrc7_Init();

	header[0] = ASCII_STX;
	header[1] = PF_ALWAYS | PF_TYPE_ANSWER | flags;
	header[2] = p_question->command;
	switch (p_owner->_type) {
	case PT_SMALL:
		header[3] = 0x20 + data_size;
		break;

	case PT_MEDIUM:
		packet_id = p_question->packet_id;
		header[3] = packet_id;
		header[4] = 0x80 | (data_size & 0x7F);
		header[5] = 0x80 | ((data_size >> 7) & 0x7F);
		break;
	}
	if (!pifRingBuffer_PutData(p_owner->__tx.p_answer_buffer, header, p_owner->__header_size)) goto fail;
	for (i = 1; i < p_owner->__header_size; i++) {
		pifCrc7_Calcurate(header[i]);
	}
	length = p_owner->__header_size;

	for (i = 0; i < data_size; i++) {
		data = p_data[i];
		if (data < 0x20) {
			pifCrc7_Calcurate(ASCII_DLE);
			if (!pifRingBuffer_PutByte(p_owner->__tx.p_answer_buffer, ASCII_DLE)) goto fail;
			data |= 0x80;
			length++;
		}
		pifCrc7_Calcurate(data);
		if (!pifRingBuffer_PutByte(p_owner->__tx.p_answer_buffer, data)) goto fail;
		length++;
	}

	tailer[0] = 0x80 | pifCrc7_Result();
	tailer[1] = ASCII_ETX;
	length += 2;
	if (p_owner->_frame_size > 1) {
		lack = length % p_owner->_frame_size;
		if (lack > 0) lack = p_owner->_frame_size - lack;
		for (i = 0; i < lack; i++) tailer[2 + i] = 0;
	}
	else lack = 0;
	if (!pifRingBuffer_PutData(p_owner->__tx.p_answer_buffer, tailer, 2 + lack)) goto fail;

#ifndef __PIF_NO_LOG__
	if (flags & PF_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "PTC(%u) As:%xh F:%xh P:%d L:%d=%d CRC:%xh", p_owner->_id, p_question->command,
				flags, packet_id, data_size, length + lack, tailer[0]);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x %x %x %x", p_owner->_id,
			header[0], header[1], header[2], header[3], header[4], header[5], tailer[0], tailer[1]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(p_owner->__tx.p_answer_buffer);
	return FALSE;
}
