// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_sbus.h"


// S.BUS frame (25 bytes): a start byte, 16 channels of 11 bits packed into 22 bytes with the least
// significant bit first, a flag byte, and an end byte.
#define SBUS_STARTBYTE         	0x0F
#define SBUS_ENDBYTE           	0x00
#define SBUS2_ENDBYTE_MASK		0xCF	// S.BUS2 ends a frame with 0x04, 0x14, 0x24 or 0x34
#define SBUS2_ENDBYTE			0x04
#define SBUS_PACKED_CHANNELS	16
#define SBUS_CHANNEL_BITS		11
#define SBUS_CHANNEL_MAX		((1 << SBUS_CHANNEL_BITS) - 1)
#define SBUS_FLAGS_INDEX		23

// Flag byte
#define SBUS_FLAG_CH17			0x01	// Digital channel 17
#define SBUS_FLAG_CH18			0x02	// Digital channel 18
#define SBUS_FLAG_FRAME_LOST	0x04	// The receiver missed a frame from the transmitter
#define SBUS_FLAG_FAILSAFE		0x08	// The receiver is in failsafe

// A frame takes 3 ms at 100000 baud and frames come every 7 or 14 ms, so a pause of this many
// milliseconds inside a frame means the frame was cut, and the next byte starts a new one.
#define SBUS_FRAME_GAP_MS		3
#define SBUS_MAX_FRAME_PERIOD	100		// ms without a frame before the link counts as lost

// Futaba S.BUS timing: one raw step is 0.625 us (5/8 us), and the raw value 1024 is the
// 1520 us centre of a servo pulse.
#define SBUS_CENTER_RAW			1024
#define SBUS_CENTER_US			1520

// Raw values given to the digital channels 17 and 18: 988 us when off and 2012 us when on.
#define SBUS_DIGITAL_OFF		173
#define SBUS_DIGITAL_ON			1811


/**
 * @brief Converts a raw 11-bit S.BUS value to a servo pulse width, rounded to the nearest us.
 * @param raw Raw channel value, 0 to 2047.
 * @return Pulse width in microseconds.
 */
static uint16_t _rawToPulse(uint16_t raw)
{
	int32_t eighths = ((int32_t)raw - SBUS_CENTER_RAW) * 5;		// Offset from the centre in 1/8 us

	return SBUS_CENTER_US + (eighths >= 0 ? eighths + 4 : eighths - 4) / 8;
}

/**
 * @brief Splits the packed channel bytes of a frame into 16 channel values.
 * @param p_data The 22 packed bytes that follow the start byte.
 * @param p_channel Destination for SBUS_PACKED_CHANNELS raw values.
 * @return None.
 */
static void _unpackChannels(const uint8_t* p_data, uint16_t* p_channel)
{
	uint32_t bits = 0;		// Bits taken from the bytes but not yet handed out, lowest first
	uint8_t count = 0;		// Number of such bits
	int c;

	for (c = 0; c < SBUS_PACKED_CHANNELS; c++) {
		while (count < SBUS_CHANNEL_BITS) {
			bits |= (uint32_t)*p_data++ << count;
			count += 8;
		}
		p_channel[c] = bits & SBUS_CHANNEL_MAX;
		bits >>= SBUS_CHANNEL_BITS;
		count -= SBUS_CHANNEL_BITS;
	}
}

/**
 * @brief Packs 16 channel values into the 22 channel bytes of a frame.
 * @param p_channel Raw channel values.
 * @param count Number of values in p_channel. Channels from count on are sent as 0.
 * @param p_data Destination for the 22 packed bytes.
 * @return None.
 */
static void _packChannels(const uint16_t* p_channel, uint8_t count, uint8_t* p_data)
{
	uint32_t bits = 0;
	uint8_t length = 0;
	int c;

	for (c = 0; c < SBUS_PACKED_CHANNELS; c++) {
		bits |= (uint32_t)(c < count ? p_channel[c] & SBUS_CHANNEL_MAX : 0) << length;
		length += SBUS_CHANNEL_BITS;
		while (length >= 8) {
			*p_data++ = bits & 0xFF;
			bits >>= 8;
			length -= 8;
		}
	}
}

/**
 * @brief Tells whether a byte can end a frame: 0x00 for S.BUS, or 0x04, 0x14, 0x24, 0x34 for S.BUS2.
 * @param data The last byte of a frame.
 * @return TRUE when it is an end byte.
 */
