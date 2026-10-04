// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "rc/pif_rc_ibus.h"


// A receiver writes a frame out back to back and leaves several milliseconds between frames
// (about 7 ms for servo frames). A pause of this many milliseconds inside a frame therefore
// means the frame was lost, and the next byte starts a new one.
#define IBUS_FRAME_GAP_MS		3

// iA6 (the older receiver) frames: a 0x55 header, 14 channel words, then a checksum word.
#define IBUS_IA6_HEADER			0x55
#define IBUS_IA6_FRAME_SIZE		(1 + 2 * PIF_IBUS_CHANNEL_COUNT + 2)


static uint16_t _getWord(const uint8_t* p_data)
{
	return p_data[0] | (p_data[1] << 8);
}

/**
 * @brief Checks the trailing checksum word of the frame in the receive buffer.
 *        iA6B frames carry 0xFFFF minus the byte sum of everything before the checksum.
 *        iA6 frames carry the sum of the channel words that follow the header.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @return TRUE when the checksum matches.
 */
static BOOL _isChecksumValid(PifRcIbus *p_owner)
{
	const uint8_t* p_frame = p_owner->__rx_buffer;
	uint8_t body = p_owner->_length - 2;
	uint16_t expected = 0;
	int i;

	if (p_owner->_model == IBUS_MODEL_IA6) {
		for (i = 1; i < body; i += 2) expected += _getWord(p_frame + i);
	}
	else {
		expected = 0xFFFF - (uint16_t)pifCheckSum((uint8_t*)p_frame, body);
	}
	return expected == _getWord(p_frame + body);
}

/**
 * @brief Hands a completed, checksum-verified frame to the client.
 *
 * Each channel word holds a 12-bit value. When the transmitter sends more than 14 channels,
 * the spare top nibbles of the words carry channels 15 to 18: channel 15 + k is built from
 * the top nibbles of words 3k, 3k + 1 and 3k + 2, lowest nibble first.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @return IBUS_FRAME_SERVO after the channels went to evt_receive, IBUS_FRAME_TELEMETRY when the
 *         frame is a sensor request for us (its command and address are in _tlm_command and
 *         _tlm_address), otherwise IBUS_FRAME_NONE.
 */
static PifRcIbusFrame _processFrame(PifRcIbus *p_owner)
{
	uint16_t channel[PIF_IBUS_EXP_CHANNEL_COUNT];
	uint16_t word[PIF_IBUS_CHANNEL_COUNT];
	const uint8_t* p_words;
	uint8_t command = p_owner->__rx_buffer[1] & 0xF0;
	uint8_t address = p_owner->__rx_buffer[1] & 0x0F;
	int c, k;

	p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

	if (p_owner->_model == IBUS_MODEL_IA6 || (p_owner->_length == IBUS_FRAME_SIZE && command == IBUS_COMMAND_SERVO)) {
		// iA6 has no command byte, so its words start right after the header.
		p_words = p_owner->__rx_buffer + (p_owner->_model == IBUS_MODEL_IA6 ? 1 : 2);
		for (c = 0; c < PIF_IBUS_CHANNEL_COUNT; c++) {
			word[c] = _getWord(p_words + 2 * c);
			channel[c] = word[c] & 0x0FFF;
		}
		for (k = 0; k < PIF_IBUS_EXP_CHANNEL_COUNT - PIF_IBUS_CHANNEL_COUNT; k++) {
			channel[PIF_IBUS_CHANNEL_COUNT + k] = (word[3 * k] >> 12) | ((word[3 * k + 1] >> 12) << 4) | ((word[3 * k + 2] >> 12) << 8);
		}

		if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
		return IBUS_FRAME_SERVO;
	}

	// A request from the receiver to a sensor is always a bare 4-byte frame. Longer frames with an
	// address are sensor replies, which on a half-duplex line includes our own replies read back.
	if (address && p_owner->_length == IBUS_TELEMETRY_SIZE) {
		p_owner->_tlm_command = command;
		p_owner->_tlm_address = address;
		return IBUS_FRAME_TELEMETRY;
	}
	return IBUS_FRAME_NONE;
}

/**
 * @brief Collects one byte into the receive buffer and checks the frame once it is complete.
 *
 * The first byte of a frame tells its size: 0x20 for an iA6B servo frame, 0x04 for a sensor
 * request, or the 0x55 header of an iA6 frame. Any other first byte is skipped.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param data Received byte.
 * @return What the byte completed; see _processFrame().
 */
