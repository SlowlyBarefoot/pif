#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_msp.h"

#include <string.h>


#if PIF_MSP_RECEIVE_TIMEOUT


static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	if (!p_issuer) {
		pif_error = E_INVALID_PARAM;
		return;
	}

	PifMsp* p_owner = (PifMsp *)p_issuer;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) ParsingPacket(Timeout) State:%u Cnt:%u", __LINE__, p_owner->_id,
			p_owner->__rx.state, p_owner->__rx.packet.data_count);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7]);
#endif
#endif
	p_owner->__rx.state = MRS_IDLE;
}

#endif

#define PKT_ERR_BIG_LENGHT		0
#define PKT_ERR_INVALID_DATA    1
#define PKT_ERR_WRONG_CRC    	2
#define PKT_ERR_NONE	    	3

#ifndef PIF_NO_LOG

static const char *kPktErr[] = {
		"Big Length",
		"Invalid Data",
		"Wrong CRC"
};

#endif

static void _parsingPacket(PifMsp *p_owner, PifActUartReceiveData act_receive_data)
{
	PifMspPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
	uint8_t pkt_err;
#ifndef PIF_NO_LOG
	int line;
#endif
	static uint8_t pre_error = PKT_ERR_NONE;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx.state) {
		case MRS_IDLE:
			if (data == '$') {
				p_owner->__rx.state = MRS_HEADER_CHAR_1;
#if PIF_MSP_RECEIVE_TIMEOUT
				pifTimer_Start(p_owner->__rx.p_timer, PIF_MSP_RECEIVE_TIMEOUT);
#endif
			}
			else if (pre_error == PKT_ERR_NONE && p_owner->__evt_other_packet) {
				(*p_owner->__evt_other_packet)(p_owner, data, p_owner->__p_issuer);
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_CHAR_1:
			if (data == 'M') {
				p_owner->__rx.state = MRS_HEADER_CHAR_2;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_CHAR_2:
			if (data == '<') {
				p_owner->__rx.state = MRS_DIRECTION;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_DIRECTION:
			if (data < PIF_MSP_RX_PACKET_SIZE - 3) {
				p_packet->data_count = data;
				p_owner->__rx.p_packet[0] = data;
				p_owner->__rx.packet_count = 1;
				p_owner->__rx.state = MRS_LENGTH;
			}
			else {
				pkt_err = PKT_ERR_BIG_LENGHT;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_LENGTH:
			p_packet->command = data;
			p_owner->__rx.p_packet[p_owner->__rx.packet_count] = data;
			p_owner->__rx.packet_count++;
			p_owner->__rx.state = MRS_MESSAGE_TYPE;
			break;

		case MRS_MESSAGE_TYPE:
			p_owner->__rx.p_packet[p_owner->__rx.packet_count] = data;
			p_owner->__rx.packet_count++;
			if (p_owner->__rx.packet_count >= 3 + p_packet->data_count) {
				if (data == pifCheckXor(p_owner->__rx.p_packet, 2 + p_packet->data_count)) {
#if PIF_MSP_RECEIVE_TIMEOUT
					pifTimer_Stop(p_owner->__rx.p_timer);
#endif
					p_packet->p_data = p_owner->__rx.p_packet + 2;
					p_owner->__rx.state = MRS_DONE;
					return;
				}
				else {
					pkt_err = PKT_ERR_WRONG_CRC;
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
			}
			break;

		default:
			break;
		}
	}
	pre_error = PKT_ERR_NONE;
	return;

fail:
	if (pkt_err != pre_error) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MWP:%u(%u) %s D:%xh RS:%u Cnt:%u", line, p_owner->_id, kPktErr[pkt_err], data,
				p_owner->__rx.state, p_packet->data_count);
#endif
		pre_error = pkt_err;
	}
#if !defined( PIF_NO_LOG) && defined(__DEBUG_PACKET__)
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1], p_owner->__rx.p_packet[2],
			p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4]);
