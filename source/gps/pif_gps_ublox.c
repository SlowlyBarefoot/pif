#include <ctype.h>
#include <string.h>

#ifndef PIF_NO_LOG
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


#ifndef PIF_NO_LOG

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

static void _parsingPacket(PifGpsUblox *p_owner, uint8_t data)
{
	PifGpsUbxPacket* p_packet = &p_owner->__rx.packet;
	PifGps *p_parent = &p_owner->_gps;
    int i;
#ifndef PIF_NO_LOG
	uint8_t pkt_err;
	int line;
	static uint8_t pre_err = PKT_ERR_NONE;
#endif

	switch (p_owner->__rx.state) {
	case GURS_SYNC_CHAR_1:
		if (data == 0xB5) {
			p_owner->__rx.state = GURS_SYNC_CHAR_2;
#ifndef PIF_NO_LOG
			pre_err = PKT_ERR_NONE;
#endif
		}
		else {
			pifGps_ParsingNmea(&p_owner->_gps, data);
		}
		break;

	case GURS_SYNC_CHAR_2:
		if (data == 0x62) {
			p_owner->__rx.state = GURS_CLASS;
		}
		else {
#ifndef PIF_NO_LOG
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
#ifndef PIF_NO_LOG
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
		}
		else {
#ifndef PIF_NO_LOG
			pkt_err = PKT_ERR_WRONG_CRC;
			line = __LINE__;
#endif
			goto fail;
		}
		break;

	default:
		break;
	}

    if (p_owner->__rx.state == GURS_DONE) {
#ifndef PIF_NO_LOG
#ifdef __DEBUG_PACKET__
    	pifLog_Printf(LT_NONE, "\n%u> %x %x %x %x %x %x %x %x", p_owner->_gps._id, p_packet->class_id, p_packet->msg_id, p_packet->length,
    			p_packet->payload.bytes[0], p_packet->payload.bytes[1], p_packet->payload.bytes[2], p_packet->payload.bytes[3], p_packet->payload.bytes[4]);
#endif
#endif

        switch (p_packet->class_id) {
        case GUCI_ACK:
    		if (p_owner->_request_state == GURS_SEND && p_packet->payload.bytes[0] == GUCI_CFG && p_packet->payload.bytes[1] == p_owner->__cfg_msg_id) {
            	switch (p_packet->msg_id) {
            	case GUMI_ACK_ACK:
            		p_owner->_request_state = GURS_ACK;
            		break;

            	case GUMI_ACK_NAK:
            		p_owner->_request_state = GURS_NAK;
            		break;

                default:
#ifndef PIF_NO_LOG
            		pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x MID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id, p_packet->msg_id);
#endif
                    break;
            	}
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
                    p_parent->_fix = p_owner->__next_fix;
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
                	p_owner->__next_fix = (p_packet->payload.sol.flags & NAV_STATUS_FIX_VALID) && (p_packet->payload.sol.gps_fix == FIX_3D);
                    if (!p_owner->__next_fix)
                    	p_parent->_fix = FALSE;
                    p_parent->_num_sat = p_packet->payload.sol.num_sv;
                    break;

                case GUMI_NAV_STATUS:
                	p_owner->__next_fix = (p_packet->payload.status.flags & NAV_STATUS_FIX_VALID) && (p_packet->payload.status.gps_fix == FIX_3D);
                    if (!p_owner->__next_fix)
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
                    break;

                default:
#ifndef PIF_NO_LOG
            		pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x MID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id, p_packet->msg_id);
#endif
                    break;
            }
        	break;

		default:
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_ERROR, "GU:%u(%u) %s CID:%x", __LINE__, p_owner->_gps._id, kPktErr[PKT_ERR_UNKNOWE_ID], p_packet->class_id);
#endif
			break;
        }

		if (p_owner->evt_ubx_receive) {
			if ((*p_owner->evt_ubx_receive)(p_owner, p_packet)) pifGps_SendEvent(&p_owner->_gps);
		}

    	p_owner->__rx.state = GURS_SYNC_CHAR_1;
    }
	return;

