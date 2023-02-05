#include <ctype.h>
#include <string.h>

#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "gps/pif_gps_ublox.h"

#include <string.h>


#define DIGIT_TO_VAL(_x)        (_x - '0')

#define PKT_ERR_BIG_LENGHT		0
#define PKT_ERR_INVALID_DATA    1
#define PKT_ERR_WRONG_CRC    	2
#define PKT_ERR_UNKNOWE_ID   	3
#define PKT_ERR_NONE		   	4


enum {
    FIX_NONE = 0,
    FIX_DEAD_RECKONING = 1,
    FIX_2D = 2,
    FIX_3D = 3,
    FIX_GPS_DEAD_RECKONING = 4,
    FIX_TIME = 5
} UbxNavGpsFix;

enum {
    NAV_STATUS_FIX_VALID = 1
} UbxNavStatus;


#ifndef __PIF_NO_LOG__

static const char *kPktErr[] = {
		"Big Length",
		"Invalid Data",
		"Wrong CRC",
		"Unknown ID"
};

#endif


static uint16_t _checksumUbx(uint8_t* p_header, uint8_t* p_payload, uint16_t len)
{
	uint8_t ck_a = 0, ck_b = 0;
	uint16_t i;

	for (i = 0; i < 4; i++) {
		ck_a += p_header[i];
		ck_b += ck_a;
	}
	for (i = 0; i < len; i++) {
		ck_a += p_payload[i];
		ck_b += ck_a;
	}
	return ck_a + (ck_b << 8);
}

