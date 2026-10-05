// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_crsf.h"


// Frames come back to back at 420000 baud, so a pause of this many milliseconds inside a frame
// means bytes were lost, and the next byte starts a new frame.
#define CRSF_FRAME_GAP_MS			2

#define CRSF_LENGTH_MIN				2		// Type and CRC
#define CRSF_LENGTH_MAX				(PIF_CRSF_FRAME_SIZE_MAX - 2)
#define CRSF_CHANNEL_BITS			11
#define CRSF_CHANNELS_PAYLOAD		22		// 16 channels of 11 bits
#define CRSF_LINK_PAYLOAD			10


static void _putU16(uint8_t* p, uint16_t value)
{
	p[0] = (uint8_t)(value >> 8);
	p[1] = (uint8_t)value;
}

static void _putU32(uint8_t* p, uint32_t value)
{
	p[0] = (uint8_t)(value >> 24);
	p[1] = (uint8_t)(value >> 16);
	p[2] = (uint8_t)(value >> 8);
	p[3] = (uint8_t)value;
}

/**
 * @brief Tells whether a byte can start a frame: the address of a device that frames are sent to.
 */
static BOOL _isFrameStart(uint8_t data)
{
	return data == PIF_CRSF_ADDRESS_FLIGHT_CONTROLLER || data == PIF_CRSF_ADDRESS_RADIO_TRANSMITTER ||
			data == PIF_CRSF_ADDRESS_RECEIVER || data == PIF_CRSF_ADDRESS_TRANSMITTER;
}

/**
 * @brief Hands a complete frame with a good CRC to the client.
 * @param p_owner Pointer to the instance.
 * @return What the frame was.
 */
static PifRcCrsfFrame _processFrame(PifRcCrsf* p_owner)
{
	const uint8_t* p_frame = p_owner->__rx_buffer;
	const uint8_t* p_payload = p_frame + 3;
	uint8_t type = p_frame[2], length = p_frame[1] - 2;
	uint16_t channel[PIF_CRSF_CHANNEL_COUNT];
	uint32_t bits = 0;
	uint8_t c, count = 0;

	if (type == PIF_CRSF_FRAME_RC_CHANNELS && length == CRSF_CHANNELS_PAYLOAD) {
		// 11 bits per channel, least significant bit first.
		for (c = 0; c < PIF_CRSF_CHANNEL_COUNT; c++) {
			while (count < CRSF_CHANNEL_BITS) {
				bits |= (uint32_t)*p_payload++ << count;
				count += 8;
			}
			channel[c] = bits & ((1 << CRSF_CHANNEL_BITS) - 1);
			bits >>= CRSF_CHANNEL_BITS;
			count -= CRSF_CHANNEL_BITS;
		}
		p_owner->parent._last_frame_time = pif_cumulative_timer1ms;
		p_owner->parent._failsafe = FALSE;
		if (p_owner->parent.__evt_receive) {
			(*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
		}
		return CRSF_FRAME_CHANNELS;
	}

	if (type == PIF_CRSF_FRAME_LINK_STATISTICS && length == CRSF_LINK_PAYLOAD) {
		p_owner->_link.uplink_rssi_ant1 = p_payload[0];
		p_owner->_link.uplink_rssi_ant2 = p_payload[1];
		p_owner->_link.uplink_link_quality = p_payload[2];
		p_owner->_link.uplink_snr = (int8_t)p_payload[3];
		p_owner->_link.active_antenna = p_payload[4];
		p_owner->_link.rf_mode = p_payload[5];
		p_owner->_link.uplink_tx_power = p_payload[6];
		p_owner->_link.downlink_rssi = p_payload[7];
		p_owner->_link.downlink_link_quality = p_payload[8];
		p_owner->_link.downlink_snr = (int8_t)p_payload[9];
		if (p_owner->evt_link) (*p_owner->evt_link)(p_owner);
		return CRSF_FRAME_LINK;
	}

	if (p_owner->evt_frame) (*p_owner->evt_frame)(p_owner, p_frame[0], type, p_payload, length);
	return CRSF_FRAME_OTHER;
}

static PifRcCrsfFrame _parsingPacket(PifRcCrsf* p_owner, uint8_t data)
{
	uint8_t* p_frame = p_owner->__rx_buffer;
	uint8_t size;

	if (p_owner->__ptr && pif_cumulative_timer1ms - p_owner->__last_time >= CRSF_FRAME_GAP_MS) {
		p_owner->__ptr = 0;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

	if (p_owner->__ptr == 0) {
		if (_isFrameStart(data)) p_frame[p_owner->__ptr++] = data;
		return CRSF_FRAME_NONE;
	}
	if (p_owner->__ptr == 1) {
		if (data < CRSF_LENGTH_MIN || data > CRSF_LENGTH_MAX) {
			// Not a length: start over, this byte possibly being the start of the next frame.
			p_owner->__ptr = 0;
			if (_isFrameStart(data)) p_frame[p_owner->__ptr++] = data;
			return CRSF_FRAME_NONE;
		}
	}

	p_frame[p_owner->__ptr++] = data;
	size = p_frame[1] + 2;
	if (p_owner->__ptr < size) return CRSF_FRAME_NONE;

	p_owner->__ptr = 0;
	if (pifCrc8_Update(0, p_frame + 2, size - 3, PIF_CRC8_POLY_DVB_S2) != p_frame[size - 1]) {
		p_owner->parent._error_frames++;
		return CRSF_FRAME_NONE;
	}
	p_owner->parent._good_frames++;
	return _processFrame(p_owner);
}

/**
 * @brief Parses the bytes the UART has.
 * @param p_client Pointer to the instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a frame was completed or one is still being received; otherwise FALSE.
 */
static BOOL _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifRcCrsf* p_owner = (PifRcCrsf*)p_client;
	uint8_t data;
	BOOL rtn = FALSE;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		if (_parsingPacket(p_owner, data) != CRSF_FRAME_NONE) rtn = TRUE;
	}
	return rtn || p_owner->__ptr > 0;
}

