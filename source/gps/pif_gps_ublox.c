// SPDX-License-Identifier: BSD-3-Clause
//
// The UBX side of a u-blox receiver: the frame of the protocol, its 8 bit Fletcher checksum, the NAV
// messages that carry a fix, and the commands with the ACK or NAK that answers them.

#include <string.h>

#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "gps/pif_gps_ublox.h"


#define UBX_SYNC_CHAR_1			0xB5
#define UBX_SYNC_CHAR_2			0x62

// Bytes of the length that heads an entry of the transmit buffer.
#define TX_ENTRY_HEADER_SIZE	2

/// Fix types of NAV-SOL, NAV-STATUS and NAV-PVT.
enum {
    FIX_NONE = 0,
    FIX_DEAD_RECKONING = 1,
    FIX_2D = 2,
    FIX_3D = 3,
    FIX_GPS_DEAD_RECKONING = 4,
    FIX_TIME = 5
};

/// Bit 0 of the flags of NAV-SOL and NAV-STATUS: the fix is valid and within the limits set.
#define NAV_STATUS_FIX_VALID	0x01

/// Bit 0 of the flags of NAV-PVT: the fix is valid.
#define NAV_PVT_FIX_OK			0x01

/// Bit 2 of the validity flags of NAV-TIMEUTC: the UTC date and time are valid.
#define NAV_TIMEUTC_VALID_UTC	0x04


#ifndef PIF_NO_LOG

static const char *kUbxError[] = {
		"Big Length",
		"Invalid Data",
		"Wrong CRC"
};

#endif


/**
 * @brief Adds one byte to the 8 bit Fletcher checksum of a UBX packet.
 * @param data Byte to add.
 * @param p_ck_a First sum.
 * @param p_ck_b Sum of the first sums.
 */
static void _addChecksum(uint8_t data, uint8_t *p_ck_a, uint8_t *p_ck_b)
{
	*p_ck_a += data;
	*p_ck_b += *p_ck_a;
}

/**
 * @brief Checksum of a UBX packet, over the class, the message ID, the two length bytes and the payload.
 * @param p_head The four bytes from the class to the high byte of the length.
 * @param p_payload Payload, or NULL when there is none.
 * @param length Payload length in bytes.
 * @param p_ck_a Takes the first sum.
 * @param p_ck_b Takes the sum of the first sums.
 */
static void _checksumUbx(const uint8_t *p_head, const uint8_t *p_payload, uint16_t length, uint8_t *p_ck_a, uint8_t *p_ck_b)
{
	uint16_t i;

	*p_ck_a = 0;
	*p_ck_b = 0;
	for (i = 0; i < 4; i++) _addChecksum(p_head[i], p_ck_a, p_ck_b);
	for (i = 0; i < length; i++) _addChecksum(p_payload[i], p_ck_a, p_ck_b);
}

/**
 * @brief Reports a UBX packet that was lost and starts the parser over.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param error Why it was lost.
 */
static void _failPacket(PifGpsUblox *p_owner, PifGpsUbxError error)
{
	if (p_owner->evt_ubx_error) (*p_owner->evt_ubx_error)(p_owner, error);
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "GU(%u): %s RS:%u CID:%x MID:%x Len:%u", p_owner->_gps._id, kUbxError[error],
			p_owner->__rx.state, p_owner->__rx.packet.class_id, p_owner->__rx.packet.msg_id,
			p_owner->__rx.packet.length);
#endif
	p_owner->__rx.state = GURX_SYNC_CHAR_1;
}

/**
 * @brief Takes the satellite view out of a NAV-SVINFO packet, into the arrays of the PifGps that the NMEA
 *        GSV sentences fill as well.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param p_sv_info Payload of the packet.
 */
static void _parseSvInfo(PifGpsUblox *p_owner, PifGpsUbxNavSvInfo *p_sv_info)
{
	PifGps *p_parent = &p_owner->_gps;
	uint8_t count, i;

	count = p_sv_info->num_ch;
	if (count > PIF_GPS_SV_MAXSATS) count = PIF_GPS_SV_MAXSATS;
	for (i = 0; i < count; i++) {
		p_parent->_sv_chn[i] = p_sv_info->channel[i].chn;
		p_parent->_sv_svid[i] = p_sv_info->channel[i].svid;
		p_parent->_sv_quality[i] = p_sv_info->channel[i].quality;
		p_parent->_sv_cno[i] = p_sv_info->channel[i].cno;
	}
	p_parent->_sv_num_sv = count;
	p_parent->_sv_received_count++;
}