static void _parsingPacket(PifGpsUblox *p_owner, PifActCommReceiveData act_receive_data)
{
	PifGpsUbxPacket* p_packet = &p_owner->__rx.packet;
	uint8_t data;
#ifndef __PIF_NO_LOG__
	uint8_t pkt_err;
	int line;
	static uint8_t pre_err = PKT_ERR_NONE;
#endif

	while ((*act_receive_data)(p_owner->__p_comm, &data)) {
		switch (p_owner->__rx.state) {
		case GURS_SYNC_CHAR_1:
			if (data == 0xB5) {
				p_owner->__rx.state = GURS_SYNC_CHAR_2;
#ifndef __PIF_NO_LOG__
				pre_err = PKT_ERR_NONE;
#endif
			}
			else if (pifGps_ParsingNmea(&p_owner->_gps, data)) {
				p_owner->__rx.state = GURS_NMEA;
#ifndef __PIF_NO_LOG__
				pre_err = PKT_ERR_NONE;
#endif
			}
			break;

		case GURS_SYNC_CHAR_2:
			if (data == 0x62) {
				p_owner->__rx.state = GURS_CLASS;
			}
			else {
#ifndef __PIF_NO_LOG__
				pkt_err = PKT_ERR_INVALID_DATA;
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case GURS_CLASS:
			p_packet->class_id = data;
			p_owner->__rx.state = GURS_ID;
			break;

		case GURS_ID:
			p_packet->msg_id = data;
			p_owner->__rx.state = GURS_LENGTH_LOW;
			break;

		case GURS_LENGTH_LOW:
			p_packet->length = data;
			p_owner->__rx.state = GURS_LENGTH_HIGH;
			break;

		case GURS_LENGTH_HIGH:
			p_packet->length |= data << 8;
			if (p_packet->length < sizeof(PifGpsUbxPacket) - 4) {
				p_owner->__rx.payload_count = 0;
				p_owner->__rx.state = GURS_PAYLOAD;
			}
			else {
#ifndef __PIF_NO_LOG__
				pkt_err = PKT_ERR_BIG_LENGHT;
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case GURS_PAYLOAD:
			p_packet->payload.bytes[p_owner->__rx.payload_count] = data;
			p_owner->__rx.payload_count++;
			if (p_owner->__rx.payload_count >= p_packet->length) {
				p_owner->__rx.state = GURS_CK_A;
			}
			break;

		case GURS_CK_A:
			p_owner->__rx.checksum = data;
			p_owner->__rx.state = GURS_CK_B;
			break;

		case GURS_CK_B:
			p_owner->__rx.checksum |= data << 8;
			if (p_owner->__rx.checksum == _checksumUbx((uint8_t*)p_packet, p_packet->payload.bytes, p_packet->length)) {
				p_owner->__rx.state = GURS_DONE;
				return;
			}
			else {
#ifndef __PIF_NO_LOG__
				pkt_err = PKT_ERR_WRONG_CRC;
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case GURS_NMEA:
			if (pifGps_ParsingNmea(&p_owner->_gps, data))	{
				p_owner->__rx.state = GURS_SYNC_CHAR_1;
				return;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
#ifndef __PIF_NO_LOG__
	if (pkt_err != pre_err) {
		if (p_owner->__rx.state) {
			pifLog_Printf(LT_ERROR, "GU:%u(%u) %s D:%xh RS:%u CID:%u MID:%u Len:%u", line, p_owner->_gps._id, kPktErr[pkt_err], data,
					p_owner->__rx.state, p_packet->class_id, p_packet->msg_id, p_packet->length);
		}
		else {
			pifLog_Printf(LT_ERROR, "GU:%u(%u) %s D:%xh", line, p_owner->_gps._id, kPktErr[pkt_err], data);
		}
		pre_err = pkt_err;
	}
#ifdef __DEBUG_PACKET__
	pifLog_Printf(LT_NONE, "\n%x %x %x %x %x %x %x %x", p_packet->payload.bytes[0], p_packet->payload.bytes[1],	p_packet->payload.bytes[2],
			p_packet->payload.bytes[3], p_packet->payload.bytes[4], p_packet->payload.bytes[5], p_packet->payload.bytes[6], p_packet->payload.bytes[7]);
#endif
#endif

	p_owner->__rx.state = GURS_SYNC_CHAR_1;
}

static void _evtParsing(void *p_client, PifActCommReceiveData act_receive_data)
{
	PifGpsUblox *p_owner = (PifGpsUblox *)p_client;
	PifGpsUbxPacket* p_packet = &p_owner->__rx.packet;
	PifGps *p_parent = &p_owner->_gps;
    int i;
    static BOOL _new_position = FALSE;
    static BOOL next_fix = FALSE;
    static BOOL _new_speed = FALSE;
    BOOL error = FALSE;

    if (!p_owner->_gps.evt_receive) return;

    if (p_owner->__rx.state < GURS_DONE) {
    	_parsingPacket(p_owner, act_receive_data);
    }

    if (p_owner->__rx.state == GURS_DONE) {
#ifndef __PIF_NO_LOG__
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x %x %x %x", p_owner->_gps._id, p_packet->class_id, p_packet->msg_id, p_packet->length,
    			p_packet->payload.bytes[0], p_packet->payload.bytes[1], p_packet->payload.bytes[2], p_packet->payload.bytes[3], p_packet->payload.bytes[4]);
#endif
#endif

        switch (p_packet->class_id) {
        case GUCI_ACK:
        	switch (p_packet->msg_id) {
        	case GUMI_ACK_ACK:
        	case GUMI_ACK_NAK:
        		if (p_owner->evt_ubx_cfg_result) (*p_owner->evt_ubx_cfg_result)(p_owner, p_packet->msg_id);
        		break;

            default:
            	error = TRUE;
#ifndef __PIF_NO_LOG__
        		pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x MID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id, p_packet->msg_id);
#endif
                break;
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
                    p_parent->_fix = next_fix;
                    _new_position = TRUE;
                    // Update GPS update rate table.
                    p_parent->_update_rate[0] = p_parent->_update_rate[1];
                    p_parent->_update_rate[1] = pif_cumulative_timer1ms;
                    break;

                case GUMI_NAV_PVT:
                	p_parent->_utc.year = 20 + p_packet->payload.pvt.year - 2000;
                	p_parent->_utc.month = p_packet->payload.pvt.month;
                	p_parent->_utc.day = p_packet->payload.pvt.day;
                	p_parent->_utc.hour = p_packet->payload.pvt.hour;
                	p_parent->_utc.minute = p_packet->payload.pvt.min;
                	p_parent->_utc.second = p_packet->payload.pvt.sec;
                	p_parent->_utc.millisecond = p_packet->payload.pvt.nano / 1000000UL;
                    break;

                case GUMI_NAV_SOL:
                    next_fix = (p_packet->payload.sol.flags & NAV_STATUS_FIX_VALID) && (p_packet->payload.sol.gps_fix == FIX_3D);
                    if (!next_fix)
                    	p_parent->_fix = FALSE;
                    p_parent->_num_sat = p_packet->payload.sol.num_sv;
                    break;

                case GUMI_NAV_STATUS:
                    next_fix = (p_packet->payload.status.flags & NAV_STATUS_FIX_VALID) && (p_packet->payload.status.gps_fix == FIX_3D);
                    if (!next_fix)
                    	p_parent->_fix = FALSE;
                    break;

                case GUMI_NAV_SVINFO:
                	p_owner->_num_ch = p_packet->payload.sv_info.num_ch;
                    if (p_owner->_num_ch > 16)
                    	p_owner->_num_ch = 16;
                    for (i = 0; i < p_owner->_num_ch; i++) {
                    	p_owner->_svinfo_chn[i] = p_packet->payload.sv_info.channel[i].chn;
                    	p_owner->_svinfo_svid[i] = p_packet->payload.sv_info.channel[i].svid;
                    	p_owner->_svinfo_quality[i] = p_packet->payload.sv_info.channel[i].quality;
                    	p_owner->_svinfo_cno[i] = p_packet->payload.sv_info.channel[i].cno;
                    }
                    // Update GPS SVIFO update rate table.
                    p_owner->_svinfo_rate[0] = p_owner->_svinfo_rate[1];
                    p_owner->_svinfo_rate[1] = pif_cumulative_timer1ms;
                    break;

                case GUMI_NAV_TIMEUTC:
                	if (p_packet->payload.time_utc.valid & 4) {
						p_parent->_utc.year = p_packet->payload.time_utc.year - 2000;
						p_parent->_utc.month = p_packet->payload.time_utc.month;
						p_parent->_utc.day = p_packet->payload.time_utc.day;
						p_parent->_utc.hour = p_packet->payload.time_utc.hour;
						p_parent->_utc.minute = p_packet->payload.time_utc.min;
						p_parent->_utc.second = p_packet->payload.time_utc.sec;
						p_parent->_utc.millisecond = p_packet->payload.time_utc.nano / 1000000UL;
                	}
                	break;

                case GUMI_NAV_VELNED:
                	p_parent->_ground_speed = p_packet->payload.velned.speed;
                	p_parent->_ground_course = p_packet->payload.velned.heading / 100000.0;
                    _new_speed = TRUE;
                    break;

                default:
                	error = TRUE;
#ifndef __PIF_NO_LOG__
            		pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x MID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id, p_packet->msg_id);
#endif
                    break;
            }
        	break;

		default:
        	error = TRUE;
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id);
#endif
			break;
        }

    	if (!error && p_owner->evt_ubx_receive) (*p_owner->evt_ubx_receive)(p_packet);

        if (_new_position && _new_speed) {
			pifGps_SendEvent(&p_owner->_gps);
            _new_speed = _new_position = FALSE;
        }
    	p_owner->__rx.state = GURS_SYNC_CHAR_1;
    }
}

static BOOL _makeNmeaPacket(PifGpsUblox* p_owner, char* p_data, BOOL blocking)
{
	uint8_t header[4];
	uint8_t parity = 0;
	int i;

	i = 1;
	while (TRUE) {
		if (p_data[i] == '*') {
			i++;
			break;
		}
		else {
			parity ^= p_data[i];
			i++;
		}
	}
	p_data[i] = kPifHexUpperChar[(parity >> 4) & 0x0F]; i++;
	p_data[i] = kPifHexUpperChar[parity & 0x0F]; i++;
	p_data[i] = '\r'; i++;
	p_data[i] = '\n'; i++;
	p_data[i] = 0;

	if (blocking) {
		while (!pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
			if (!pifTaskManager_Yield()) break;
		}
	}

	pifRingBuffer_BeginPutting(&p_owner->__tx.buffer);

	header[0] = i;
	header[1] = 0;
	header[2] = 0;
	header[3] = 0;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, header, 4)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t *)p_data, header[0])) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.buffer);

	pifTask_SetTrigger(p_owner->__p_comm->_p_task);
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.buffer);
	return FALSE;
}

