#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_link.h"


// Each entry of the request buffer is [frame length (2)][CRC position (2)][PifLinkRequest*][frame].
#define REQUEST_INFO_SIZE	(4 + sizeof(const PifLinkRequest*))

// CRC-16/CCITT-FALSE is sent as 3 bytes of 7 bits each with bit 7 set.
#define CRC_INIT			0xFFFF
#define CRC_SIZE			3

// Room in the receive buffer for the largest header and the CRC.
#define RX_EXTRA_SIZE		(PIF_LINK_MULTI_HEADER_SIZE + CRC_SIZE)



static void _encodeCrc(uint16_t crc, uint8_t* p_buffer)
{
	p_buffer[0] = 0x80 | (crc & 0x7F);
	p_buffer[1] = 0x80 | ((crc >> 7) & 0x7F);
	p_buffer[2] = 0x80 | ((crc >> 14) & 0x03);
}

static void _startRetry(PifLink* p_owner)
{
#if PIF_LINK_RETRY_DELAY
	pifTimer_Start(p_owner->__tx.p_timer, PIF_LINK_RETRY_DELAY);
	p_owner->__tx.state = PTS_RETRY_DELAY;
#else
	pifTimer_Stop(p_owner->__tx.p_timer);
	p_owner->__tx.state = PTS_RETRY;
#endif
}

static void _finishRequest(PifLink* p_owner, PifLinkPacket* p_packet)
{
	const PifLinkRequest* p_request = p_owner->__tx.p_request;

	pifRingBuffer_Remove(&p_owner->__tx.request_buffer, REQUEST_INFO_SIZE + p_owner->__tx.length);
	pifTimer_Stop(p_owner->__tx.p_timer);
	p_owner->__tx.state = PTS_IDLE;
	if (p_request->evt_response) (*p_request->evt_response)(p_owner, p_packet);
}

static void _failRequest(PifLink* p_owner, PifLinkError error)
{
	const PifLinkRequest* p_request = p_owner->__tx.p_request;

	pifRingBuffer_Remove(&p_owner->__tx.request_buffer, REQUEST_INFO_SIZE + p_owner->__tx.length);
	pifTimer_Stop(p_owner->__tx.p_timer);
	p_owner->__tx.state = PTS_IDLE;
	if (p_owner->evt_error) (*p_owner->evt_error)(p_owner, p_request, error);
}

// Sets LF_RETRY in the queued frame of the request in progress and updates its CRC.
static void _markRetry(PifLink* p_owner)
{
	PifRingBuffer* p_buffer = &p_owner->__tx.request_buffer;
	uint8_t* p_flags = pifRingBuffer_GetTailPointer(p_buffer, REQUEST_INFO_SIZE + 1);
	uint8_t crc_bytes[CRC_SIZE];
	uint16_t i, crc = CRC_INIT;

	if (*p_flags & LF_RETRY) return;

	*p_flags |= LF_RETRY;
	for (i = 1; i < p_owner->__tx.crc_pos; i++) {
		crc = pifCrc16_Add(crc, *pifRingBuffer_GetTailPointer(p_buffer, REQUEST_INFO_SIZE + i));
	}
	_encodeCrc(crc, crc_bytes);
	for (i = 0; i < CRC_SIZE; i++) {
		*pifRingBuffer_GetTailPointer(p_buffer, REQUEST_INFO_SIZE + p_owner->__tx.crc_pos + i) = crc_bytes[i];
	}
}



#if PIF_LINK_RECEIVE_TIMEOUT

static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	PifLink* p_owner = (PifLink*)p_issuer;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "LK(%u) ParsingPacket(Timeout) State:%u Len:%u Cnt:%u", p_owner->_id,
			p_owner->__rx.state, p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x : %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7], p_owner->__rx.header_count);
#endif
#endif
	if (p_owner->_type == PLT_SINGLE && p_owner->__tx.state == PTS_IDLE) {
		pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_NAK);
	}
	p_owner->__rx.state = PRS_IDLE;
}

#endif