#endif

#if PIF_MSP_RECEIVE_TIMEOUT
   	pifTimer_Stop(p_owner->__rx.p_timer);
#endif
	p_owner->__rx.state = MRS_IDLE;
}

static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifMsp *p_owner = (PifMsp *)p_client;

    if (p_owner->__rx.state < MRS_DONE) {
    	_parsingPacket(p_owner, act_receive_data);
    }

    if (p_owner->__rx.state == MRS_DONE) {
#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x", p_owner->_id, p_owner->__rx.p_packet[0],	p_owner->__rx.p_packet[1],
    			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4]);
#endif
#endif

		p_owner->__rx.packet.p_pointer = p_owner->__rx.packet.p_data;
    	if (p_owner->__evt_receive) (*p_owner->__evt_receive)(p_owner, &p_owner->__rx.packet, p_owner->__p_issuer);
    	pifTask_SetTrigger(p_owner->__p_uart->_p_task);
    	p_owner->__rx.state = MRS_IDLE;
    }
}

static uint16_t _evtSending(void *p_client, PifActUartSendData act_send_data)
{
	PifMsp *p_owner = (PifMsp *)p_client;
	uint16_t length;

	if (p_owner->__rx.state != MRS_IDLE) return 0;

	switch (p_owner->__tx.state) {
	case MTS_IDLE:
		if (!pifRingBuffer_IsEmpty(&p_owner->__tx.answer_buffer)) {
			p_owner->__tx.length = pifRingBuffer_GetFillSize(&p_owner->__tx.answer_buffer);
			p_owner->__tx.pos = 0;
			p_owner->__tx.state = MTS_SENDING;
			pifTask_SetTrigger(p_owner->__p_uart->_p_task);
		}
		break;

	case MTS_SENDING:
		length = (*act_send_data)(p_owner->__p_uart, pifRingBuffer_GetTailPointer(&p_owner->__tx.answer_buffer, p_owner->__tx.pos),
				pifRingBuffer_GetLinerSize(&p_owner->__tx.answer_buffer, p_owner->__tx.pos));
		if (!length) return 0;

		p_owner->__tx.pos += length;
		if (p_owner->__tx.pos >= p_owner->__tx.length) {
			pifRingBuffer_Remove(&p_owner->__tx.answer_buffer, p_owner->__tx.pos);
			p_owner->__tx.state = MTS_IDLE;
		}
		break;

	default:
		break;
	}
	return 0;
}

BOOL pifMsp_Init(PifMsp* p_owner, PifTimerManager* p_timer, PifId id)
{
    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

	memset(p_owner, 0, sizeof(PifMsp));

    p_owner->__rx.p_packet = calloc(sizeof(uint8_t), PIF_MSP_RX_PACKET_SIZE);
    if (!p_owner->__rx.p_packet) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.answer_buffer, PIF_ID_AUTO, PIF_MSP_TX_ANSWER_SIZE)) goto fail;

#if PIF_MSP_RECEIVE_TIMEOUT
    p_owner->__rx.p_timer = pifTimerManager_Add(p_timer, TT_ONCE);
    if (!p_owner->__rx.p_timer) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__rx.p_timer, _evtTimerRxTimeout, p_owner);
#endif

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifMsp_Clear(p_owner);
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, id, pif_error);
#endif
    return FALSE;
}

void pifMsp_Clear(PifMsp* p_owner)
{
	if (p_owner->__rx.p_packet) {
		free(p_owner->__rx.p_packet);
		p_owner->__rx.p_packet = NULL;
	}
	pifRingBuffer_Clear(&p_owner->__tx.answer_buffer);
#if PIF_MSP_RECEIVE_TIMEOUT
	if (p_owner->__rx.p_timer) {
		pifTimerManager_Remove(p_owner->__rx.p_timer);
	}
#endif
}

