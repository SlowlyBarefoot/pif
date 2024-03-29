#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_protocol.h"


#if PIF_PROTOCOL_RECEIVE_TIMEOUT

static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	PifProtocol* p_owner = (PifProtocol*)p_issuer;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "PTC(%u) ParsingPacket(Timeout) State:%u Len:%u Cnt:%u", p_owner->_id,
			p_owner->__rx.state, p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x : %x : %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7], p_owner->__rx.packet.crc,
			p_owner->__rx.header_count);
#endif
#endif
	pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_NAK);
	p_owner->__rx.state = PRS_IDLE;
}

#endif

static void _evtTimerTxTimeout(PifIssuerP p_issuer)
{
	PifProtocol* p_owner = (PifProtocol*)p_issuer;

	switch (p_owner->__tx.state) {
	case PTS_WAIT_RESPONSE:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "PTC(%u) TxTimeout State:%u Len:%u Cnt:%u", p_owner->_id, p_owner->__tx.state,
				p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#endif
		p_owner->__tx.state = PTS_RETRY;
		break;

	case PTS_RETRY_DELAY:
		p_owner->__tx.state = PTS_RETRY;
		break;

	default:
#ifndef PIF_NO_LOG
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

#ifndef PIF_NO_LOG

static const char* kPktErr[5] = {
		"Big Length",
		"Invalid Data",
		"Wrong Command",
		"Wrong CRC",
		"Wrong ETX"
};

#endif

static void _parsingPacket(PifProtocol* p_owner, PifActUartReceiveData act_receive_data)
{
	PifProtocolPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
#ifndef PIF_NO_LOG
	uint8_t pkt_err;
#endif
	static uint8_t crc7;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx.state)	{
		case PRS_IDLE:
			if (data == ASCII_STX) {
				p_owner->__rx.p_packet[0] = data;
				p_owner->__rx.header_count = 1;
				p_owner->__rx.state = PRS_GET_HEADER;
				crc7 = 0;
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
				if (p_owner->__tx.state == PTS_IDLE) {
					pifTimer_Start(p_owner->__rx.p_timer, PIF_PROTOCOL_RECEIVE_TIMEOUT);
				}
#endif
			}
			else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
				if (data == ASCII_ACK) {
					p_owner->__rx.state = PRS_ACK;
				}
				else {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_WARN, "PTC(%u):Receive NAK(%xh)", p_owner->_id, data);
#endif
#if PIF_PROTOCOL_RETRY_DELAY
					pifTimer_Start(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
					p_owner->__tx.state = PTS_RETRY_DELAY;
#else
					pifTimer_Stop(p_owner->__tx.p_timer);
					p_owner->__tx.state = PTS_enRetry;
#endif
					p_owner->__rx.state = PRS_IDLE;
				}
			}
			break;

		case PRS_GET_HEADER:
			if (data >= 0x20) {
				crc7 = pifCrc7_Add(crc7, data);
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
#ifndef PIF_NO_LOG
						pkt_err = PKT_ERR_BIG_LENGHT;
#endif
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
#ifndef PIF_NO_LOG
				pkt_err = PKT_ERR_INVALID_DATA;
#endif
				goto fail;
			}
			break;

		case PRS_GET_DATA:
			crc7 = pifCrc7_Add(crc7, data);
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
#ifndef PIF_NO_LOG
				pkt_err = PKT_ERR_INVALID_DATA;
#endif
				goto fail;
			}
			break;

		case PRS_GET_CRC:
			if (data >= 0x20) {
				p_packet->crc = data;
				if (p_packet->crc == (0x80 | pifCrc7_Result(crc7))) {
					p_owner->__rx.state = PRS_GET_TAILER;
				}
				else {
#ifndef PIF_NO_LOG
					pkt_err = PKT_ERR_WRONG_CRC;
#endif
					goto fail;
				}
			}
			else {
#ifndef PIF_NO_LOG
				pkt_err = PKT_ERR_INVALID_DATA;
#endif
				goto fail;
			}
			break;

		case PRS_GET_TAILER:
			if (data == ASCII_ETX) {
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
	            pifTimer_Stop(p_owner->__rx.p_timer);
#endif
	            p_owner->__rx.state = PRS_DONE;
	            return;
			}
			else {
#ifndef PIF_NO_LOG
				pkt_err = PKT_ERR_WRONG_ETX;
#endif
				goto fail;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
#ifndef PIF_NO_LOG
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
    	pifTimer_Stop(p_owner->__rx.p_timer);
#endif
    	pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_NAK);
    }
    else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