/**
 * @brief Takes the whole solution out of a NAV-PVT packet: a receiver set to send it alone, with no
 *        POSLLH, SOL, VELNED or NMEA, still gives the position, the fix, the speed and the time.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param p_pvt Payload of the packet.
 */
static void _parsePvt(PifGpsUblox *p_owner, PifGpsUbxNavPvt *p_pvt)
{
	PifGps *p_parent = &p_owner->_gps;

	p_parent->_utc.year = p_pvt->year - 2000;
	p_parent->_utc.month = p_pvt->month;
	p_parent->_utc.day = p_pvt->day;
	p_parent->_utc.hour = p_pvt->hour;
	p_parent->_utc.minute = p_pvt->min;
	p_parent->_utc.second = p_pvt->sec;
	// The fraction of the second runs from -1e9 to 1e9: the second itself is the nearest one, so a
	// negative fraction belongs to the second before and leaves nothing to add to this one.
	p_parent->_utc.millisecond = p_pvt->nano > 0 ? p_pvt->nano / 1000000 : 0;

	p_parent->_fix = p_pvt->fix_type == FIX_3D && (p_pvt->flags & NAV_PVT_FIX_OK);
	p_parent->_num_sat = p_pvt->num_sv;
	p_parent->_coord_deg[PIF_GPS_LON] = p_pvt->lon / 10000000.0;
	p_parent->_coord_deg[PIF_GPS_LAT] = p_pvt->lat / 10000000.0;
	p_parent->_altitude = p_pvt->h_msl / 1000.0;
	p_parent->_horizontal_acc = p_pvt->h_acc;
	p_parent->_vertical_acc = p_pvt->v_acc;
	p_parent->_ground_speed = p_pvt->g_speed / 10.0;	// mm/s to cm/s
	p_parent->_ground_course = p_pvt->head_mot / 100000.0;
	p_parent->_hdop = p_pvt->p_dop;		// Position DOP: NAV-PVT carries no horizontal one.
	p_parent->_update_rate[0] = p_parent->_update_rate[1];
	p_parent->_update_rate[1] = pif_cumulative_timer1ms;
}

/**
 * @brief Takes in a complete UBX packet, then hands it to evt_ubx_receive.
 * @param p_owner Pointer to the u-blox wrapper.
 */
static void _parsePacket(PifGpsUblox *p_owner)
{
	PifGpsUbxPacket *p_packet = &p_owner->__rx.packet;
	PifGps *p_parent = &p_owner->_gps;

#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x %x %x %x", p_parent->_id, p_packet->class_id, p_packet->msg_id,
			p_packet->length, p_packet->payload.bytes[0], p_packet->payload.bytes[1], p_packet->payload.bytes[2],
			p_packet->payload.bytes[3]);