static BOOL _makeUbxPacket(PifGpsUblox* p_owner, uint8_t* p_header, uint16_t length, uint8_t* p_payload, BOOL blocking)
{
	uint8_t info[4];
	uint8_t tailer[2];
	uint16_t checksum;

	checksum = _checksumUbx(p_header + 2, p_payload, length);
	tailer[0] = checksum & 0xFF;
	tailer[1] = checksum >> 8;

	if (blocking) {
		while (!pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
			if (!pifTaskManager_Yield()) break;
		}
	}

	pifRingBuffer_BeginPutting(&p_owner->__tx.buffer);

	info[0] = length + 8;
	info[1] = 0;
	info[2] = 0;
	info[3] = 0;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, info, 4)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_header, 6)) goto fail;
	if (length > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_payload, length)) goto fail;
	}
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, tailer, 2)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.buffer);

	pifTask_SetTrigger(p_owner->__p_comm->_p_task);
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.buffer);
	return FALSE;
}

BOOL _evtSending(void* p_client, PifActCommSendData act_send_data)
{
	PifGpsUblox *p_owner = (PifGpsUblox *)p_client;
	uint16_t length;

	switch (p_owner->__tx.state) {
	case GUTS_IDLE:
		if (!pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
			pifRingBuffer_CopyToArray(p_owner->__tx.ui.info, 4, &p_owner->__tx.buffer, 0);
			p_owner->__tx.pos = 4;
			p_owner->__tx.state = GUTS_SENDING;
		}
		break;

	case GUTS_SENDING:
		length = (*act_send_data)(p_owner->__p_comm, pifRingBuffer_GetTailPointer(&p_owner->__tx.buffer, p_owner->__tx.pos),
				pifRingBuffer_GetLinerSize(&p_owner->__tx.buffer, p_owner->__tx.pos));
		p_owner->__tx.pos += length;
		if (p_owner->__tx.pos >= 4 + p_owner->__tx.ui.st.length) {
			p_owner->__tx.state = GUTS_WAIT_SENDED;
		}
		return TRUE;

	case GUTS_WAIT_SENDED:
		if (!p_owner->__tx.ui.st.response) {
			pifRingBuffer_Remove(&p_owner->__tx.buffer, 4 + p_owner->__tx.ui.st.length);
			p_owner->__tx.state = GUTS_IDLE;
		}
		break;

	default:
		break;
	}
	return FALSE;
}