static BOOL _isEndByte(uint8_t data)
{
	return data == SBUS_ENDBYTE || (data & SBUS2_ENDBYTE_MASK) == SBUS2_ENDBYTE;
}

/**
 * @brief Drops a frame that failed its end byte check, keeping the bytes from the next start byte
 *        inside it on, since that one may be the real start of a frame.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @return None.
 */
static void _resync(PifRcSbus *p_owner)
{
	uint8_t i;

	for (i = 1; i < SBUS_FRAME_SIZE; i++) {
		if (p_owner->__buffer[i] == SBUS_STARTBYTE) break;
	}
	p_owner->__index = SBUS_FRAME_SIZE - i;
	if (p_owner->__index) memmove(p_owner->__buffer, p_owner->__buffer + i, p_owner->__index);
}

/**
 * @brief Parses incoming UART bytes, validates the protocol frame, updates receiver status, and dispatches channel data.
 * @param p_client Pointer to the protocol receiver instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a full frame is parsed or parser state remains active; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive_data)
{
	PifRcSbus *p_owner = (PifRcSbus *)p_client;
	uint8_t i, data, flags;
	uint8_t* p_buffer;
	uint16_t channels[PIF_SBUS_CHANNEL_COUNT];
	BOOL rtn = FALSE;

	p_buffer = p_owner->__buffer;

	while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1)) {
		// Timed per byte: the UART keeps calling this while a frame is open, so a time taken per
		// call would never show the pause.
		if (p_owner->__index && pif_cumulative_timer1ms - p_owner->__last_time >= SBUS_FRAME_GAP_MS) {
			p_owner->__index = 0;
		}
		p_owner->__last_time = pif_cumulative_timer1ms;

		// Bytes before a start byte belong to a frame whose beginning was missed.
		if (!p_owner->__index && data != SBUS_STARTBYTE) continue;

		p_buffer[p_owner->__index++] = data;
		if (p_owner->__index < SBUS_FRAME_SIZE) continue;

		// Without the end byte in place, the "start byte" was a data byte of some other frame.
		if (!_isEndByte(p_buffer[SBUS_FRAME_SIZE - 1])) {
			p_owner->parent._error_frames++;
			_resync(p_owner);
			continue;
		}
		p_owner->__index = 0;

		p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

		flags = p_buffer[SBUS_FLAGS_INDEX];
		_unpackChannels(p_buffer + 1, channels);
		channels[SBUS_PACKED_CHANNELS] = (flags & SBUS_FLAG_CH17) ? SBUS_DIGITAL_ON : SBUS_DIGITAL_OFF;
		channels[SBUS_PACKED_CHANNELS + 1] = (flags & SBUS_FLAG_CH18) ? SBUS_DIGITAL_ON : SBUS_DIGITAL_OFF;
		p_owner->parent._failsafe = (flags & SBUS_FLAG_FAILSAFE) != 0;

		if (flags & SBUS_FLAG_FRAME_LOST) {
			p_owner->parent._lost_frames++;
		}
		else {
			p_owner->parent._good_frames++;
			for (i = 0; i < PIF_SBUS_CHANNEL_COUNT; i++) {
				channels[i] = _rawToPulse(channels[i]);
			}
			if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channels, p_owner->parent.__p_issuer);
		}
		rtn = TRUE;
		break;
	}
	return rtn || p_owner->__index > 0;
}

BOOL pifRcSbus_Init(PifRcSbus* p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcSbus));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._channel_count = PIF_SBUS_CHANNEL_COUNT;
	p_owner->parent._failsafe = TRUE;
	p_owner->parent._max_frame_period = SBUS_MAX_FRAME_PERIOD;
    return TRUE;
}

void pifRcSbus_AttachUart(PifRcSbus* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, p_owner, _evtParsing, NULL, NULL);
}

void pifRcSbus_DetachUart(PifRcSbus* p_owner)
{
	if (!p_owner->__p_uart) return;
	pifTransport_DetachClient(&p_owner->__p_uart->parent);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSbus_SendFrame(PifRcSbus* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t buffer[SBUS_FRAME_SIZE];

	if (!p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	buffer[0] = SBUS_STARTBYTE;
	_packChannels(p_channel, count, buffer + 1);
	buffer[SBUS_FLAGS_INDEX] = 0;
	buffer[SBUS_FRAME_SIZE - 1] = SBUS_ENDBYTE;

	return pifUart_SendTxData(p_owner->__p_uart, buffer, SBUS_FRAME_SIZE) == SBUS_FRAME_SIZE;
}
