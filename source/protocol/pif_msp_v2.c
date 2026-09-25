#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_msp_v2.h"

#include <string.h>


#define PKT_ERR_BIG_LENGTH		0
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

/**
 * @brief Updates a CRC-8/DVB-S2 (polynomial 0xD5), the checksum of MSPv2.
 * @param crc CRC so far.
 * @param p_data Bytes to add.
 * @param length Number of bytes.
 * @return Updated CRC.
 */
static uint8_t _crc8DvbS2(uint8_t crc, const uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint8_t bit;

	for (i = 0; i < length; i++) {
		crc ^= p_data[i];
		for (bit = 0; bit < 8; bit++) {
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
		}
	}
	return crc;
}

/**
 * @brief Drops the packet being received and logs why, once per run of the same error.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param pkt_err One of PKT_ERR_*.
 * @param line Source line that found the error.
 * @param data Byte that caused the error.
 * @return MF_NONE, for the parser to return.
 */
static PifMspFrame _failPacket(PifMspV2* p_owner, uint8_t pkt_err, int line, uint8_t data)
{
	PifMsp* p_parent = &p_owner->_msp;

	if (pkt_err != p_parent->__rx.pre_error) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MSP2:%u(%u) %s D:%xh RS:%u Cnt:%u", line, p_parent->_id, kPktErr[pkt_err], data,
				p_parent->__rx.state, p_parent->__rx.packet.data_count);
#else
		(void)line;
		(void)data;
#endif
		p_parent->__rx.pre_error = pkt_err;
	}

#if PIF_MSP_RECEIVE_TIMEOUT
	pifTimer_Stop(p_parent->__rx.p_timer);
#endif
	p_parent->__rx.state = MV2RS_IDLE;
	return MF_NONE;
}

/**
 * @brief Sets up the payload of the packet whose header has just been read.
 * @param p_owner Pointer to the protocol instance.
 * @param command Command of the packet.
 * @param flags MSPv2 flags of the packet.
 * @param size Payload length.
 * @param payload_state State that reads the payload.
 * @param checksum_state State that reads the checksum, for a packet with no payload.
 * @return FALSE if the payload does not fit in the receive buffer.
 */
static BOOL _startPayload(PifMspV2* p_owner, uint16_t command, uint8_t flags, uint16_t size,
		PifMspV2RxState payload_state, PifMspV2RxState checksum_state)
{
	PifMsp* p_parent = &p_owner->_msp;

	if (size > p_parent->__rx.packet_size) return FALSE;

	p_parent->__rx.packet.command = command;
	p_parent->__rx.packet.flags = flags;
	p_parent->__rx.packet.data_count = size;
	p_owner->__offset = 0;
	p_parent->__rx.state = size > 0 ? payload_state : checksum_state;
	return TRUE;
}

/**
 * @brief Runs one received byte through the MSPv1 / MSPv2 parser that replaces the one of pif_msp.
 * @param p_parser The PifMspV2 instance.
 * @param data Received byte.
 * @return What the byte did.
 */