static void _evtTimerTxTimeout(PifIssuerP p_issuer)
{
	PifLink* p_owner = (PifLink*)p_issuer;

	switch (p_owner->__tx.state) {
	case PTS_WAIT_RESPONSE:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "LK(%u) TxTimeout State:%u Len:%u Cnt:%u", p_owner->_id, p_owner->__tx.state,
				p_owner->__rx.packet.length, p_owner->__rx.packet.data_count);
#endif
		p_owner->__tx.state = PTS_RETRY;
		break;

	case PTS_RETRY_DELAY:
		p_owner->__tx.state = PTS_RETRY;
		break;

	default:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "LK(%u) TxTimeout State:%u", p_owner->_id, p_owner->__tx.state);
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

static void _parsingPacket(PifLink* p_owner, PifActUartReceiveData act_receive_data)
{
	PifLinkPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data, *p_header;
#ifndef PIF_NO_LOG
	uint8_t pkt_err;
#endif

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx.state)	{
		case PRS_IDLE:
			if (data == ASCII_STX) {
				p_owner->__rx.p_packet[0] = data;
				p_owner->__rx.header_count = 1;
				p_owner->__rx.state = PRS_GET_HEADER;
				p_owner->__rx.crc = CRC_INIT;
#if PIF_LINK_RECEIVE_TIMEOUT
				pifTimer_Start(p_owner->__rx.p_timer, PIF_LINK_RECEIVE_TIMEOUT);
#endif
			}
			else if (p_owner->_type == PLT_SINGLE && p_owner->__tx.state == PTS_WAIT_RESPONSE) {
				if (data == ASCII_ACK) {
					p_owner->__rx.state = PRS_ACK;
					return;
				}
				else if (data == ASCII_NAK) {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_WARN, "LK(%u):Receive NAK", p_owner->_id);
#endif
					_startRetry(p_owner);
				}
			}
			break;

		case PRS_GET_HEADER:
			if (data >= 0x20) {
				p_owner->__rx.crc = pifCrc16_Add(p_owner->__rx.crc, data);
				p_owner->__rx.p_packet[p_owner->__rx.header_count] = data;
				p_owner->__rx.header_count++;
				if (p_owner->__rx.header_count >= p_owner->__header_size) {
					p_packet->flags = p_owner->__rx.p_packet[1];
					p_packet->command = p_owner->__rx.p_packet[2];
					p_packet->packet_id = p_owner->__rx.p_packet[3];
					p_header = p_owner->__rx.p_packet + p_owner->__header_size - 2;
					p_packet->length = (p_header[0] & 0x7F) + ((p_header[1] & 0x7F) << 7);
					p_packet->data_count = 0;
					p_owner->__rx.crc_count = 0;
					if (p_owner->_type == PLT_MULTI) {
						p_packet->src_id = p_owner->__rx.p_packet[4] & 0x7F;
						p_packet->dst_id = p_owner->__rx.p_packet[5] & 0x7F;
					}
					else {
						p_packet->src_id = 0;
						p_packet->dst_id = 0;
					}
					if (p_owner->_type == PLT_MULTI && p_packet->dst_id != p_owner->_address &&
							p_packet->dst_id != PIF_LINK_BROADCAST) {
						// Not for this node. The rest of the frame has no STX, so it is skipped in PRS_IDLE.
#if PIF_LINK_RECEIVE_TIMEOUT
						pifTimer_Stop(p_owner->__rx.p_timer);
#endif
						p_owner->__rx.state = PRS_IDLE;
					}
					else if (!p_packet->length) {
						p_owner->__rx.state = PRS_GET_CRC;
						p_packet->p_data = NULL;
					}
					else if (p_packet->length > p_owner->__rx.packet_size - RX_EXTRA_SIZE) {
#ifndef PIF_NO_LOG
						pkt_err = PKT_ERR_BIG_LENGHT;
#endif
						goto fail;
					}
					else {
						p_owner->__rx.state = PRS_GET_DATA;
						p_owner->__rx.data_link_escape = FALSE;
						p_packet->p_data = p_owner->__rx.p_packet + p_owner->__header_size;
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
			p_owner->__rx.crc = pifCrc16_Add(p_owner->__rx.crc, data);
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
			else if (data == ASCII_DLE && !p_owner->__rx.data_link_escape) {
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
			if (data & 0x80) {
				p_owner->__rx.p_packet[p_owner->__header_size + p_packet->length + p_owner->__rx.crc_count] = data;
				p_owner->__rx.crc_count++;
				if (p_owner->__rx.crc_count >= CRC_SIZE) {
					uint8_t* p_crc = p_owner->__rx.p_packet + p_owner->__header_size + p_packet->length;

					p_packet->crc = (p_crc[0] & 0x7F) | ((p_crc[1] & 0x7F) << 7) | ((uint16_t)(p_crc[2] & 0x03) << 14);
					if (p_packet->crc == p_owner->__rx.crc && !(p_crc[2] & 0x7C)) {
						p_owner->__rx.state = PRS_GET_TAILER;
					}
					else {
#ifndef PIF_NO_LOG
						pkt_err = PKT_ERR_WRONG_CRC;
#endif
						goto fail;
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

		case PRS_GET_TAILER:
			if (data == ASCII_ETX) {
#if PIF_LINK_RECEIVE_TIMEOUT
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
	pifLog_Printf(LT_ERROR, "LK(%u) ParsingPacket(%s) TS:%u RS:%u Len:%u Cnt:%u", p_owner->_id, kPktErr[pkt_err],
			p_owner->__tx.state, p_owner->__rx.state, p_packet->length, p_packet->data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x : %x : %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7], data, p_owner->__rx.header_count);
#endif
#endif
#if PIF_LINK_RECEIVE_TIMEOUT
	pifTimer_Stop(p_owner->__rx.p_timer);
#endif
    // On a shared bus the broken frame may belong to another node, so only the timeout handles it.
    if (p_owner->_type == PLT_SINGLE) {
    	if (p_owner->__tx.state == PTS_IDLE) {
    		pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_NAK);
    	}
    	else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
    		_startRetry(p_owner);
    	}
    }
	p_owner->__rx.state = PRS_IDLE;
}

static BOOL _isBroadcast(PifLink* p_owner, PifLinkPacket* p_packet)
{
	return p_owner->_type == PLT_MULTI && p_packet->dst_id == PIF_LINK_BROADCAST;
}

static void _handleQuestion(PifLink* p_owner, PifLinkPacket* p_packet)
{
	PifLinkLastQuestion* p_last = &p_owner->__last;
	const PifLinkQuestion* p_question;

#ifndef PIF_NO_LOG
	if (p_packet->flags & LF_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "LK(%u) Qt:%xh F:%xh P:%d L:%d CRC:%xh", p_owner->_id, p_packet->command,
				p_packet->flags, p_packet->packet_id, p_packet->length, p_packet->crc);
	}
#endif

	if ((p_packet->flags & LF_RETRY) && p_last->valid && p_last->command == p_packet->command &&
			p_last->packet_id == p_packet->packet_id && p_last->src_id == p_packet->src_id) {
		// Already handled: send the same answer again, or wait for the handler that has not answered yet.
		if (p_last->answer_length) {
			pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, p_last->p_answer, p_last->answer_length);
		}
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "LK(%u) Duplicate question %xh:%d", p_owner->_id, p_packet->command, p_packet->packet_id);
#endif
		return;
	}

	p_last->valid = TRUE;
	p_last->command = p_packet->command;
	p_last->packet_id = p_packet->packet_id;
	p_last->src_id = p_packet->src_id;
	p_last->answer_length = 0;

	for (p_question = p_owner->__p_questions; p_question->command; p_question++) {
		if (p_question->command == p_packet->command) break;
	}
	if (p_question->command && p_question->evt_answer) {
		(*p_question->evt_answer)(p_owner, p_packet);
	}
	else if ((p_packet->flags & LF_ANSWER_MASK) == LF_ANSWER_YES && !_isBroadcast(p_owner, p_packet)) {
		pifLink_MakeError(p_owner, p_packet, 0, LE_UNSUPPORTED);
	}
}

static BOOL _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifLink* p_owner = (PifLink*)p_client;
	PifLinkPacket* p_packet;
	BOOL rtn = FALSE;

	_parsingPacket(p_owner, act_receive_data);

    if (p_owner->__rx.state == PRS_DONE) {
    	p_owner->__rx.state = PRS_IDLE;
#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x %x : %x", p_owner->_id,
    			p_owner->__rx.p_packet[0],	p_owner->__rx.p_packet[1], p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3],
				p_owner->__rx.p_packet[4],	p_owner->__rx.p_packet[5],	p_owner->__rx.packet.crc);