#endif
#endif

	switch (p_packet->class_id) {
	case GUCI_ACK:
		// An ACK or a NAK names the message it answers in its two payload bytes.
		if (p_owner->_request_state == GURS_SEND && p_packet->length >= 2 &&
				p_packet->payload.bytes[0] == GUCI_CFG && p_packet->payload.bytes[1] == p_owner->__cfg_msg_id) {
			if (p_packet->msg_id == GUMI_ACK_ACK) p_owner->_request_state = GURS_ACK;
			else if (p_packet->msg_id == GUMI_ACK_NAK) p_owner->_request_state = GURS_NAK;
		}
		break;

	case GUCI_NAV:
		switch (p_packet->msg_id) {
		case GUMI_NAV_POSLLH:
			p_parent->_coord_deg[PIF_GPS_LON] = p_packet->payload.posllh.lon / 10000000.0;
			p_parent->_coord_deg[PIF_GPS_LAT] = p_packet->payload.posllh.lat / 10000000.0;
			p_parent->_altitude = p_packet->payload.posllh.h_msl / 1000.0;
			p_parent->_horizontal_acc = p_packet->payload.posllh.h_acc;
			p_parent->_vertical_acc = p_packet->payload.posllh.v_acc;
			// POSLLH says nothing of the fix: NAV-SOL or NAV-STATUS of the same epoch does, and it
			// comes first.
			p_parent->_fix = p_owner->__next_fix;
			p_parent->_update_rate[0] = p_parent->_update_rate[1];
			p_parent->_update_rate[1] = pif_cumulative_timer1ms;
			break;

		case GUMI_NAV_PVT:
			_parsePvt(p_owner, &p_packet->payload.pvt);
			break;

		case GUMI_NAV_SOL:
			p_owner->__next_fix = (p_packet->payload.sol.flags & NAV_STATUS_FIX_VALID) &&
					p_packet->payload.sol.gps_fix == FIX_3D;
			if (!p_owner->__next_fix) p_parent->_fix = FALSE;
			p_parent->_num_sat = p_packet->payload.sol.num_sv;
			break;

		case GUMI_NAV_STATUS:
			p_owner->__next_fix = (p_packet->payload.status.flags & NAV_STATUS_FIX_VALID) &&
					p_packet->payload.status.gps_fix == FIX_3D;
			if (!p_owner->__next_fix) p_parent->_fix = FALSE;
			break;

		case GUMI_NAV_SVINFO:
			_parseSvInfo(p_owner, &p_packet->payload.sv_info);
			break;

		case GUMI_NAV_TIMEUTC:
			if (p_packet->payload.time_utc.valid & NAV_TIMEUTC_VALID_UTC) {
				p_parent->_utc.year = p_packet->payload.time_utc.year - 2000;
				p_parent->_utc.month = p_packet->payload.time_utc.month;
				p_parent->_utc.day = p_packet->payload.time_utc.day;
				p_parent->_utc.hour = p_packet->payload.time_utc.hour;
				p_parent->_utc.minute = p_packet->payload.time_utc.min;
				p_parent->_utc.second = p_packet->payload.time_utc.sec;
				p_parent->_utc.millisecond = p_packet->payload.time_utc.nano > 0 ?
						p_packet->payload.time_utc.nano / 1000000 : 0;
			}
			break;

		case GUMI_NAV_VELNED:
			// The ground speed is the 2-D one; speed is the 3-D one, which climbing adds to.
			p_parent->_ground_speed = p_packet->payload.velned.g_speed;
			p_parent->_ground_course = p_packet->payload.velned.heading / 100000.0;
			break;

		default:
			break;
		}
		break;

	// Every other class is for the client to read through evt_ubx_receive.
	default:
		break;
	}

	if (p_owner->evt_ubx_receive) {
		if ((*p_owner->evt_ubx_receive)(p_owner, p_packet)) pifGps_SendEvent(p_parent);
	}
}

/**
 * @brief Parses one received byte as UBX, and hands a byte that is not UBX to the NMEA parser.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param data One byte received from the transport.
 * @return `TRUE` when a complete UBX packet was parsed, otherwise `FALSE`.
 */