static PifMspFrame _parsingPacket(void* p_parser, uint8_t data)
{
	PifMspV2* p_owner = (PifMspV2*)p_parser;
	PifMsp* p_parent = &p_owner->_msp;
	PifMspPacket* p_packet = &p_parent->__rx.packet;

	switch (p_parent->__rx.state) {
	case MV2RS_IDLE:
		if (data != '$') return MF_OTHER;

		p_parent->__rx.state = MV2RS_HEADER_START;
#if PIF_MSP_RECEIVE_TIMEOUT
		pifTimer_Start(p_parent->__rx.p_timer, PIF_MSP_RECEIVE_TIMEOUT);
#endif
		break;

	case MV2RS_HEADER_START:
		p_owner->__offset = 0;
		p_owner->__checksum1 = 0;
		p_owner->__checksum2 = 0;
		switch (data) {
		case 'M':
			p_packet->version = MV_V1;
			p_parent->__rx.state = MV2RS_HEADER_M;
			break;

		case 'X':
			p_packet->version = MV_V2_NATIVE;
			p_parent->__rx.state = MV2RS_HEADER_X;
			break;

		default:
			return _failPacket(p_owner, PKT_ERR_INVALID_DATA, __LINE__, data);
		}
		break;

	case MV2RS_HEADER_M:
	case MV2RS_HEADER_X:
		switch (data) {
		case '<':
			p_packet->type = MPT_COMMAND;
			break;

		case '>':
			p_packet->type = MPT_REPLY;
			break;

		default:
			return _failPacket(p_owner, PKT_ERR_INVALID_DATA, __LINE__, data);
		}
		p_parent->__rx.state = p_parent->__rx.state == MV2RS_HEADER_M ? MV2RS_HEADER_V1 : MV2RS_HEADER_V2_NATIVE;
		break;

	case MV2RS_HEADER_V1:
		// Size and command, which the MSPv1 checksum already covers.
		p_owner->__header[p_owner->__offset++] = data;
		p_owner->__checksum1 ^= data;
		if (p_owner->__offset == 2) {
			if (p_owner->__header[1] == PIF_MSP_V2_FRAME_ID) {
				// The MSPv1 payload has to hold the MSPv2 header and its checksum.
				if (p_owner->__header[0] < 5 + 1) {
					return _failPacket(p_owner, PKT_ERR_INVALID_DATA, __LINE__, data);
				}
				p_packet->version = MV_V2_OVER_V1;
				p_parent->__rx.state = MV2RS_HEADER_V2_OVER_V1;
			}
			else if (!_startPayload(p_owner, p_owner->__header[1], 0, p_owner->__header[0], MV2RS_PAYLOAD_V1, MV2RS_CHECKSUM_V1)) {
				return _failPacket(p_owner, PKT_ERR_BIG_LENGTH, __LINE__, data);
			}
		}
		break;

	case MV2RS_PAYLOAD_V1:
		p_parent->__rx.p_packet[p_owner->__offset++] = data;
		p_owner->__checksum1 ^= data;
		if (p_owner->__offset == p_packet->data_count) {
			p_parent->__rx.state = MV2RS_CHECKSUM_V1;
		}
		break;

	case MV2RS_CHECKSUM_V1:
		if (p_owner->__checksum1 != data) {
			return _failPacket(p_owner, PKT_ERR_WRONG_CRC, __LINE__, data);
		}
		goto done;

	case MV2RS_HEADER_V2_OVER_V1:
		// The MSPv2 header is MSPv1 payload, so both checksums take it.
		p_owner->__header[p_owner->__offset++] = data;
		p_owner->__checksum1 ^= data;
		p_owner->__checksum2 = _crc8DvbS2(p_owner->__checksum2, &data, 1);
		if (p_owner->__offset == 2 + 5) {
			if (!_startPayload(p_owner, p_owner->__header[3] | (p_owner->__header[4] << 8), p_owner->__header[2],
					p_owner->__header[5] | (p_owner->__header[6] << 8), MV2RS_PAYLOAD_V2_OVER_V1, MV2RS_CHECKSUM_V2_OVER_V1)) {
				return _failPacket(p_owner, PKT_ERR_BIG_LENGTH, __LINE__, data);
			}
		}
		break;

	case MV2RS_PAYLOAD_V2_OVER_V1:
		p_parent->__rx.p_packet[p_owner->__offset++] = data;
		p_owner->__checksum1 ^= data;
		p_owner->__checksum2 = _crc8DvbS2(p_owner->__checksum2, &data, 1);
		if (p_owner->__offset == p_packet->data_count) {
			p_parent->__rx.state = MV2RS_CHECKSUM_V2_OVER_V1;
		}
		break;

	case MV2RS_CHECKSUM_V2_OVER_V1:
		// The MSPv2 checksum is the last MSPv1 payload byte; the MSPv1 checksum follows.
		p_owner->__checksum1 ^= data;
		if (p_owner->__checksum2 != data) {
			return _failPacket(p_owner, PKT_ERR_WRONG_CRC, __LINE__, data);
		}
		p_parent->__rx.state = MV2RS_CHECKSUM_V1;
		break;

	case MV2RS_HEADER_V2_NATIVE:
		p_owner->__header[p_owner->__offset++] = data;
		p_owner->__checksum2 = _crc8DvbS2(p_owner->__checksum2, &data, 1);
		if (p_owner->__offset == 5) {
			if (!_startPayload(p_owner, p_owner->__header[1] | (p_owner->__header[2] << 8), p_owner->__header[0],
					p_owner->__header[3] | (p_owner->__header[4] << 8), MV2RS_PAYLOAD_V2_NATIVE, MV2RS_CHECKSUM_V2_NATIVE)) {
				return _failPacket(p_owner, PKT_ERR_BIG_LENGTH, __LINE__, data);
			}
		}
		break;

	case MV2RS_PAYLOAD_V2_NATIVE:
		p_parent->__rx.p_packet[p_owner->__offset++] = data;
		p_owner->__checksum2 = _crc8DvbS2(p_owner->__checksum2, &data, 1);
		if (p_owner->__offset == p_packet->data_count) {
			p_parent->__rx.state = MV2RS_CHECKSUM_V2_NATIVE;
		}
		break;

	case MV2RS_CHECKSUM_V2_NATIVE:
		if (p_owner->__checksum2 != data) {
			return _failPacket(p_owner, PKT_ERR_WRONG_CRC, __LINE__, data);
		}
		goto done;

	default:
		p_parent->__rx.state = MV2RS_IDLE;
		break;
	}
	p_parent->__rx.pre_error = PKT_ERR_NONE;
	return MF_NONE;

done:
#if PIF_MSP_RECEIVE_TIMEOUT
	pifTimer_Stop(p_parent->__rx.p_timer);
#endif
	p_packet->p_data = p_parent->__rx.p_packet;
	if (p_packet->version == MV_V1) p_packet->flags = 0;
	p_parent->__rx.pre_error = PKT_ERR_NONE;
	p_parent->__rx.state = MV2RS_IDLE;
	return MF_PACKET;
}