static PifRcIbusFrame _parsingPacket(PifRcIbus *p_owner, uint8_t data)
{
	if (p_owner->__ptr && pif_cumulative_timer1ms - p_owner->__last_time >= IBUS_FRAME_GAP_MS) {
		p_owner->__ptr = 0;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

	if (!p_owner->__ptr) {
		switch (data) {
		case IBUS_FRAME_SIZE:
		case IBUS_TELEMETRY_SIZE:
			p_owner->_model = IBUS_MODEL_IA6B;
			p_owner->_length = data;
			break;

		case IBUS_IA6_HEADER:
			p_owner->_model = IBUS_MODEL_IA6;
			p_owner->_length = IBUS_IA6_FRAME_SIZE;
			break;

		default:
			return IBUS_FRAME_NONE;
		}
	}

	p_owner->__rx_buffer[p_owner->__ptr++] = data;
	if (p_owner->__ptr < p_owner->_length) return IBUS_FRAME_NONE;

	p_owner->__ptr = 0;
	if (!_isChecksumValid(p_owner)) {
		p_owner->parent._error_frames++;
		return IBUS_FRAME_NONE;
	}
	p_owner->parent._good_frames++;
	return _processFrame(p_owner);
}

/**
 * @brief Parses the bytes the UART has, dispatches the channel data and answers sensor requests.
 * @param p_client Pointer to the protocol receiver instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a frame was completed or one is still being received; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifRcIbus *p_owner = (PifRcIbus *)p_client;
	PifRcIbusFrame frame;
	uint8_t data;
	BOOL rtn = FALSE;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		frame = _parsingPacket(p_owner, data);
		if (frame != IBUS_FRAME_NONE) {
			if (frame == IBUS_FRAME_TELEMETRY) {
				pifRcIbus_SendTelemetry(p_owner, p_owner->_tlm_command, p_owner->_tlm_address);
			}
			rtn = TRUE;
			// One frame per call, as the receiver sends no more than that between two runs.
			break;
		}
	}
	return rtn || p_owner->__ptr > 0;
}

BOOL pifRcIbus_Init(PifRcIbus* p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcIbus));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._channel_count = PIF_IBUS_CHANNEL_COUNT;
	p_owner->parent._failsafe = FALSE;
    return TRUE;
}

void pifRcIbus_Clear(PifRcIbus* p_owner)
{
	memset(p_owner, 0, sizeof(PifRcIbus));
}

void pifRcIbus_AttachUart(PifRcIbus* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifRcIbus_DetachUart(PifRcIbus* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

PifRcIbusFrame pifRcIbus_ParsingPacket(PifRcIbus* p_owner, uint8_t data)
{
	return _parsingPacket(p_owner, data);
}

BOOL pifRcIbus_SendTelemetry(PifRcIbus* p_owner, uint8_t command, uint8_t address)
{
	PifRcIbusSensorinfo sensor;
	uint8_t tx_buffer[IBUS_FRAME_SIZE + 1];
	uint16_t chksum;
	uint8_t p = 0;
	int i;

	if (!p_owner->evt_telemetry || !p_owner->__p_uart) return FALSE;
	if (!p_owner->__p_uart->_p_tx_buffer && !p_owner->__p_uart->act_send_data) return FALSE;

	// The client decides which addresses are its sensors: an address it declines gets no reply,
	// discovery included, so the receiver does not go on to query it.
	memset(&sensor, 0, sizeof(sensor));
	switch (command) {
	case IBUS_COMMAND_DISCOVER:
		if (!(*p_owner->evt_telemetry)(p_owner, command, address, &sensor)) return FALSE;
		// Reply: length 4, the command with our address, checksum
		tx_buffer[p++] = 0x04;
		tx_buffer[p++] = IBUS_COMMAND_DISCOVER + address;
		break;

	case IBUS_COMMAND_TYPE:
		if (!(*p_owner->evt_telemetry)(p_owner, command, address, &sensor)) return FALSE;
		// Reply: length 6, the command with our address, sensor type, value size, checksum
		tx_buffer[p++] = 0x06;
		tx_buffer[p++] = IBUS_COMMAND_TYPE + address;
		tx_buffer[p++] = sensor.type;
		tx_buffer[p++] = sensor.length;
		break;

	case IBUS_COMMAND_VALUE:
		if (!(*p_owner->evt_telemetry)(p_owner, command, address, &sensor)) return FALSE;
		if (sensor.length > sizeof(sensor.value)) return FALSE;
		// Reply: length 4 + value size, the command with our address, value bytes, checksum
		tx_buffer[p++] = 0x04 + sensor.length;
		tx_buffer[p++] = IBUS_COMMAND_VALUE + address;
		for (i = 0; i < sensor.length; i++) {
			tx_buffer[p++] = sensor.value[i];
		}
		break;

	default:
		return FALSE;	// unknown command, nothing to send
	}

	chksum = 0xFFFF - pifCheckSum(tx_buffer, p);
	tx_buffer[p++] = chksum & 0x0ff;
	tx_buffer[p++] = chksum >> 8;

	// Through the UART rather than into its buffer, so the TX task is triggered.
	return pifUart_SendTxData(p_owner->__p_uart, tx_buffer, p) == p;
}

BOOL pifRcIbus_SendFrame(PifRcIbus* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t i, buffer[IBUS_FRAME_SIZE];
	uint8_t p = 0;
	uint16_t crc;

	buffer[p++] = IBUS_FRAME_SIZE;
	buffer[p++] = IBUS_COMMAND_SERVO;
	for (i = 0; i < PIF_IBUS_CHANNEL_COUNT; i++) {
		if (i < count) {
			buffer[p++] = p_channel[i] & 0xFF;
			buffer[p++] = p_channel[i] >> 8;
		}
		else {
			buffer[p++] = 0;
			buffer[p++] = 0;
		}
	}
	crc = 0xFFFF;
	for (i = 0; i < p; i++) crc -= buffer[i];
	buffer[p++] = crc & 0xFF;
	buffer[p++] = crc >> 8;

	return pifUart_SendTxData(p_owner->__p_uart, buffer, p);
}