#endif
#endif

    	p_packet = &p_owner->__rx.packet;

    	if ((p_packet->flags & LF_TYPE_MASK) == LF_TYPE_QUESTION) {
    		_handleQuestion(p_owner, p_packet);
    	}
    	else if (p_owner->__tx.state == PTS_WAIT_RESPONSE) {
#ifndef PIF_NO_LOG
    		if (p_packet->flags & LF_LOG_PRINT_MASK) {
    			pifLog_Printf(LT_COMM, "LK(%u) Rs:%xh F:%xh P:%d L:%d CRC:%xh", p_owner->_id, p_packet->command,
    					p_packet->flags, p_packet->packet_id, p_packet->length, p_packet->crc);
    		}
#endif
    		if (p_owner->__tx.p_request->command == p_packet->command &&
    				p_owner->__tx.packet_id == p_packet->packet_id && p_owner->__tx.dst_id == p_packet->src_id) {
    			if (p_packet->flags & LF_ERROR) {
    				pif_error = E_TRANSFER_FAILED;
    				_failRequest(p_owner, p_packet->data_count ? (PifLinkError)p_packet->p_data[0] : LE_FAILED);
    			}
    			else {
    				_finishRequest(p_owner, p_packet);
    			}
    		}
#ifndef PIF_NO_LOG
    		else {
    			// It may be a late response to an earlier attempt, so ignore it and keep waiting.
    			pifLog_Printf(LT_WARN, "LK(%u) Unexpected response %xh:%d", p_owner->_id, p_packet->command,
    					p_packet->packet_id);
    		}
#endif
    	}