fail:
#ifndef PIF_NO_LOG
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

static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifGpsUblox *p_owner = (PifGpsUblox *)p_client;
    uint8_t data;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		_parsingPacket(p_owner, data);
	}
}

#define DATA_SIZE	128

static uint16_t _doTask(PifTask* p_task)
{
	PifGpsUblox *p_owner = p_task->_p_client;
	uint8_t data[DATA_SIZE];
	uint16_t size, i;
#ifndef PIF_NO_LOG
	int line;
	static uint32_t timer1ms;
#endif

	if (!p_owner->__length) {
		if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c_device, 0xFD, &p_owner->__length)) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
		if (!p_owner->__length) return 0;
		if (p_owner->__length == 0xFFFF) { p_owner->__length = 0; return 0; }
		if (p_owner->__length & 0x8000) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
#ifndef PIF_NO_LOG
		timer1ms = pif_cumulative_timer1ms;
		pifLog_Printf(LT_INFO, "GU(%u): Start L=%d bytes", __LINE__, p_owner->__length);
#endif
	}

	size = p_owner->__length > DATA_SIZE ? DATA_SIZE : p_owner->__length;
	if (!pifI2cDevice_Read(p_owner->_p_i2c_device, 0, 0, data, size)) {
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
	}
	for (i = 0; i < size; i++) {
		_parsingPacket(p_owner, data[i]);
	}
	p_owner->__length -= size;
	if (p_owner->__length) pifTask_SetTrigger(p_task);
#ifndef PIF_NO_LOG
	else {
		pifLog_Printf(LT_INFO, "GU(%u): End T=%ld ms", __LINE__, pif_cumulative_timer1ms - timer1ms);
	}
#endif
	return 0;

fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "GU(%u): len=%d", line, p_owner->__length);
#endif
	p_owner->__length = 0;
	return 0;
}

static BOOL _checkAbortSerial(PifIssuerP p_issuer)
{
	PifGpsUblox* p_owner = (PifGpsUblox*)p_issuer;

	return pifRingBuffer_IsEmpty(&p_owner->__tx.buffer);
}

static BOOL _makeNmeaPacket(PifGpsUblox* p_owner, char* p_data, uint16_t waiting)
{
	uint32_t header;
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

	if (!pifRingBuffer_MoveHeadForLinear(&p_owner->__tx.buffer, 4 + i)) goto fail;

	pifRingBuffer_BeginPutting(&p_owner->__tx.buffer);

	header = i;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t*)&header, 4)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t*)p_data, header)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.buffer);

	if (p_owner->__p_uart) {
		pifTask_SetTrigger(p_owner->__p_uart->_p_task);

		pifTaskManager_YieldAbortMs(waiting, _checkAbortSerial, p_owner);
	}
	else {
		if (!pifI2cDevice_Write(p_owner->_p_i2c_device, 0, 0, pifRingBuffer_GetTailPointer(&p_owner->__tx.buffer, 4), i)) goto fail;
		pifRingBuffer_Remove(&p_owner->__tx.buffer, 4 + i);
	}
	p_owner->_request_state = GURS_TIMEOUT;
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.buffer);
	return FALSE;
}

static BOOL _checkAbortSerialResponse(PifIssuerP p_issuer)
{
	PifGpsUblox* p_owner = (PifGpsUblox*)p_issuer;

	if (pifRingBuffer_IsEmpty(&p_owner->__tx.buffer)) {
		switch (p_owner->_request_state) {
		case GURS_ACK:
		case GURS_NAK:
			return TRUE;

		default:
			break;
		}
	}
	return FALSE;
}