BOOL pifMspV2_Init(PifMspV2* p_owner, PifTimerManager* p_timer, PifId id)
{
	memset(p_owner, 0, sizeof(PifMspV2));

	if (!pifMsp_Init(&p_owner->_msp, p_timer, id)) return FALSE;

	p_owner->_msp.__p_parser = p_owner;
	p_owner->_msp.__act_parsing = _parsingPacket;
	return TRUE;
}

void pifMspV2_Clear(PifMspV2* p_owner)
{
	pifMsp_Clear(&p_owner->_msp);
}

void pifMspV2_AttachUart(PifMspV2* p_owner, PifUart* p_uart)
{
	pifMsp_AttachUart(&p_owner->_msp, p_uart);
}

void pifMspV2_DetachUart(PifMspV2* p_owner)
{
	pifMsp_DetachUart(&p_owner->_msp);
}

PifMspFrame pifMspV2_ParsingPacket(PifMspV2* p_owner, uint8_t data)
{
	return pifMsp_ParsingPacket(&p_owner->_msp, data);
}

uint16_t pifMspV2_GetFrameSize(PifMspVersion version, uint16_t data_size)
{
	uint32_t v1_size;

	switch (version) {
	case MV_V1:
		v1_size = data_size;
		break;

	case MV_V2_OVER_V1:
		v1_size = 5 + data_size + 1;
		break;

	case MV_V2_NATIVE:
		return 3 + 5 + data_size + 1;

	default:
		return 0;
	}
	return 3 + 2 + (v1_size >= PIF_MSP_V1_JUMBO_SIZE ? 2 : 0) + v1_size + 1;
}