#ifndef PIF_NO_LOG
    	else {
    		pifLog_Printf(LT_WARN, "LK(%u) Invalid State %d", p_owner->_id, p_owner->__tx.state);
    	}
#endif
		rtn = TRUE;
    }
    else if (p_owner->__rx.state == PRS_ACK) {
    	p_owner->__rx.state = PRS_IDLE;
    	_finishRequest(p_owner, NULL);
		rtn = TRUE;
    }
	return rtn;
}

static uint16_t _sendBuffer(PifLink* p_owner, PifActUartSendData act_send_data, PifRingBuffer* p_buffer,
		uint16_t pos, uint16_t end)
{
	uint16_t length = pifRingBuffer_GetLinerSize(p_buffer, pos);

	// Stop at the end of the current frame even if the next one follows it in the buffer.
	if (length > end - pos) length = end - pos;
	return (*act_send_data)(p_owner->__p_uart, pifRingBuffer_GetTailPointer(p_buffer, pos), length);
}

static uint16_t _evtSending(void* p_client, PifActUartSendData act_send_data)
{
	PifLink* p_owner = (PifLink*)p_client;
	uint8_t info[REQUEST_INFO_SIZE + PIF_LINK_MULTI_HEADER_SIZE];

	if (!p_owner->__p_uart->_fc_state) return 0;
	if (p_owner->__rx.state != PRS_IDLE) return 0;

	// Answers go out whenever a request frame is not being written, even while a response is awaited.
	if (!p_owner->__tx.answer_length && p_owner->__tx.state != PTS_SENDING &&
			!pifRingBuffer_IsEmpty(&p_owner->__tx.answer_buffer)) {
		p_owner->__tx.answer_length = pifRingBuffer_GetFillSize(&p_owner->__tx.answer_buffer);
		p_owner->__tx.answer_pos = 0;
	}
	if (p_owner->__tx.answer_length) {
		p_owner->__tx.answer_pos += _sendBuffer(p_owner, act_send_data, &p_owner->__tx.answer_buffer,
				p_owner->__tx.answer_pos, p_owner->__tx.answer_length);
		if (p_owner->__tx.answer_pos >= p_owner->__tx.answer_length) {
			pifRingBuffer_Remove(&p_owner->__tx.answer_buffer, p_owner->__tx.answer_length);
			p_owner->__tx.answer_length = 0;
		}
		return 0;
	}

	switch (p_owner->__tx.state) {
	case PTS_IDLE:
		if (!pifRingBuffer_IsEmpty(&p_owner->__tx.request_buffer)) {
			pifRingBuffer_CopyToArray(info, sizeof(info), &p_owner->__tx.request_buffer, 0);
			p_owner->__tx.length = pifLink_GetU16(info);
			p_owner->__tx.crc_pos = pifLink_GetU16(info + 2);
			memcpy(&p_owner->__tx.p_request, info + 4, sizeof(p_owner->__tx.p_request));
			p_owner->__tx.flags = info[REQUEST_INFO_SIZE + 1];
			p_owner->__tx.packet_id = info[REQUEST_INFO_SIZE + 3];
			p_owner->__tx.dst_id = p_owner->_type == PLT_MULTI ? info[REQUEST_INFO_SIZE + 5] & 0x7F : 0;
			p_owner->__tx.retry = p_owner->__tx.p_request->retry;
			p_owner->__tx.pos = REQUEST_INFO_SIZE;
			p_owner->__tx.state = PTS_SENDING;
		}
		break;

	case PTS_SENDING:
		p_owner->__tx.pos += _sendBuffer(p_owner, act_send_data, &p_owner->__tx.request_buffer,
				p_owner->__tx.pos, REQUEST_INFO_SIZE + p_owner->__tx.length);
		if (p_owner->__tx.pos >= REQUEST_INFO_SIZE + p_owner->__tx.length) {
			p_owner->__tx.state = PTS_WAIT_SENDED;
		}
		break;

	case PTS_WAIT_SENDED:
		if ((p_owner->__tx.flags & LF_RESPONSE_MASK) == LF_RESPONSE_NO) {
			pifRingBuffer_Remove(&p_owner->__tx.request_buffer, REQUEST_INFO_SIZE + p_owner->__tx.length);
			p_owner->__tx.state = PTS_IDLE;
		}
		else if (!pifTimer_Start(p_owner->__tx.p_timer, p_owner->__tx.p_request->timeout)) {
			pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_WARN, "LK(%u) Not start timer", p_owner->_id);
#endif
			_failRequest(p_owner, LE_NO_TIMER);
		}
		else {
			p_owner->__tx.state = PTS_WAIT_RESPONSE;
		}
		break;

	case PTS_RETRY:
		if (p_owner->__tx.retry) {
			p_owner->__tx.retry--;
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_WARN, "LK(%u) Retry: %d", p_owner->_id, p_owner->__tx.retry);
#endif
			_markRetry(p_owner);
			p_owner->__tx.pos = REQUEST_INFO_SIZE;
			p_owner->__tx.state = PTS_SENDING;
		}
		else {
			pif_error = E_TRANSFER_FAILED;
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_ERROR, "LK(%u) Transfer failed", p_owner->_id);
#endif
			_failRequest(p_owner, LE_TIMEOUT);
		}
		break;

	default:
		break;
	}
	return 0;
}