#if PIF_PROTOCOL_RETRY_DELAY
    	pifTimer_Start(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
		p_owner->__tx.state = PTS_RETRY_DELAY;
#else
		pifTimer_Stop(p_owner->__tx.p_timer);
		p_owner->__tx.state = PTS_enRetry;
#endif
    }
	p_owner->__rx.state = PRS_IDLE;
}

static void _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifProtocol* p_owner = (PifProtocol*)p_client;
	PifProtocolPacket* p_packet;

	_parsingPacket(p_owner, act_receive_data);

    if (p_owner->__rx.state == PRS_DONE) {
#ifndef PIF_NO_LOG
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
#ifndef PIF_NO_LOG
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
#ifndef PIF_NO_LOG
    	    	if (p_packet->flags & PF_LOG_PRINT_MASK) {
    	    		pifLog_Printf(LT_COMM, "PTC(%u) Rs:%xh F:%xh P:%d L:%d CRC:%xh", p_owner->_id, p_packet->command,
    	    				p_packet->flags, p_packet->packet_id, p_packet->length, p_packet->crc);
    	    	}
#endif

    	    	if (p_owner->__tx.ui.st.command == p_packet->command && (p_owner->_type == PT_SMALL ||
    	    			p_owner->__tx.ui.st.packet_id == p_packet->packet_id)) {
    				pifRingBuffer_Remove(&p_owner->__tx.request_buffer, 5 + p_owner->__tx.ui.st.length);
    				pifTimer_Stop(p_owner->__tx.p_timer);
					(*p_owner->__tx.p_request->evt_response)(p_packet);
					p_owner->__tx.state = PTS_IDLE;
    	    	}
    	    	else {
#if PIF_PROTOCOL_RETRY_DELAY
    	    		pifTimer_Start(p_owner->__tx.p_timer, PIF_PROTOCOL_RETRY_DELAY);
					p_owner->__tx.state = PTS_RETRY_DELAY;
#else
					pifTimer_Stop(p_owner->__tx.p_timer);
					p_owner->__tx.state = PTS_enRetry;
#endif
    	    	}
    	    }
#ifndef PIF_NO_LOG
    	    else {
    	    	pifLog_Printf(LT_WARN, "PTC(%u) Invalid State %d", p_owner->_id, p_owner->__tx.state);
    	    }
#endif
    	}
    	p_owner->__rx.state = PRS_IDLE;
    }
    else if (p_owner->__rx.state == PRS_ACK) {
		pifRingBuffer_Remove(&p_owner->__tx.request_buffer, 5 + p_owner->__tx.ui.st.length);
        pifTimer_Stop(p_owner->__tx.p_timer);
		(*p_owner->__tx.p_request->evt_response)(NULL);
		p_owner->__tx.state = PTS_IDLE;
    	p_owner->__rx.state = PRS_IDLE;
    }
}