BOOL pifMspV2_MakePacket(PifMspV2* p_owner, PifMspVersion version, uint8_t direction, uint8_t flags, uint16_t command,
		uint8_t* p_data, uint16_t data_size)
{
	PifMsp* p_parent = &p_owner->_msp;
	PifRingBuffer* p_buffer = &p_parent->__tx.answer_buffer;
	uint8_t header[PIF_MSP_V2_MAX_OVERHEAD - 2];
	uint8_t trailer[2];
	uint8_t header_size = 3, trailer_size = 0, v2_pos;
	uint32_t v1_size;

	if (!pifRingBuffer_IsBuffer(p_buffer) || (data_size && !p_data)) {
		pif_error = E_INVALID_STATE;
		goto fail;
	}

	header[0] = '$';
	header[1] = version == MV_V2_NATIVE ? 'X' : 'M';
	header[2] = direction;

	if (version != MV_V2_NATIVE) {
		v1_size = version == MV_V1 ? data_size : 5 + data_size + 1;
		header[3] = v1_size >= PIF_MSP_V1_JUMBO_SIZE ? PIF_MSP_V1_JUMBO_SIZE : v1_size;
		header[4] = version == MV_V1 ? (uint8_t)command : PIF_MSP_V2_FRAME_ID;
		header_size = 5;
		if (v1_size >= PIF_MSP_V1_JUMBO_SIZE) {
			header[header_size++] = v1_size & 0xFF;
			header[header_size++] = v1_size >> 8;
		}
	}

	if (version != MV_V1) {
		v2_pos = header_size;
		header[header_size++] = flags;
		header[header_size++] = command & 0xFF;
		header[header_size++] = command >> 8;
		header[header_size++] = data_size & 0xFF;
		header[header_size++] = data_size >> 8;

		// Covers the MSPv2 header and the payload.
		trailer[trailer_size] = _crc8DvbS2(0, &header[v2_pos], 5);
		if (data_size) trailer[trailer_size] = _crc8DvbS2(trailer[trailer_size], p_data, data_size);
		trailer_size++;
	}

	if (version != MV_V2_NATIVE) {
		// Covers everything after the direction, the MSPv2 checksum included.
		trailer[trailer_size] = pifCheckXor(&header[3], header_size - 3);
		if (data_size) trailer[trailer_size] ^= pifCheckXor(p_data, data_size);
		if (trailer_size) trailer[trailer_size] ^= trailer[0];
		trailer_size++;
	}

	pifRingBuffer_BeginPutting(p_buffer);
	if (!pifRingBuffer_PutData(p_buffer, header, header_size)) goto rollback;
	if (data_size && !pifRingBuffer_PutData(p_buffer, p_data, data_size)) goto rollback;
	if (!pifRingBuffer_PutData(p_buffer, trailer, trailer_size)) goto rollback;
	pifRingBuffer_CommitPutting(p_buffer);

	if (p_parent->__p_uart && p_parent->__p_uart->_p_tx_task) {
		pifTask_SetTrigger(p_parent->__p_uart->_p_tx_task, 0);
	}
	return TRUE;

rollback:
	pifRingBuffer_RollbackPutting(p_buffer);
	pif_error = E_OVERFLOW_BUFFER;
fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MSP2:%u(%u) C:%u L:%u EC:%d", __LINE__, p_parent->_id, command, data_size, pif_error);
#endif
	return FALSE;
}

BOOL pifMspV2_MakeAnswer(PifMspV2* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size)
{
	return pifMspV2_MakePacket(p_owner, p_question->version, '>', 0, p_question->command, p_data, data_size);
}

BOOL pifMspV2_MakeError(PifMspV2* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size)
{
	return pifMspV2_MakePacket(p_owner, p_question->version, '!', 0, p_question->command, p_data, data_size);
}