static BOOL _parsingPacket(PifGpsUblox *p_owner, uint8_t data)
{
	PifGpsUbxPacket *p_packet = &p_owner->__rx.packet;

	switch (p_owner->__rx.state) {
	case GURX_SYNC_CHAR_1:
		if (data == UBX_SYNC_CHAR_1) p_owner->__rx.state = GURX_SYNC_CHAR_2;
		else pifGps_ParsingNmea(&p_owner->_gps, data);
		break;

	case GURX_SYNC_CHAR_2:
		if (data == UBX_SYNC_CHAR_2) {
			p_owner->__rx.state = GURX_CLASS;
			p_owner->__rx.ck_a = 0;
			p_owner->__rx.ck_b = 0;
		}
		else {
			if (p_owner->evt_ubx_error) (*p_owner->evt_ubx_error)(p_owner, GUE_INVALID_DATA);
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_ERROR, "GU(%u): %s D:%xh", p_owner->_gps._id, kUbxError[GUE_INVALID_DATA], data);
#endif
			// The byte may be the start of the next packet, or the NMEA the receiver sends between them.
			if (data == UBX_SYNC_CHAR_1) break;
			p_owner->__rx.state = GURX_SYNC_CHAR_1;
			pifGps_ParsingNmea(&p_owner->_gps, data);
		}
		break;

	case GURX_CLASS:
		p_packet->class_id = data;
		_addChecksum(data, &p_owner->__rx.ck_a, &p_owner->__rx.ck_b);
		p_owner->__rx.state = GURX_ID;
		break;

	case GURX_ID:
		p_packet->msg_id = data;
		_addChecksum(data, &p_owner->__rx.ck_a, &p_owner->__rx.ck_b);
		p_owner->__rx.state = GURX_LENGTH_LOW;
		break;

	case GURX_LENGTH_LOW:
		p_packet->length = data;
		_addChecksum(data, &p_owner->__rx.ck_a, &p_owner->__rx.ck_b);
		p_owner->__rx.state = GURX_LENGTH_HIGH;
		break;

	case GURX_LENGTH_HIGH:
		p_packet->length |= data << 8;
		_addChecksum(data, &p_owner->__rx.ck_a, &p_owner->__rx.ck_b);
		if (p_packet->length > sizeof(p_packet->payload)) {
			_failPacket(p_owner, GUE_BIG_LENGTH);
			break;
		}
		p_owner->__rx.payload_count = 0;
		p_owner->__rx.state = p_packet->length ? GURX_PAYLOAD : GURX_CK_A;
		break;

	case GURX_PAYLOAD:
		p_packet->payload.bytes[p_owner->__rx.payload_count++] = data;
		_addChecksum(data, &p_owner->__rx.ck_a, &p_owner->__rx.ck_b);
		if (p_owner->__rx.payload_count >= p_packet->length) p_owner->__rx.state = GURX_CK_A;
		break;

	case GURX_CK_A:
		if (data != p_owner->__rx.ck_a) {
			_failPacket(p_owner, GUE_WRONG_CRC);
			break;
		}
		p_owner->__rx.state = GURX_CK_B;
		break;

	case GURX_CK_B:
		if (data != p_owner->__rx.ck_b) {
			_failPacket(p_owner, GUE_WRONG_CRC);
			break;
		}
		p_owner->__rx.state = GURX_SYNC_CHAR_1;
		_parsePacket(p_owner);
		return TRUE;

	default:
		p_owner->__rx.state = GURX_SYNC_CHAR_1;
		break;
	}
	return FALSE;
}

/**
 * @brief Transport receive callback that feeds bytes to the UBX parser.
 * @param p_client Pointer to `PifGpsUblox`.
 * @param act_receive Transport receive function pointer.
 * @return `TRUE` if a packet is completed during this callback, otherwise `FALSE`.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive)
{
	PifGpsUblox *p_owner = (PifGpsUblox *)p_client;
    uint8_t data;
    BOOL rtn = FALSE;

	while ((*act_receive)(p_owner->__p_transport, &data, 1)) {
		rtn = _parsingPacket(p_owner, data);
		if (rtn) break;
	}
	return rtn;
}

/**
 * @brief Records a request as being on its way, so that pifGpsUblox_CheckRequest() can tell when
 *        it is over.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param response TRUE when the receiver is expected to answer with an ACK or a NAK.
 * @param waiting How long the request may take, in milliseconds.
 */
static void _beginRequest(PifGpsUblox *p_owner, BOOL response, uint16_t waiting)
{
	p_owner->__request_response = response;
	p_owner->__request_pretime = pif_cumulative_timer1ms;
	p_owner->__request_timeout = waiting;
	p_owner->_request_state = GURS_SEND;
}

/**
 * @brief Puts a message in the transmit buffer, headed by its length, and asks the transport to send.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param p_head First part of the message.
 * @param head_length Bytes of p_head.
 * @param p_body Second part of the message, or NULL when there is none.
 * @param body_length Bytes of p_body.
 * @param p_tail Last part of the message, or NULL when there is none.
 * @param tail_length Bytes of p_tail.
 * @return TRUE when the message was queued.
 */