static uint16_t _evtSending(void* p_client, PifActUartSendData act_send_data)
{
	PifProtocol* p_owner = (PifProtocol*)p_client;
	uint16_t length;

	if (!p_owner->__p_uart->_fc_state) return 0;
	if (p_owner->__rx.state != PRS_IDLE) return 0;

	if (!pifRingBuffer_IsEmpty(&p_owner->__tx.answer_buffer)) {
		switch (p_owner->__tx.state) {
	    case PTS_IDLE:
	    	if (!pifRingBuffer_IsEmpty(&p_owner->__tx.answer_buffer)) {
				p_owner->__tx.ui.st.length = pifRingBuffer_GetFillSize(&p_owner->__tx.answer_buffer);
				p_owner->__tx.pos = 0;
				p_owner->__tx.state = PTS_SENDING;
	    	}
	    	break;

	    case PTS_SENDING:
	    	length = (*act_send_data)(p_owner->__p_uart, pifRingBuffer_GetTailPointer(&p_owner->__tx.answer_buffer, p_owner->__tx.pos),
	    			pifRingBuffer_GetLinerSize(&p_owner->__tx.answer_buffer, p_owner->__tx.pos));
	    	if (!length) return 0;

	    	p_owner->__tx.pos += length;
			if (p_owner->__tx.pos >= p_owner->__tx.ui.st.length) {
				pifRingBuffer_Remove(&p_owner->__tx.answer_buffer, p_owner->__tx.pos);
				p_owner->__tx.state = PTS_IDLE;
			}
			break;

		default:
			break;
	    }
	}
	else {
	    switch (p_owner->__tx.state) {
	    case PTS_IDLE:
	    	if (!pifRingBuffer_IsEmpty(&p_owner->__tx.request_buffer)) {
				pifRingBuffer_CopyToArray(p_owner->__tx.ui.info, 9, &p_owner->__tx.request_buffer, 0);
				p_owner->__tx.pos = 5;
				p_owner->__tx.state = PTS_SENDING;
	    	}
	    	break;

	    case PTS_SENDING:
	    	length = (*act_send_data)(p_owner->__p_uart, pifRingBuffer_GetTailPointer(&p_owner->__tx.request_buffer, p_owner->__tx.pos),
	    			pifRingBuffer_GetLinerSize(&p_owner->__tx.request_buffer, p_owner->__tx.pos));
	    	p_owner->__tx.pos += length;
			if (p_owner->__tx.pos >= 5 + p_owner->__tx.ui.st.length) {
				p_owner->__tx.state = PTS_WAIT_SENDED;
			}
			break;

	    case PTS_WAIT_SENDED:
			if ((p_owner->__tx.ui.st.flags & PF_RESPONSE_MASK) == PF_RESPONSE_NO) {
				pifRingBuffer_Remove(&p_owner->__tx.request_buffer, 5 + p_owner->__tx.ui.st.length);
				p_owner->__tx.state = PTS_IDLE;
			}
			else {
				if (!pifTimer_Start(p_owner->__tx.p_timer, p_owner->__tx.ui.st.timeout)) {
					pif_error = E_OVERFLOW_BUFFER;
					if (p_owner->evt_error) (*p_owner->evt_error)(p_owner->_id);
					pifRingBuffer_Remove(&p_owner->__tx.request_buffer, 5 + p_owner->__tx.ui.st.length);
#ifndef PIF_NO_LOG
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
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_WARN, "PTC(%u) Retry: %d", p_owner->_id, p_owner->__tx.ui.st.retry);
#endif
				p_owner->__rx.state = PRS_IDLE;
				p_owner->__tx.pos = 5;
				p_owner->__tx.state = PTS_SENDING;
			}
			else {
				pif_error = E_TRANSFER_FAILED;
				if (p_owner->evt_error) (*p_owner->evt_error)(p_owner->_id);
				pifRingBuffer_Remove(&p_owner->__tx.request_buffer, 5 + p_owner->__tx.ui.st.length);
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "PTC(%u) Transfer failed", p_owner->_id);
#endif
				p_owner->__tx.state = PTS_IDLE;
			}
	    	break;

		default:
			break;
	    }
	}
	return 0;
}

BOOL pifProtocol_Init(PifProtocol* p_owner, PifId id, PifTimerManager* p_timer_manager, PifProtocolType type,
		const PifProtocolQuestion* p_questions)
{
	const PifProtocolQuestion* p_question = p_questions;

	if (!p_owner || !p_timer_manager || !p_questions) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	while (p_question->command) {
		if (p_question->command < 0x20) {
	        pif_error = E_INVALID_PARAM;
			return FALSE;
		}
		p_question++;
	}

	memset(p_owner, 0, sizeof(PifProtocol));

    p_owner->__p_timer_manager = p_timer_manager;
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
    p_owner->__rx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
#endif

    if (id == PIF_ID_AUTO) id = pif_id++;

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.request_buffer, PIF_ID_AUTO, PIF_PROTOCOL_TX_REQUEST_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.request_buffer, "RQB");

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.answer_buffer, PIF_ID_AUTO, PIF_PROTOCOL_TX_ANSWER_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.answer_buffer, "RSB");

    p_owner->__tx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__tx.p_timer) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__tx.p_timer, _evtTimerTxTimeout, p_owner);

    p_owner->_type = type;
    p_owner->__p_questions = p_questions;
    p_owner->_id = id;
    p_owner->__packet_id = 0x20;
    p_owner->__rx.packet_size = 10 + PIF_PROTOCOL_RX_PACKET_SIZE;
    return TRUE;

fail:
	pifProtocol_Clear(p_owner);
    return FALSE;
}

void pifProtocol_Clear(PifProtocol* p_owner)
{
	if (p_owner->__rx.p_packet) {
		free(p_owner->__rx.p_packet);
		p_owner->__rx.p_packet = NULL;
	}
	pifRingBuffer_Clear(&p_owner->__tx.request_buffer);
	pifRingBuffer_Clear(&p_owner->__tx.answer_buffer);
	if (p_owner->__rx.p_timer) {
		pifTimerManager_Remove(p_owner->__rx.p_timer);
		p_owner->__rx.p_timer = NULL;
	}
	if (p_owner->__tx.p_timer) {
		pifTimerManager_Remove(p_owner->__tx.p_timer);
		p_owner->__tx.p_timer = NULL;
	}
}

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