BOOL pifGpsUblox_Init(PifGpsUblox* p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		goto fail;
	}

	memset(p_owner, 0, sizeof(PifGpsUblox));

    if (!pifGps_Init(&p_owner->_gps, id)) goto fail;

    if (!pifRingBuffer_InitHeap(&p_owner->__tx.buffer, PIF_ID_AUTO, PIF_GPS_UBLOX_TX_SIZE)) goto fail;
    pifRingBuffer_SetName(&p_owner->__tx.buffer, "TxB");
    return TRUE;

fail:
	pifGpsUblox_Clear(p_owner);
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, id, pif_error);
#endif
    return FALSE;
}

void pifGpsUblox_Clear(PifGpsUblox* p_owner)
{
	pifRingBuffer_Clear(&p_owner->__tx.buffer);
}

void pifGpsUblox_AttachComm(PifGpsUblox* p_owner, PifComm *p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner, _evtParsing, _evtSending);
}

void pifGpsUblox_DetachComm(PifGpsUblox* p_owner)
{
	pifComm_DetachClient(p_owner->__p_comm);
	p_owner->__p_comm = NULL;
}

BOOL pifGpsUblox_PollRequestGBQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking)
{
	char data[16] = "$GBGBQ,";
	int i;

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_PollRequestGLQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking)
{
	char data[16] = "$GLGLQ,";
	int i;

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_PollRequestGNQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking)
{
	char data[16] = "$GNGNQ,";
	int i;

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_PollRequestGPQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking)
{
	char data[16] = "$GPGPQ,";
	int i;

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_SetPubxConfig(PifGpsUblox* p_owner, uint8_t port_id, uint16_t in_proto, uint16_t out_proto, uint32_t baudrate, BOOL blocking)
{
	char data[40];

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	pif_Printf(data, "$PUBX,41,%u,%4X,%4X,%lu,0*", port_id, in_proto, out_proto, baudrate);

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_SetPubxRate(PifGpsUblox* p_owner, const char* p_mag_id, uint8_t rddc, uint8_t rus1, uint8_t rus2, uint8_t rusb, uint8_t rspi, BOOL blocking)
{
	char data[40];

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	pif_Printf(data, "$PUBX,40,%s,%u,%u,%u,%u,%u,0*", p_mag_id, rddc, rus1, rus2, rusb, rspi);

	return _makeNmeaPacket(p_owner, data, blocking);
}

BOOL pifGpsUblox_SendUbxMsg(PifGpsUblox* p_owner, uint8_t class_id, uint8_t msg_id, uint16_t length, uint8_t* payload, BOOL blocking)
{
	uint8_t header[6] = { 0xB5, 0x62 };

	if (p_owner->__tx.state != GUTS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	header[2] = class_id;
	header[3] = msg_id;
	header[4] = length & 0xFF;
	header[5] = length >> 8;

	return _makeUbxPacket(p_owner, header, length, payload, blocking);
}