static BOOL _queueMessage(PifGpsUblox *p_owner, uint8_t *p_head, uint16_t head_length, uint8_t *p_body,
		uint16_t body_length, uint8_t *p_tail, uint16_t tail_length)
{
	uint16_t length = head_length + body_length + tail_length;

	if (!p_owner->__p_transport) {
		pif_error = E_CANNOT_FOUND;
		return FALSE;
	}
	// The whole entry is made linear, so the transmit side hands it over in one piece.
	if (!pifRingBuffer_MoveHeadForLinear(&p_owner->__tx.buffer, TX_ENTRY_HEADER_SIZE + length)) return FALSE;

	pifRingBuffer_BeginPutting(&p_owner->__tx.buffer);

	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t *)&length, TX_ENTRY_HEADER_SIZE)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_head, head_length)) goto fail;
	if (body_length > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_body, body_length)) goto fail;
	}
	if (tail_length > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_tail, tail_length)) goto fail;
	}

	pifRingBuffer_CommitPutting(&p_owner->__tx.buffer);

	pifTransport_TriggerSend(p_owner->__p_transport);
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.buffer);
	return FALSE;
}

/**
 * @brief Finishes an NMEA command, queues it and asks the transport to send it.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param p_data Sentence the caller built, from "$" up to and including the "*". It must have room for
 *        the two digits of the checksum and the CR LF after it.
 * @param waiting How long the request may take, in milliseconds.
 * @return `TRUE` if the sentence was queued, otherwise `FALSE`. Queued is not sent: the request is only
 *         over once pifGpsUblox_CheckRequest() says so.
 */
static BOOL _makeNmeaPacket(PifGpsUblox *p_owner, char *p_data, uint16_t waiting)
{
	uint16_t length;
	uint8_t parity = 0;

	// The checksum is the exclusive or of everything between the "$" and the "*".
	for (length = 1; p_data[length] && p_data[length] != '*'; length++) parity ^= (uint8_t)p_data[length];
	length++;	// The "*" itself belongs to the sentence.
	p_data[length++] = kPifHexUpperChar[(parity >> 4) & 0x0F];
	p_data[length++] = kPifHexUpperChar[parity & 0x0F];
	p_data[length++] = '\r';
	p_data[length++] = '\n';

	if (!_queueMessage(p_owner, (uint8_t *)p_data, length, NULL, 0, NULL, 0)) return FALSE;

	// Draining the buffer needs the transport to run, so the request is left on its way and
	// pifGpsUblox_CheckRequest() is what sees it end. An NMEA command is not answered, so there is
	// nothing to wait for beyond the last byte leaving.
	_beginRequest(p_owner, FALSE, waiting);
	return TRUE;
}

/**
 * @brief Builds a UBX packet, queues it and asks the transport to send it.
 * @param p_owner Pointer to the u-blox wrapper.
 * @param p_header The six bytes from the sync characters to the high byte of the length.
 * @param length Payload size in bytes.
 * @param p_payload Payload, or NULL when length is zero.
 * @param response TRUE when the receiver is expected to answer this message with an ACK or a NAK.
 * @param waiting How long the request may take, in milliseconds.
 * @return `TRUE` if the packet was queued, otherwise `FALSE`. Queued is not answered: the request
 *         is only over once pifGpsUblox_CheckRequest() says so.
 */
static BOOL _makeUbxPacket(PifGpsUblox *p_owner, uint8_t *p_header, uint16_t length, uint8_t *p_payload, BOOL response, uint16_t waiting)
{
	uint8_t tailer[2];

	_checksumUbx(p_header + 2, p_payload, length, &tailer[0], &tailer[1]);

	if (!_queueMessage(p_owner, p_header, 6, p_payload, length, tailer, 2)) {
		p_owner->_request_state = GURS_FAILURE;
		return FALSE;
	}

	// Both the sending and any answer need other tasks to run, so the request is left on its way
	// either way and pifGpsUblox_CheckRequest() is what sees it end.
	_beginRequest(p_owner, response, waiting);
	return TRUE;
}

/**
 * @brief Transport transmit callback that hands the queued messages over, one at a time.
 * @param p_client Pointer to `PifGpsUblox`.
 * @param act_send Transport send function pointer.
 * @return Number of bytes handed over, or 1 while it takes the next message up, so the transport asks
 *         again.
 */