static BOOL _checkAbortI2cResponse(PifIssuerP p_issuer)
{
	PifGpsUblox* p_owner = (PifGpsUblox*)p_issuer;

	switch (p_owner->_request_state) {
	case GURS_ACK:
	case GURS_NAK:
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

static BOOL _makeUbxPacket(PifGpsUblox* p_owner, uint8_t* p_header, uint16_t length, uint8_t* p_payload, uint16_t waiting)
{
	uint32_t info;
	uint8_t tailer[2];
	uint16_t checksum;

	checksum = _checksumUbx(p_header + 2, p_payload, length);
	tailer[0] = checksum & 0xFF;
	tailer[1] = checksum >> 8;

	if (!pifRingBuffer_MoveHeadForLinear(&p_owner->__tx.buffer, 12 + length)) goto fail;

	pifRingBuffer_BeginPutting(&p_owner->__tx.buffer);

	info = length + 8;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, (uint8_t*)&info, 4)) goto fail;
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_header, 6)) goto fail;
	if (length > 0) {
		if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, p_payload, length)) goto fail;
	}
	if (!pifRingBuffer_PutData(&p_owner->__tx.buffer, tailer, 2)) goto fail;

	pifRingBuffer_CommitPutting(&p_owner->__tx.buffer);

	if (p_owner->__p_uart) {
		pifTask_SetTrigger(p_owner->__p_uart->_p_task);

		if (p_owner->_request_state == GURS_SEND) {
			pifTaskManager_YieldAbortMs(waiting, _checkAbortSerialResponse, p_owner);
			if (p_owner->_request_state == GURS_SEND) p_owner->_request_state = GURS_TIMEOUT;
		}
		else {
			pifTaskManager_YieldAbortMs(waiting, _checkAbortSerial, p_owner);
			p_owner->_request_state = GURS_TIMEOUT;
		}
	}
	else if (p_owner->_p_i2c_device) {
		if (!pifI2cDevice_Write(p_owner->_p_i2c_device, 0, 0, pifRingBuffer_GetTailPointer(&p_owner->__tx.buffer, 4), 8 + length)) goto fail;
		pifRingBuffer_Remove(&p_owner->__tx.buffer, 12 + length);

		if (p_owner->_request_state == GURS_SEND) {
			pifTaskManager_YieldAbortMs(waiting, _checkAbortI2cResponse, p_owner);
			if (p_owner->_request_state == GURS_SEND) p_owner->_request_state = GURS_TIMEOUT;
		}
		else {
			p_owner->_request_state = GURS_TIMEOUT;
		}
	}
	return TRUE;

fail:
	pifRingBuffer_RollbackPutting(&p_owner->__tx.buffer);
	p_owner->_request_state = GURS_FAILURE;
	return FALSE;
}

static uint16_t _evtSending(void* p_client, PifActUartSendData act_send_data)
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
		length = (*act_send_data)(p_owner->__p_uart, pifRingBuffer_GetTailPointer(&p_owner->__tx.buffer, p_owner->__tx.pos),
				pifRingBuffer_GetLinerSize(&p_owner->__tx.buffer, p_owner->__tx.pos));
		p_owner->__tx.pos += length;
		if (p_owner->__tx.pos >= 4 + p_owner->__tx.ui.st.length) {
			p_owner->__tx.state = GUTS_WAIT_SENDED;
		}
		break;

	case GUTS_WAIT_SENDED:
		if (!p_owner->__tx.ui.st.response) {
			pifRingBuffer_Remove(&p_owner->__tx.buffer, 4 + p_owner->__tx.ui.st.length);
			p_owner->__tx.state = GUTS_IDLE;
		}
		break;

	default:
		break;
	}
	return 0;
}

static void _evtAbortRx(void* p_client)
{
	((PifGpsUblox *)p_client)->__rx.state = GURS_SYNC_CHAR_1;
}

static BOOL _checkAbortBlocking(PifIssuerP p_issuer)
{
	return ((PifGpsUblox*)p_issuer)->__tx.state == GUTS_IDLE;
}

static BOOL _checkBlocking(PifGpsUblox* p_owner, BOOL blocking)
{
	if (blocking) {
		pifTaskManager_YieldAbort(_checkAbortBlocking, p_owner);
	}
	else {
		if (p_owner->__tx.state != GUTS_IDLE) {
			p_owner->_request_state = GURS_FAILURE;
			pif_error = E_INVALID_STATE;
			return FALSE;
		}
	}
	return TRUE;
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
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "MWP:%u(%u) EC:%d", __LINE__, id, pif_error);
#endif
    return FALSE;
}

