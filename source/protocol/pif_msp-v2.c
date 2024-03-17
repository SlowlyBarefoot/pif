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
			p_owner->__rx.state, p_owner->__rx.packet.length);
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x", p_owner->__rx.p_packet[0], p_owner->__rx.p_packet[1],
			p_owner->__rx.p_packet[2], p_owner->__rx.p_packet[3], p_owner->__rx.p_packet[4], p_owner->__rx.p_packet[5],
			p_owner->__rx.p_packet[6], p_owner->__rx.p_packet[7]);
#endif
#endif
	pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, ASCII_NAK);
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

static void _parsingPacket(PifMsp *p_owner, PifActCommReceiveData act_receive_data)
{
	PifMspPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
	uint8_t pkt_err;
#ifndef PIF_NO_LOG
	int line;
#endif
	static uint8_t pre_error = PKT_ERR_NONE;

	while ((*act_receive_data)(p_owner->__p_comm, &data)) {
		switch (p_owner->__rx.state) {
		case MRS_IDLE:
			if (data == '$') {
				p_owner->__rx.state = MRS_HEADER_START;
#if PIF_MSP_RECEIVE_TIMEOUT
				pifTimer_Start(p_owner->__rx.p_timer, PIF_MSP_RECEIVE_TIMEOUT);
#endif
			}
			else if (pre_error == PKT_ERR_NONE && p_owner->evt_other_packet) {
				(*p_owner->evt_other_packet)(p_owner, data);
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_START:
			p_owner->__rx.offset = 0;
			p_owner->__rx.checksum1 = 0;
			p_owner->__rx.checksum2 = 0;
			if (data == 'M') {
				p_owner->__rx.state = MRS_HEADER_M;
				p_owner->__rx.version = MSP_V1;
			}
			else if (data == 'X') {
				p_owner->__rx.state = MRS_HEADER_X;
				p_owner->__rx.version = MSP_V2_NATIVE;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_M:
			p_owner->__rx.state = MRS_HEADER_V1;
			if (data == '<') {
				p_owner->__rx.type = MPT_COMMAND;
			}
			else if (data == '>') {
				p_owner->__rx.type = MPT_REPLY;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_X:
			p_owner->__rx.state = MRS_HEADER_V2_NATIVE;
			if (data == '<') {
				p_owner->__rx.type = MPT_COMMAND;
			}
			else if (data == '>') {
				p_owner->__rx.type = MPT_REPLY;
			}
			else {
				pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_V1:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum1 ^= data;
			if (p_owner->__rx.offset == sizeof(PifMspHeaderV1)) {
				PifMspHeaderV1* p_header = (PifMspHeaderV1*)p_owner->__rx.p_packet;
				if (p_header->size > PIF_MSP_RX_PACKET_SIZE) {
					pkt_err = PKT_ERR_BIG_LENGHT;
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
				else if (p_header->cmd == 255) {	// 255 : MSP_V2_FRAME_ID
					if (p_header->size >= sizeof(PifMspHeaderV2) + 1) {
						p_owner->__rx.state = MRS_HEADER_V2_OVER_V1;
						p_owner->__rx.version = MSP_V2_OVER_V1;
					}
					else {
						pkt_err = PKT_ERR_INVALID_DATA;
#ifndef PIF_NO_LOG
						line = __LINE__;
#endif
						goto fail;
					}
				}
				else {
					p_packet->flags = 0;
					p_packet->length = p_header->size;
					p_packet->command = p_header->cmd;
					p_owner->__rx.offset = 0;
					p_owner->__rx.state = p_packet->length > 0 ? MRS_PAYLOAD_V1 : MRS_CHECKSUM_V1;
				}
			}
			break;

		case MRS_PAYLOAD_V1:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum1 ^= data;
			if (p_owner->__rx.offset == p_packet->length) {
				p_owner->__rx.state = MRS_CHECKSUM_V1;
			}
			break;

		case MRS_CHECKSUM_V1:
			if (p_owner->__rx.checksum1 == data) {
#if PIF_MSP_RECEIVE_TIMEOUT
				pifTimer_Stop(p_owner->__rx.p_timer);
#endif
				p_packet->p_payload = p_owner->__rx.p_packet;
				p_owner->__rx.state = MRS_DONE;
			}
			else {
				pkt_err = PKT_ERR_WRONG_CRC;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_V2_OVER_V1:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum1 ^= data;
			p_owner->__rx.checksum2 = pifCrc8_Add(p_owner->__rx.checksum2, data, 0xD5);
			if (p_owner->__rx.offset == sizeof(PifMspHeaderV1) + sizeof(PifMspHeaderV2)) {
				PifMspHeaderV2* p_header = (PifMspHeaderV2*)(p_owner->__rx.p_packet + sizeof(PifMspHeaderV1));
				if (p_header->size > PIF_MSP_RX_PACKET_SIZE) {
					pkt_err = PKT_ERR_BIG_LENGHT;
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
				else {
					p_packet->flags = p_header->flags;
					p_packet->length = p_header->size;
					p_packet->command = p_header->cmd;
					p_owner->__rx.offset = 0;
					p_owner->__rx.state = p_packet->length > 0 ? MRS_PAYLOAD_V2_OVER_V1 : MRS_CHECKSUM_V2_OVER_V1;
				}
			}
			break;

		case MRS_PAYLOAD_V2_OVER_V1:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum1 ^= data;
			p_owner->__rx.checksum2 = pifCrc8_Add(p_owner->__rx.checksum2, data, 0xD5);
			if (p_owner->__rx.offset == p_packet->length) {
				p_owner->__rx.state = MRS_CHECKSUM_V2_OVER_V1;
			}
			break;

		case MRS_CHECKSUM_V2_OVER_V1:
			p_owner->__rx.checksum1 ^= data;
			if (p_owner->__rx.checksum2 == data) {
				p_owner->__rx.state = MRS_CHECKSUM_V1;
			}
			else {
				pkt_err = PKT_ERR_WRONG_CRC;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case MRS_HEADER_V2_NATIVE:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum2 = pifCrc8_Add(p_owner->__rx.checksum2, data, 0xD5);
			if (p_owner->__rx.offset == sizeof(PifMspHeaderV2)) {
				PifMspHeaderV2* p_header = (PifMspHeaderV2*)p_owner->__rx.p_packet;
				p_packet->flags = p_header->flags;
				p_packet->command = p_header->cmd;
				p_packet->length = p_header->size;
				p_owner->__rx.offset = 0;
				p_owner->__rx.state = p_packet->length > 0 ? MRS_PAYLOAD_V2_NATIVE : MRS_CHECKSUM_V2_NATIVE;
			}
			break;

		case MRS_PAYLOAD_V2_NATIVE:
			p_owner->__rx.p_packet[p_owner->__rx.offset++] = data;
			p_owner->__rx.checksum2 = pifCrc8_Add(p_owner->__rx.checksum2, data, 0xD5);
			if (p_owner->__rx.offset == p_packet->length) {
				p_owner->__rx.state = MRS_CHECKSUM_V2_NATIVE;
			}
			break;

		case MRS_CHECKSUM_V2_NATIVE:
			if (p_owner->__rx.checksum2 == data) {
#if PIF_MSP_RECEIVE_TIMEOUT
				pifTimer_Stop(p_owner->__rx.p_timer);
#endif
				p_packet->p_payload = p_owner->__rx.p_packet;
				p_owner->__rx.state = MRS_DONE;
			}
			else {
				pkt_err = PKT_ERR_WRONG_CRC;
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
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
				p_owner->__rx.state, p_packet->length);
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

static void _evtParsing(void *p_client, PifActCommReceiveData act_receive_data)
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

    	switch (p_owner->__rx.type) {
    	case MPT_COMMAND:
        	if (p_owner->evt_receive) (*p_owner->evt_receive)(p_owner, &p_owner->__rx.packet);
        	if (p_owner->__p_comm->_p_task) p_owner->__p_comm->_p_task->immediate = TRUE;
    		break;

    	case MPT_REPLY:
    		break;
    	}

    	p_owner->__rx.state = MRS_IDLE;
    }
}

static uint16_t _evtSending(void *p_client, PifActCommSendData act_send_data)
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
			if (p_owner->__p_comm->_p_task) p_owner->__p_comm->_p_task->immediate = TRUE;
		}
		break;

	case MTS_SENDING:
		length = (*act_send_data)(p_owner->__p_comm, pifRingBuffer_GetTailPointer(&p_owner->__tx.answer_buffer, p_owner->__tx.pos),
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

void pifMsp_AttachComm(PifMsp* p_owner, PifComm *p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner, _evtParsing, _evtSending);
}

void pifMsp_DetachComm(PifMsp* p_owner)
{
	pifComm_DetachClient(p_owner->__p_comm);
	p_owner->__p_comm = NULL;
}

BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size)
{
	uint8_t header[5], check_xor;

	pifRingBuffer_BackupHead(&p_owner->__tx.answer_buffer);

	header[0] = '$';
	header[1] = 'M';
	header[2] = '>';
	header[3] = data_size;
	header[4] = p_question->command;
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, header, 5)) goto fail;
	check_xor = header[3] ^ header[4];
	if (data_size > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, p_data, data_size)) goto fail;
		check_xor ^= pifCheckXor(p_data, data_size);
	}
	if (!pifRingBuffer_PutByte(&p_owner->__tx.answer_buffer, check_xor)) goto fail;
	p_owner->__p_comm->_p_task->immediate = TRUE;

#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x : %x", p_owner->_id,
			header[0], header[1], header[2], header[3], header[4], check_xor);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) C:%u D:%u EC:%d", __LINE__, p_owner->_id, p_question->command, data_size, pif_error);
#endif
	return FALSE;
}

BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question)
{
	uint8_t header[6];

	pifRingBuffer_BackupHead(&p_owner->__tx.answer_buffer);

	header[0] = '$';
	header[1] = 'M';
	header[2] = '!';
	header[3] = 0;
	header[4] = p_question->command;
	header[5] = header[3] ^ header[4];
	if (!pifRingBuffer_PutData(&p_owner->__tx.answer_buffer, header, 6)) goto fail;
	p_owner->__p_comm->_p_task->immediate = TRUE;

#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u< %x %x %x %x %x : %x", p_owner->_id,
			header[0], header[1], header[2], header[3], header[4], header[5]);
#endif
#endif
	return TRUE;

fail:
	pifRingBuffer_RestoreHead(&p_owner->__tx.answer_buffer);
	if (!pif_error) pif_error = E_OVERFLOW_BUFFER;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) C:%u EC:%d", __LINE__, p_owner->_id, p_question->command, pif_error);
#endif
	return FALSE;
}