static uint16_t _evtSending(void *p_client, PifActTransportSend act_send)
{
	PifGpsUblox *p_owner = (PifGpsUblox *)p_client;
	uint16_t length = 0;
	uint8_t *p_data;

	switch (p_owner->__tx.state) {
	case GUTS_IDLE:
		if (!pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
			pifRingBuffer_CopyToArray((uint8_t *)&p_owner->__tx.length, TX_ENTRY_HEADER_SIZE, &p_owner->__tx.buffer, 0);
			p_owner->__tx.pos = TX_ENTRY_HEADER_SIZE;
			p_owner->__tx.state = GUTS_SENDING;
			length = 1;
		}
		break;

	case GUTS_SENDING:
		p_data = pifRingBuffer_GetReadPointer(&p_owner->__tx.buffer, p_owner->__tx.pos, &length);
		length = (*act_send)(p_owner->__p_transport, p_data, length);
		if (!length) return 0;

		p_owner->__tx.pos += length;
		if (p_owner->__tx.pos >= TX_ENTRY_HEADER_SIZE + p_owner->__tx.length) {
			pifRingBuffer_Remove(&p_owner->__tx.buffer, p_owner->__tx.pos);
			p_owner->__tx.state = GUTS_IDLE;
		}
		break;

	default:
		break;
	}
	return length;
}

/**
 * @brief Transport abort callback that resets UBX receive parser state.
 * @param p_client Pointer to `PifGpsUblox`.
 */
static void _evtAbortRx(void *p_client)
{
	((PifGpsUblox *)p_client)->__rx.state = GURX_SYNC_CHAR_1;
}

/**
 * @brief Whether a new request may be started. One is refused while the previous one is still
 *        being sent or still waiting for its answer, because there is one transmit buffer and
 *        one request state to hold it in. The refusal leaves that state alone: it belongs to the
 *        request still on its way, whose ACK or NAK would otherwise be lost.
 * @param p_owner Pointer to the u-blox wrapper.
 * @return `TRUE` if sending is allowed, otherwise `FALSE` with pif_error set to E_INVALID_STATE.
 */
static BOOL _beginPossible(PifGpsUblox *p_owner)
{
	if (p_owner->__tx.state != GUTS_IDLE || p_owner->_request_state == GURS_SEND) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	return TRUE;
}

/**
 * @brief Builds one of the NMEA poll requests, which are all a talker, a query formatter and the ID of
 *        the message the receiver is to send.
 * @param p_data Takes the sentence, up to and including the "*".
 * @param size Bytes of p_data. The sentence needs the five characters of the checksum and the end of
 *        line after the "*".
 * @param p_query The query sentence without its fields, for example "$GPGPQ".
 * @param p_mag_id ID of the message to poll.
 * @return TRUE when the sentence fits.
 */
static BOOL _makePollRequest(char *p_data, size_t size, const char *p_query, const char *p_mag_id)
{
	size_t length;

	// The six characters of the query, the comma, the message ID, the "*", the two digits of the
	// checksum, CR LF and the terminator.
	length = strlen(p_query) + 1 + strlen(p_mag_id) + 1;
	if (length + 5 > size) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	strcpy(p_data, p_query);
	strcat(p_data, ",");
	strcat(p_data, p_mag_id);
	strcat(p_data, "*");
	return TRUE;
}

BOOL pifGpsUblox_Init(PifGpsUblox *p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGpsUblox));

    if (!pifGps_Init(&p_owner->_gps, id)) goto fail;

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.buffer, PIF_ID_AUTO, PIF_GPS_UBLOX_TX_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.buffer, "TxB");
    return TRUE;

fail:
	pifGpsUblox_Clear(p_owner);
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "GU:%u(%u) EC:%d", __LINE__, id, pif_error);
#endif
    return FALSE;
}

void pifGpsUblox_Clear(PifGpsUblox *p_owner)
{
	pifGpsUblox_DetachTransport(p_owner);
	pifRingBuffer_Clear(&p_owner->__tx.buffer);
	pifGps_Clear(&p_owner->_gps);
}

BOOL pifGpsUblox_AttachTransport(PifGpsUblox *p_owner, PifTransport *p_transport)
{
	if (!p_owner || !p_transport) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	pifGpsUblox_DetachTransport(p_owner);
	p_owner->__p_transport = p_transport;
	pifTransport_AttachClient(p_transport, p_owner, _evtParsing, _evtSending, _evtAbortRx);
	return TRUE;
}