void pifGpsUblox_Clear(PifGpsUblox* p_owner)
{
	pifRingBuffer_Clear(&p_owner->__tx.buffer);
}

void pifGpsUblox_AttachUart(PifGpsUblox* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
	p_uart->evt_abort_rx = _evtAbortRx;
}

void pifGpsUblox_DetachUart(PifGpsUblox* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifGpsUblox_AttachI2c(PifGpsUblox* p_owner, PifI2cPort* p_i2c, uint8_t addr, void *p_client, uint16_t period, BOOL start, const char* name)
{
    p_owner->_p_i2c_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_owner->_p_i2c_device) goto fail;

    p_owner->_p_task = pifTaskManager_Add(TM_PERIOD_MS, period, _doTask, p_owner, start);
	if (!p_owner->_p_task) goto fail;

    p_owner->__p_i2c_port = p_i2c;
	if (name) p_owner->_p_task->name = name;
	else p_owner->_p_task->name = "GPS UBLOX";
	p_owner->_p_i2c_device->timeout = 200;
    return TRUE;

fail:
	pifGpsUblox_DetachI2c(p_owner);
	return FALSE;
}

void pifGpsUblox_DetachI2c(PifGpsUblox* p_owner)
{
	if (p_owner->_p_i2c_device) {
		pifI2cPort_RemoveDevice(p_owner->__p_i2c_port, p_owner->_p_i2c_device);
		p_owner->_p_i2c_device = NULL;
	}
}

BOOL pifGpsUblox_PollRequestGBQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting)
{
	char data[16] = "$GBGBQ,";
	int i;

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGLQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting)
{
	char data[16] = "$GLGLQ,";
	int i;

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGNQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting)
{
	char data[16] = "$GNGNQ,";
	int i;

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_PollRequestGPQ(PifGpsUblox* p_owner, const char* p_mag_id, BOOL blocking, uint16_t waiting)
{
	char data[16] = "$GPGPQ,";
	int i;

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	i = 0;
	while (p_mag_id[i]) {
		data[7 + i] = p_mag_id[i];
		i++;
	}
	data[7 + i] = '*';

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SetPubxConfig(PifGpsUblox* p_owner, uint8_t port_id, uint16_t in_proto, uint16_t out_proto, uint32_t baudrate, BOOL blocking, uint16_t waiting)
{
	char data[40];

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	pif_Printf(data, sizeof(data), "$PUBX,41,%u,%4X,%4X,%lu,0*", port_id, in_proto, out_proto, baudrate);

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SetPubxRate(PifGpsUblox* p_owner, const char* p_mag_id, uint8_t rddc, uint8_t rus1, uint8_t rus2, uint8_t rusb, uint8_t rspi, BOOL blocking, uint16_t waiting)
{
	char data[40];

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	pif_Printf(data, sizeof(data), "$PUBX,40,%s,%u,%u,%u,%u,%u,0*", p_mag_id, rddc, rus1, rus2, rusb, rspi);

	return _makeNmeaPacket(p_owner, data, waiting);
}

BOOL pifGpsUblox_SendUbxMsg(PifGpsUblox* p_owner, uint8_t class_id, uint8_t msg_id, uint16_t length, uint8_t* payload, BOOL blocking, uint16_t waiting)
{
	uint8_t header[6] = { 0xB5, 0x62 };

	if (!_checkBlocking(p_owner, blocking)) return FALSE;

	header[2] = class_id;
	header[3] = msg_id;
	header[4] = length & 0xFF;
	header[5] = length >> 8;

	if (class_id == GUCI_CFG) {
		p_owner->_request_state = GURS_SEND;
		p_owner->__cfg_msg_id = msg_id;
	}
	else {
		p_owner->_request_state = GURS_NONE;
	}

	return _makeUbxPacket(p_owner, header, length, payload, waiting);
}