/**
 * @brief Sends a built frame through the attached UART.
 */
static BOOL _send(PifRcCrsf* p_owner, uint8_t* p_frame, uint8_t size)
{
	if (!size || !p_owner->__p_uart) return FALSE;
	return pifUart_SendTxData(p_owner->__p_uart, p_frame, size) == size;
}

BOOL pifRcCrsf_Init(PifRcCrsf* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcCrsf));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->parent._id = id;
	p_owner->parent._channel_count = PIF_CRSF_CHANNEL_COUNT;
	p_owner->parent._max_frame_period = PIF_CRSF_DEFAULT_TIMEOUT_MS;
	p_owner->_address = PIF_CRSF_ADDRESS_FLIGHT_CONTROLLER;
	return TRUE;
}

void pifRcCrsf_Clear(PifRcCrsf* p_owner)
{
	memset(p_owner, 0, sizeof(PifRcCrsf));
}

void pifRcCrsf_AttachUart(PifRcCrsf* p_owner, PifUart* p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifRcCrsf_DetachUart(PifRcCrsf* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

void pifRcCrsf_SetAddress(PifRcCrsf* p_owner, uint8_t address)
{
	p_owner->_address = address;
}

void pifRcCrsf_SetTimeout(PifRcCrsf* p_owner, uint16_t timeout_ms)
{
	p_owner->parent._max_frame_period = timeout_ms;
}

PifRcCrsfFrame pifRcCrsf_ParsingPacket(PifRcCrsf* p_owner, uint8_t data)
{
	return _parsingPacket(p_owner, data);
}

/**
 * @brief Divides rounding down, also for a negative dividend.
 */
static int32_t _floorDiv(int32_t dividend, int32_t divisor)
{
	return dividend >= 0 ? dividend / divisor : -((-dividend + divisor - 1) / divisor);
}

uint16_t pifRcCrsf_RawToUs(uint16_t raw)
{
	// 5/8 us per step, rounded to the nearest.
	return (uint16_t)(1500 + _floorDiv(((int32_t)raw - PIF_CRSF_CHANNEL_CENTER) * 5 + 4, 8));
}

uint16_t pifRcCrsf_UsToRaw(uint16_t us)
{
	int32_t raw = PIF_CRSF_CHANNEL_CENTER + _floorDiv(((int32_t)us - 1500) * 8 + 2, 5);

	if (raw < 0) raw = 0;
	else if (raw > (1 << CRSF_CHANNEL_BITS) - 1) raw = (1 << CRSF_CHANNEL_BITS) - 1;
	return (uint16_t)raw;
}

uint8_t pifRcCrsf_BuildFrame(uint8_t* p_buffer, uint8_t address, uint8_t type, const uint8_t* p_payload, uint8_t length)
{
	if (length > PIF_CRSF_PAYLOAD_SIZE_MAX) {
		pif_error = E_INVALID_PARAM;
		return 0;
	}

	p_buffer[0] = address;
	p_buffer[1] = length + 2;
	p_buffer[2] = type;
	if (length) memcpy(p_buffer + 3, p_payload, length);
	p_buffer[3 + length] = pifCrc8_Update(0, p_buffer + 2, length + 1, PIF_CRC8_POLY_DVB_S2);
	return length + 4;
}

uint8_t pifRcCrsf_BuildChannels(uint8_t* p_buffer, uint8_t address, const uint16_t* p_channel, uint8_t count)
{
	uint8_t payload[CRSF_CHANNELS_PAYLOAD];
	uint32_t bits = 0;
	uint8_t c, filled = 0, p = 0;

	for (c = 0; c < PIF_CRSF_CHANNEL_COUNT; c++) {
		bits |= (uint32_t)(c < count ? p_channel[c] & ((1 << CRSF_CHANNEL_BITS) - 1) : 0) << filled;
		filled += CRSF_CHANNEL_BITS;
		while (filled >= 8) {
			payload[p++] = (uint8_t)bits;
			bits >>= 8;
			filled -= 8;
		}
	}
	return pifRcCrsf_BuildFrame(p_buffer, address, PIF_CRSF_FRAME_RC_CHANNELS, payload, sizeof(payload));
}

BOOL pifRcCrsf_SendFrame(PifRcCrsf* p_owner, uint8_t type, const uint8_t* p_payload, uint8_t length)
{
	uint8_t frame[PIF_CRSF_FRAME_SIZE_MAX];

	return _send(p_owner, frame, pifRcCrsf_BuildFrame(frame, p_owner->_address, type, p_payload, length));
}

BOOL pifRcCrsf_SendExtendedFrame(PifRcCrsf* p_owner, uint8_t type, uint8_t destination, uint8_t origin,
		const uint8_t* p_payload, uint8_t length)
{
	uint8_t payload[PIF_CRSF_PAYLOAD_SIZE_MAX];

	if (length > PIF_CRSF_PAYLOAD_SIZE_MAX - 2) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	payload[0] = destination;
	payload[1] = origin;
	if (length) memcpy(payload + 2, p_payload, length);
	return pifRcCrsf_SendFrame(p_owner, type, payload, length + 2);
}

BOOL pifRcCrsf_SendChannels(PifRcCrsf* p_owner, const uint16_t* p_channel, uint8_t count)
{
	uint8_t frame[PIF_CRSF_FRAME_SIZE_MAX];

	return _send(p_owner, frame, pifRcCrsf_BuildChannels(frame, p_owner->_address, p_channel, count));
}

BOOL pifRcCrsf_SendBattery(PifRcCrsf* p_owner, uint16_t voltage_dv, uint16_t current_da, uint32_t consumed_mah,
		uint8_t remaining_percent)
{
	uint8_t payload[8];

	if (consumed_mah > 0xFFFFFF) consumed_mah = 0xFFFFFF;
	_putU16(payload, voltage_dv);
	_putU16(payload + 2, current_da);
	payload[4] = (uint8_t)(consumed_mah >> 16);
	payload[5] = (uint8_t)(consumed_mah >> 8);
	payload[6] = (uint8_t)consumed_mah;
	payload[7] = remaining_percent;
	return pifRcCrsf_SendFrame(p_owner, PIF_CRSF_FRAME_BATTERY, payload, sizeof(payload));
}

BOOL pifRcCrsf_SendGps(PifRcCrsf* p_owner, int32_t latitude, int32_t longitude, uint16_t groundspeed, uint16_t heading,
		int32_t altitude_m, uint8_t satellites)
{
	uint8_t payload[15];

	// The altitude goes as metres + 1000.
	altitude_m += 1000;
	if (altitude_m < 0) altitude_m = 0;
	else if (altitude_m > 0xFFFF) altitude_m = 0xFFFF;

	_putU32(payload, (uint32_t)latitude);
	_putU32(payload + 4, (uint32_t)longitude);
	_putU16(payload + 8, groundspeed);
	_putU16(payload + 10, heading);
	_putU16(payload + 12, (uint16_t)altitude_m);
	payload[14] = satellites;
	return pifRcCrsf_SendFrame(p_owner, PIF_CRSF_FRAME_GPS, payload, sizeof(payload));
}

BOOL pifRcCrsf_SendAttitude(PifRcCrsf* p_owner, int16_t pitch, int16_t roll, int16_t yaw)
{
	uint8_t payload[6];

	_putU16(payload, (uint16_t)pitch);
	_putU16(payload + 2, (uint16_t)roll);
	_putU16(payload + 4, (uint16_t)yaw);
	return pifRcCrsf_SendFrame(p_owner, PIF_CRSF_FRAME_ATTITUDE, payload, sizeof(payload));
}

BOOL pifRcCrsf_SendFlightMode(PifRcCrsf* p_owner, const char* p_mode)
{
	uint8_t payload[PIF_CRSF_PAYLOAD_SIZE_MAX];
	size_t length = strlen(p_mode);

	// The text goes with its terminating NUL.
	if (length > PIF_CRSF_PAYLOAD_SIZE_MAX - 1) length = PIF_CRSF_PAYLOAD_SIZE_MAX - 1;
	memcpy(payload, p_mode, length);
	payload[length] = 0;
	return pifRcCrsf_SendFrame(p_owner, PIF_CRSF_FRAME_FLIGHT_MODE, payload, (uint8_t)(length + 1));
}

BOOL pifRcCrsf_SendVario(PifRcCrsf* p_owner, int16_t vertical_speed)
{
	uint8_t payload[2];

	_putU16(payload, (uint16_t)vertical_speed);
	return pifRcCrsf_SendFrame(p_owner, PIF_CRSF_FRAME_VARIO, payload, sizeof(payload));
}