void pifMsp_AttachUart(PifMsp* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifMsp_DetachUart(PifMsp* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

void pifMsp_AttachEvtReceive(PifMsp* p_owner, PifEvtMspReceive evt_receive, PifEvtMspOtherPacket evt_other_packet, PifIssuerP p_issuer)
{
	p_owner->__evt_receive = evt_receive;
	p_owner->__evt_other_packet = evt_other_packet;
	p_owner->__p_issuer = p_issuer;
}

uint8_t pifMsp_ReadData8(PifMspPacket* p_packet)
{
	uint8_t data;

	data = p_packet->p_pointer[0];
	p_packet->p_pointer++;
	return data;
}

uint16_t pifMsp_ReadData16(PifMspPacket* p_packet)
{
	uint16_t data;

	data = p_packet->p_pointer[0] | (p_packet->p_pointer[1] << 8);
	p_packet->p_pointer += 2;
	return data;
}

uint32_t pifMsp_ReadData32(PifMspPacket* p_packet)
{
	uint32_t data;

	data = p_packet->p_pointer[0] | (p_packet->p_pointer[1] << 8) | (p_packet->p_pointer[2] << 16) | (p_packet->p_pointer[3] << 24);
	p_packet->p_pointer += 4;
	return data;
}

void pifMsp_ReadData(PifMspPacket* p_packet, uint8_t* p_data, uint16_t size)
{
	uint16_t i;

	for (i = 0; i < size; i++) {
		p_data[i] = p_packet->p_pointer[i];
	}
	p_packet->p_pointer += size;
}

BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question)
{
	uint8_t header[5];

	pifRingBuffer_BeginPutting(&p_owner->__tx.answer_buffer);

	header[0] = '$';
	header[1] = 'M';
	header[2] = '>';
	header[3] = 0;
	header[4] = p_question->command;
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, header, 5)) goto fail;
	p_owner->__check_xor = header[4];
	p_owner->__data_size = 0;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) C:%u EC:%d", __LINE__, p_owner->_id, p_question->command, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_AddAnswer8(PifMsp* p_owner, uint8_t data)
{
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, &data, 1)) goto fail;
	p_owner->__check_xor ^= data;
	p_owner->__data_size += 1;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, p_owner->_id, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_AddAnswer16(PifMsp* p_owner, uint16_t data)
{
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, (uint8_t*)&data, 2)) goto fail;
	p_owner->__check_xor ^= pifCheckXor((uint8_t*)&data, 2);
	p_owner->__data_size += 2;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, p_owner->_id, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_AddAnswer32(PifMsp* p_owner, uint32_t data)
{
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, (uint8_t*)&data, 4)) goto fail;
	p_owner->__check_xor ^= pifCheckXor((uint8_t*)&data, 4);
	p_owner->__data_size += 4;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, p_owner->_id, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_AddAnswer(PifMsp* p_owner, uint8_t* p_data, uint16_t size)
{
	if (size > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, p_data, size)) goto fail;
		p_owner->__check_xor ^= pifCheckXor(p_data, size);
		p_owner->__data_size += size;
	}
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, p_owner->_id, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question)
{
	uint8_t header[5];

	pifRingBuffer_BeginPutting(&p_owner->__tx.answer_buffer);

	header[0] = '$';
	header[1] = 'M';
	header[2] = '!';
	header[3] = 0;
	header[4] = p_question->command;
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, header, 5)) goto fail;
	p_owner->__check_xor = header[4];
	p_owner->__data_size = 0;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) C:%u EC:%d", __LINE__, p_owner->_id, p_question->command, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_SendAnswer(PifMsp* p_owner)
{
	*pifRingBuffer_GetPointerPutting(&p_owner->__tx.answer_buffer, 3) = p_owner->__data_size;
	p_owner->__check_xor ^= p_owner->__data_size;
	if (!pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, p_owner->__check_xor)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.answer_buffer);

	pifTask_SetTrigger(p_owner->__p_uart->_p_task);
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, p_owner->_id, pif_error);
#endif
	return FALSE;
}