BOOL pifLink_Init(PifLink* p_owner, PifId id, PifTimerManager* p_timer_manager, PifLinkType type, uint8_t address,
		const PifLinkQuestion* p_questions)
{
	const PifLinkQuestion* p_question = p_questions;

	if (!p_owner || !p_timer_manager || !p_questions || (type != PLT_SINGLE && type != PLT_MULTI) ||
			(type == PLT_MULTI && address > PIF_LINK_MAX_ADDRESS)) {
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

	memset(p_owner, 0, sizeof(PifLink));

    p_owner->__p_timer_manager = p_timer_manager;
    p_owner->_type = type;
    p_owner->_address = type == PLT_MULTI ? address : 0;
    p_owner->__header_size = type == PLT_MULTI ? PIF_LINK_MULTI_HEADER_SIZE : PIF_LINK_SINGLE_HEADER_SIZE;

    p_owner->__rx.p_packet = calloc(sizeof(uint8_t), RX_EXTRA_SIZE + PIF_LINK_RX_PACKET_SIZE);
    if (!p_owner->__rx.p_packet) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

#if PIF_LINK_RECEIVE_TIMEOUT
    p_owner->__rx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
#endif

    if (id == PIF_ID_AUTO) id = pif_id++;

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.request_buffer, PIF_ID_AUTO, PIF_LINK_TX_REQUEST_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.request_buffer, "RQB");

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.answer_buffer, PIF_ID_AUTO, PIF_LINK_TX_ANSWER_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.answer_buffer, "RSB");

    p_owner->__last.p_answer = malloc(PIF_LINK_TX_ANSWER_SIZE);
    if (!p_owner->__last.p_answer) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }
    p_owner->__last.answer_size = PIF_LINK_TX_ANSWER_SIZE;

    p_owner->__tx.p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__tx.p_timer) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__tx.p_timer, _evtTimerTxTimeout, p_owner);

    p_owner->__p_questions = p_questions;
    p_owner->_id = id;
    p_owner->__packet_id = 0x20;
    p_owner->__rx.packet_size = RX_EXTRA_SIZE + PIF_LINK_RX_PACKET_SIZE;
    return TRUE;