void pifGpsUblox_DetachTransport(PifGpsUblox *p_owner)
{
	if (!p_owner || !p_owner->__p_transport) return;

	pifTransport_DetachClient(p_owner->__p_transport);
	p_owner->__p_transport = NULL;
}

BOOL pifGpsUblox_ParsingPacket(PifGpsUblox *p_owner, uint8_t data)
{
	return _parsingPacket(p_owner, data);
}

PifGpsUbxRequestState pifGpsUblox_CheckRequest(PifGpsUblox *p_owner)
{
	if (p_owner->_request_state != GURS_SEND) return p_owner->_request_state;

	// An ACK or a NAK is set by the parser the moment it arrives, so what is left to decide here
	// are the two ways a request ends without one.
	if (!p_owner->__request_response) {
		if (pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) p_owner->_request_state = GURS_TIMEOUT;
	}
	if (p_owner->_request_state == GURS_SEND &&
			pif_cumulative_timer1ms - p_owner->__request_pretime >= p_owner->__request_timeout) {
		p_owner->_request_state = GURS_TIMEOUT;
	}
	return p_owner->_request_state;
}

BOOL pifGpsUblox_PollRequestGBQ(PifGpsUblox *p_owner, const char *p_mag_id, uint16_t waiting)
{
	char data[24];

	if (!_beginPossible(p_owner)) return FALSE;
	if (!_makePollRequest(data, sizeof(data), "$GBGBQ", p_mag_id)) return FALSE;

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGLQ(PifGpsUblox *p_owner, const char *p_mag_id, uint16_t waiting)
{
	char data[24];

	if (!_beginPossible(p_owner)) return FALSE;
	if (!_makePollRequest(data, sizeof(data), "$GLGLQ", p_mag_id)) return FALSE;

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGNQ(PifGpsUblox *p_owner, const char *p_mag_id, uint16_t waiting)
{
	char data[24];

	if (!_beginPossible(p_owner)) return FALSE;
	if (!_makePollRequest(data, sizeof(data), "$GNGNQ", p_mag_id)) return FALSE;

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGPQ(PifGpsUblox *p_owner, const char *p_mag_id, uint16_t waiting)
{
	char data[24];

	if (!_beginPossible(p_owner)) return FALSE;
	if (!_makePollRequest(data, sizeof(data), "$GPGPQ", p_mag_id)) return FALSE;

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SetPubxConfig(PifGpsUblox *p_owner, uint8_t port_id, uint16_t in_proto, uint16_t out_proto, uint32_t baudrate, uint16_t waiting)
{
	char data[48];

	if (!_beginPossible(p_owner)) return FALSE;

	pif_Printf(data, sizeof(data) - 5, "$PUBX,41,%u,%4X,%4X,%lu,0*", port_id, in_proto, out_proto, baudrate);

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SetPubxRate(PifGpsUblox *p_owner, const char *p_mag_id, uint8_t rddc, uint8_t rus1, uint8_t rus2, uint8_t rusb, uint8_t rspi, uint16_t waiting)
{
	char data[48];

	if (!_beginPossible(p_owner)) return FALSE;

	pif_Printf(data, sizeof(data) - 5, "$PUBX,40,%s,%u,%u,%u,%u,%u,0*", p_mag_id, rddc, rus1, rus2, rusb, rspi);

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SendUbxMsg(PifGpsUblox *p_owner, uint8_t class_id, uint8_t msg_id, uint16_t length, uint8_t *payload, uint16_t waiting)
{
	uint8_t header[6] = { UBX_SYNC_CHAR_1, UBX_SYNC_CHAR_2 };

	if (!_beginPossible(p_owner)) return FALSE;
	header[2] = class_id;
	header[3] = msg_id;
	header[4] = length & 0xFF;
	header[5] = length >> 8;

	// Only a configuration message is acknowledged, and __cfg_msg_id is what the parser matches
	// the acknowledgement against.
	if (class_id == GUCI_CFG) {
		p_owner->__cfg_msg_id = msg_id;
		return _makeUbxPacket(p_owner, header, length, payload, TRUE, waiting);
	}

	return _makeUbxPacket(p_owner, header, length, payload, FALSE, waiting);
}