BOOL pifProtocol_ResizeTxRequest(PifProtocol* p_owner, uint16_t tx_request_size)
{
    if (!tx_request_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    return pifRingBuffer_ResizeHeap(&p_owner->__tx.request_buffer, tx_request_size);
}

BOOL pifProtocol_ResizeTxResponse(PifProtocol* p_owner, uint16_t tx_response_size)
{
    if (tx_response_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    return pifRingBuffer_ResizeHeap(&p_owner->__tx.answer_buffer, tx_response_size);
}

void pifProtocol_AttachUart(PifProtocol* p_owner, PifUart* p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifProtocol_DetachUart(PifProtocol* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifProtocol_MakeRequest(PifProtocol* p_owner, const PifProtocolRequest* p_request, uint8_t* p_data, uint16_t data_size)
{
	uint16_t i;
	uint16_t length;
	uint8_t header[13];
	uint8_t tailer[10];
	uint8_t packet_id = 0, data, lack;
	uint8_t crc7 = 0;

	if (p_owner->__tx.state != PTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__tx.p_request = p_request;
	p_owner->__tx.p_data = p_data;
	p_owner->__tx.data_size = data_size;

	pifRingBuffer_BeginPutting(&p_owner->__tx.request_buffer);

	uint16_t usCount = 0;
	for (i = 0; i < data_size; i++) {
		if (p_owner->__tx.p_data[i] < 0x20) usCount++;
	}

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
	if (p_owner->__p_uart->_frame_size > 1) {
		lack = length % p_owner->__p_uart->_frame_size;
		if (lack > 0) lack = p_owner->__p_uart->_frame_size - lack;
		length += lack;
	}
	else lack = 0;
	header[0] = length & 0xFF;
	header[1] = (length >> 8) & 0xFF;
	header[2] = p_request->timeout & 0xFF;
	header[3] = (p_request->timeout >> 8) & 0xFF;
	header[4] = p_request->retry;
	if (!pifRingBuffer_PutData(&p_owner->__tx.request_buffer, header, 5 + p_owner->__header_size)) goto fail;
	for (i = 1; i < p_owner->__header_size; i++) {
		crc7 = pifCrc7_Add(crc7, header[5 + i]);
	}

	for (i = 0; i < data_size; i++) {
		data = p_owner->__tx.p_data[i];
		if (data < 0x20) {
			crc7 = pifCrc7_Add(crc7, ASCII_DLE);
			if (!pifRingBuffer_PutByte(&p_owner->__tx.request_buffer, ASCII_DLE)) goto fail;
			data |= 0x80;
		}
		crc7 = pifCrc7_Add(crc7, data);
		if (!pifRingBuffer_PutByte(&p_owner->__tx.request_buffer, data)) goto fail;
	}

	tailer[0] = 0x80 | pifCrc7_Result(crc7);
	tailer[1] = ASCII_ETX;
	for (i = 0; i < lack; i++) tailer[2 + i] = 0;
	if (!pifRingBuffer_PutData(&p_owner->__tx.request_buffer, tailer, 2 + lack)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.request_buffer);

#ifndef PIF_NO_LOG
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
	pifRingBuffer_RollbackPutting(&p_owner->__tx.request_buffer);
	return FALSE;
}

BOOL pifProtocol_MakeAnswer(PifProtocol* p_owner, PifProtocolPacket* p_question, uint8_t flags,
		uint8_t* p_data, uint16_t data_size)
{
	uint16_t i;
	uint8_t header[8];
	uint8_t tailer[10];
	uint8_t packet_id = 0, data, lack;
	uint8_t crc7 = 0;
	uint16_t length;

	pifRingBuffer_BeginPutting(&p_owner->__tx.answer_buffer);

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
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, header, p_owner->__header_size)) goto fail;
	for (i = 1; i < p_owner->__header_size; i++) {
		crc7 = pifCrc7_Add(crc7, header[i]);
	}
	length = p_owner->__header_size;

	for (i = 0; i < data_size; i++) {
		data = p_data[i];
		if (data < 0x20) {
			crc7 = pifCrc7_Add(crc7, ASCII_DLE);
			if (!pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_DLE)) goto fail;
			data |= 0x80;
			length++;
		}
		crc7 = pifCrc7_Add(crc7, data);
		if (!pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, data)) goto fail;
		length++;
	}

	tailer[0] = 0x80 | pifCrc7_Result(crc7);
	tailer[1] = ASCII_ETX;
	length += 2;
	if (p_owner->__p_uart->_frame_size > 1) {
		lack = length % p_owner->__p_uart->_frame_size;
		if (lack > 0) lack = p_owner->__p_uart->_frame_size - lack;
		for (i = 0; i < lack; i++) tailer[2 + i] = 0;
	}
	else lack = 0;
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, tailer, 2 + lack)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.answer_buffer);

#ifndef PIF_NO_LOG
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
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	return FALSE;
}