fail:
	pifLink_Clear(p_owner);
    return FALSE;
}

void pifLink_Clear(PifLink* p_owner)
{
	if (p_owner->__rx.p_packet) {
		free(p_owner->__rx.p_packet);
		p_owner->__rx.p_packet = NULL;
	}
	pifRingBuffer_Clear(&p_owner->__tx.request_buffer);
	pifRingBuffer_Clear(&p_owner->__tx.answer_buffer);
	if (p_owner->__last.p_answer) {
		free(p_owner->__last.p_answer);
		p_owner->__last.p_answer = NULL;
	}
	p_owner->__last.valid = FALSE;
#if PIF_LINK_RECEIVE_TIMEOUT
	if (p_owner->__rx.p_timer) {
		pifTimerManager_Remove(p_owner->__rx.p_timer);
		p_owner->__rx.p_timer = NULL;
	}
#endif
	if (p_owner->__tx.p_timer) {
		pifTimerManager_Remove(p_owner->__tx.p_timer);
		p_owner->__tx.p_timer = NULL;
	}
}

BOOL pifLink_ResizeRxPacket(PifLink* p_owner, uint16_t rx_packet_size)
{
	uint8_t* p_packet;

    if (!rx_packet_size || rx_packet_size > PIF_LINK_MAX_DATA_SIZE) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_packet = realloc(p_owner->__rx.p_packet, sizeof(uint8_t) * (RX_EXTRA_SIZE + rx_packet_size));
    if (!p_packet) {
        pif_error = E_OUT_OF_HEAP;
	    return FALSE;
    }

    p_owner->__rx.p_packet = p_packet;
    p_owner->__rx.packet_size = RX_EXTRA_SIZE + rx_packet_size;
    p_owner->__rx.state = PRS_IDLE;
    return TRUE;
}

BOOL pifLink_ResizeTxRequest(PifLink* p_owner, uint16_t tx_request_size)
{
    if (!tx_request_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (p_owner->__tx.state != PTS_IDLE) pifTimer_Stop(p_owner->__tx.p_timer);
    p_owner->__tx.state = PTS_IDLE;
    return pifRingBuffer_ResizeHeap(&p_owner->__tx.request_buffer, tx_request_size);
}

BOOL pifLink_ResizeTxResponse(PifLink* p_owner, uint16_t tx_response_size)
{
    if (!tx_response_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    uint8_t* p_answer = realloc(p_owner->__last.p_answer, tx_response_size);
    if (!p_answer) {
        pif_error = E_OUT_OF_HEAP;
	    return FALSE;
    }
    p_owner->__last.p_answer = p_answer;
    p_owner->__last.answer_size = tx_response_size;
    p_owner->__last.valid = FALSE;
    p_owner->__tx.answer_length = 0;

    return pifRingBuffer_ResizeHeap(&p_owner->__tx.answer_buffer, tx_response_size);
}

void pifLink_AttachUart(PifLink* p_owner, PifUart* p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifLink_DetachUart(PifLink* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

static BOOL _checkFrame(PifLink* p_owner, uint8_t command, uint8_t* p_data, uint16_t data_size)
{
	if (!p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (command < 0x20 || (data_size && !p_data) ||
			data_size > PIF_LINK_MAX_DATA_SIZE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

// Puts the data, CRC, ETX and frame size padding after the header and returns the whole frame length.
// p_crc_pos gets the position of the CRC from the start of the frame.
static uint16_t _putFrameBody(PifLink* p_owner, PifRingBuffer* p_buffer, uint8_t* p_header, uint8_t* p_data,
		uint16_t data_size, uint16_t* p_crc, uint16_t* p_crc_pos)
{
	uint16_t i, length;
	uint16_t crc = CRC_INIT;
	uint8_t data, tailer[CRC_SIZE + 1];

	for (i = 1; i < p_owner->__header_size; i++) {
		crc = pifCrc16_Add(crc, p_header[i]);
	}
	length = p_owner->__header_size;

	for (i = 0; i < data_size; i++) {
		data = p_data[i];
		if (data < 0x20) {
			crc = pifCrc16_Add(crc, ASCII_DLE);
			if (!pifRingBuffer_PutByte(p_buffer, ASCII_DLE)) return 0;
			data |= 0x80;
			length++;
		}
		crc = pifCrc16_Add(crc, data);
		if (!pifRingBuffer_PutByte(p_buffer, data)) return 0;
		length++;
	}

	*p_crc_pos = length;
	_encodeCrc(crc, tailer);
	tailer[CRC_SIZE] = ASCII_ETX;
	if (!pifRingBuffer_PutData(p_buffer, tailer, sizeof(tailer))) return 0;
	length += sizeof(tailer);

	if (p_owner->__p_uart->_frame_size > 1) {
		while (length % p_owner->__p_uart->_frame_size) {
			if (!pifRingBuffer_PutByte(p_buffer, 0)) return 0;
			length++;
		}
	}
	*p_crc = crc;
	return length;
}

// Fills the header from flags to the length.
static void _makeHeader(PifLink* p_owner, uint8_t* p_header, uint8_t flags, uint8_t command, uint8_t packet_id,
		uint8_t dst_id, uint16_t data_size)
{
	uint8_t* p_length = p_header + p_owner->__header_size - 2;

	p_header[0] = ASCII_STX;
	p_header[1] = LF_ALWAYS | flags;
	p_header[2] = command;
	p_header[3] = packet_id;
	if (p_owner->_type == PLT_MULTI) {
		p_header[4] = 0x80 | p_owner->_address;
		p_header[5] = 0x80 | dst_id;
	}
	p_length[0] = 0x80 | (data_size & 0x7F);
	p_length[1] = 0x80 | ((data_size >> 7) & 0x7F);
}

BOOL pifLink_MakeRequest(PifLink* p_owner, uint8_t dst_id, const PifLinkRequest* p_request, uint8_t* p_data,
		uint16_t data_size)
{
	uint8_t flags;
	uint8_t info[REQUEST_INFO_SIZE];
	uint8_t header[PIF_LINK_MULTI_HEADER_SIZE];
	uint8_t packet_id;
	uint16_t length, crc, crc_pos;

	if (!p_request) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_checkFrame(p_owner, p_request->command, p_data, data_size)) return FALSE;
	if (p_owner->_type == PLT_MULTI) {
		if (dst_id > PIF_LINK_BROADCAST || dst_id == p_owner->_address) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}
	}
	else {
		dst_id = 0;
	}

	flags = LF_TYPE_REQUEST | (p_request->flags & (LF_RESPONSE_MASK | LF_LOG_PRINT_MASK));
	if (p_owner->_type == PLT_MULTI && dst_id == PIF_LINK_BROADCAST) flags |= LF_RESPONSE_NO;
	packet_id = p_owner->__packet_id;
	_makeHeader(p_owner, header, flags, p_request->command, packet_id, dst_id, data_size);

	// The frame length is filled in after the whole frame is put.
	pifRingBuffer_BeginPutting(&p_owner->__tx.request_buffer);
	memcpy(info + 4, &p_request, sizeof(p_request));
	if (!pifRingBuffer_PutData(&p_owner->__tx.request_buffer, info, REQUEST_INFO_SIZE)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.request_buffer, header, p_owner->__header_size)) goto fail;
	length = _putFrameBody(p_owner, &p_owner->__tx.request_buffer, header, p_data, data_size, &crc, &crc_pos);
	if (!length) goto fail;
	pifLink_PutU16(info, length);
	pifLink_PutU16(info + 2, crc_pos);
	for (int i = 0; i < 4; i++) {
		*pifRingBuffer_GetPointerPutting(&p_owner->__tx.request_buffer, i) = info[i];
	}
	pifRingBuffer_CommitPutting(&p_owner->__tx.request_buffer);

	p_owner->__packet_id++;
	if (!p_owner->__packet_id) p_owner->__packet_id = 0x20;

#ifndef PIF_NO_LOG
	if (p_request->flags & LF_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "LK(%u) Rq:%xh F:%xh P:%d L:%d=%d CRC:%xh", p_owner->_id, p_request->command,
				p_request->flags, packet_id, data_size, length, crc);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x %x : %x", p_owner->_id,
			header[0], header[1], header[2], header[3], header[4], header[5], crc);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.request_buffer);
	return FALSE;
}

static BOOL _makeAnswer(PifLink* p_owner, PifLinkPacket* p_question, uint8_t flags,
		uint8_t* p_data, uint16_t data_size)
{
	PifRingBuffer* p_buffer = &p_owner->__tx.answer_buffer;
	PifLinkLastQuestion* p_last = &p_owner->__last;
	uint8_t header[PIF_LINK_MULTI_HEADER_SIZE];
	uint8_t packet_id;
	uint16_t i, length, crc, crc_pos;

	if (!p_question) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_checkFrame(p_owner, p_question->command, p_data, data_size)) return FALSE;
	if (_isBroadcast(p_owner, p_question)) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	packet_id = p_question->packet_id;
	_makeHeader(p_owner, header, LF_TYPE_ANSWER | flags, p_question->command, packet_id, p_question->src_id, data_size);

	pifRingBuffer_BeginPutting(p_buffer);
	if (!pifRingBuffer_PutData(p_buffer, header, p_owner->__header_size)) goto fail;
	length = _putFrameBody(p_owner, p_buffer, header, p_data, data_size, &crc, &crc_pos);
	if (!length) goto fail;

	// Keep a copy to answer a retransmission of the same question.
	if (p_last->valid && p_last->command == p_question->command && p_last->packet_id == p_question->packet_id &&
			p_last->src_id == p_question->src_id) {
		if (length <= p_last->answer_size) {
			for (i = 0; i < length; i++) {
				p_last->p_answer[i] = *pifRingBuffer_GetPointerPutting(p_buffer, i);
			}
			p_last->answer_length = length;
		}
	}
	pifRingBuffer_CommitPutting(p_buffer);

#ifndef PIF_NO_LOG
	if (flags & LF_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "LK(%u) As:%xh F:%xh P:%d L:%d=%d CRC:%xh", p_owner->_id, p_question->command,
				header[1], packet_id, data_size, length, crc);
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x %x : %x", p_owner->_id,
			header[0], header[1], header[2], header[3], header[4], header[5], crc);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(p_buffer);
	return FALSE;
}

BOOL pifLink_MakeAnswer(PifLink* p_owner, PifLinkPacket* p_question, uint8_t flags,
		uint8_t* p_data, uint16_t data_size)
{
	return _makeAnswer(p_owner, p_question, flags & LF_LOG_PRINT_MASK, p_data, data_size);
}

BOOL pifLink_MakeError(PifLink* p_owner, PifLinkPacket* p_question, uint8_t flags, PifLinkError error)
{
	uint8_t code = error;

	if (code == LE_NONE || code >= 0x80) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return _makeAnswer(p_owner, p_question, LF_ERROR | (flags & LF_LOG_PRINT_MASK), &code, 1);
}

uint8_t* pifLink_PutU16(uint8_t* p_buffer, uint16_t value)
{
	p_buffer[0] = value & 0xFF;
	p_buffer[1] = (value >> 8) & 0xFF;
	return p_buffer + 2;
}

uint8_t* pifLink_PutU32(uint8_t* p_buffer, uint32_t value)
{
	p_buffer[0] = value & 0xFF;
	p_buffer[1] = (value >> 8) & 0xFF;
	p_buffer[2] = (value >> 16) & 0xFF;
	p_buffer[3] = (value >> 24) & 0xFF;
	return p_buffer + 4;
}

uint8_t* pifLink_PutFloat(uint8_t* p_buffer, float value)
{
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return pifLink_PutU32(p_buffer, bits);
}

uint16_t pifLink_GetU16(const uint8_t* p_buffer)
{
	return p_buffer[0] | ((uint16_t)p_buffer[1] << 8);
}

uint32_t pifLink_GetU32(const uint8_t* p_buffer)
{
	return p_buffer[0] | ((uint32_t)p_buffer[1] << 8) | ((uint32_t)p_buffer[2] << 16) | ((uint32_t)p_buffer[3] << 24);
}

float pifLink_GetFloat(const uint8_t* p_buffer)
{
	uint32_t bits = pifLink_GetU32(p_buffer);
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}
