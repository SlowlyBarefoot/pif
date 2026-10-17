// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_spektrum.h"


// A frame takes 1.4 ms at 115200 baud and frames come every 11 or 22 ms, so a pause of this many
// milliseconds inside a frame means the frame was cut, and the next byte starts a new one.
#define SPEKTRUM_FRAME_GAP_MS		5

#define SPEKTRUM_SERVO_SLOTS		((SPEKTRUM_FRAME_SIZE - 2) / 2)
#define SPEKTRUM_EMPTY_SLOT			0xFF	// Both bytes of a slot without a channel

#define SPEKTRUM_PULSE_MIN			988		// us for the position 0


/**
 * @brief Puts one servo word into a frame.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param p_data Destination for the two bytes.
 * @param ch Channel id.
 * @param position Raw servo position.
 * @return None.
 */
static void _putServo(PifRcSpektrum *p_owner, uint8_t* p_data, uint8_t ch, uint16_t position)
{
	p_data[0] = (ch << p_owner->__id_shift) | ((position >> 8) & p_owner->__pos_mask);
	p_data[1] = position & 0xFF;
}

/**
 * @brief Takes the channels out of a complete frame in the receive buffer and dispatches them.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @return None.
 */
static void _processFrame(PifRcSpektrum *p_owner)
{
	uint8_t* p_buffer = p_owner->__p_buffer;
	uint8_t id;
	int index;

	// The first byte counts the frames the receiver lost, so its growth since the last frame is
	// what was lost in between.
	if (p_owner->parent._good_frames) {
		p_owner->parent._lost_frames += (uint8_t)(p_buffer[0] - p_owner->__fades);
	}
	p_owner->__fades = p_buffer[0];
	p_owner->parent._good_frames++;

	for (index = 2; index < SPEKTRUM_FRAME_SIZE; index += 2) {
		id = (p_buffer[index] >> p_owner->__id_shift) & p_owner->__id_mask;
		if (id < p_owner->parent._channel_count) {
			p_owner->__channel[id] = SPEKTRUM_PULSE_MIN +
					((((uint16_t)(p_buffer[index] & p_owner->__pos_mask) << 8) | p_buffer[index + 1]) >> p_owner->_pos_shift);
		}
	}
	p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

	// A frame may carry only part of the channels, but two frames carry every channel the
	// transmitter sends, so channels not received yet are never handed out.
	if (p_owner->parent._good_frames < 2) return;
	if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, p_owner->__channel, p_owner->parent.__p_issuer);
}

/**
 * @brief Parses incoming UART bytes, validates the protocol frame, updates receiver status, and dispatches channel data.
 * @param p_client Pointer to the protocol receiver instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a full frame is parsed or parser state remains active; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive_data)
{
	PifRcSpektrum *p_owner = (PifRcSpektrum *)p_client;
	uint8_t data;
	uint8_t* p_buffer;
	BOOL rtn = FALSE;

	p_buffer = p_owner->__p_buffer;

	while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1)) {
		// Timed per byte: the UART keeps calling this while a frame is open, so a time taken per
		// call would never show the pause.
		if (p_owner->__index && pif_cumulative_timer1ms - p_owner->__last_time >= SPEKTRUM_FRAME_GAP_MS) {
			p_owner->__index = 0;
		}
		p_owner->__last_time = pif_cumulative_timer1ms;

		p_buffer[p_owner->__index++] = data;

		if (p_owner->__index == 2) {
			// Without a checksum, the system byte is the only check that the frame start is right.
			if (p_buffer[1] != p_owner->_protocol_id) {
				if (!p_owner->__hunting) {
					p_owner->parent._error_frames++;
					p_owner->__hunting = TRUE;
				}
				// The byte may be the first byte of the frame that really starts here.
				p_buffer[0] = p_buffer[1];
				p_owner->__index = 1;
			}
		}
		else if (p_owner->__index >= SPEKTRUM_FRAME_SIZE) {
			p_owner->__index = 0;
			p_owner->__hunting = FALSE;
			_processFrame(p_owner);
			rtn = TRUE;
			break;
		}
	}
	return rtn || p_owner->__index > 0;
}

BOOL pifRcSpektrum_Init(PifRcSpektrum* p_owner, PifId id, uint8_t protocol_id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcSpektrum));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._failsafe = FALSE;
	p_owner->_protocol_id = protocol_id;
	p_owner->__last_ch = 6;
	switch (protocol_id) {
	case PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2:
		p_owner->parent._channel_count = 7;
		p_owner->parent._max_frame_period = 25;
		p_owner->__id_mask = 0x3F;
		p_owner->__id_shift = 2;
		p_owner->__pos_mask = 0x03;
		p_owner->_pos_shift = 0;
		break;

	case PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSM2:
	case PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX:
		p_owner->parent._channel_count = 8;
		p_owner->parent._max_frame_period = 15;
		p_owner->__id_mask = 0x0F;
		p_owner->__id_shift = 3;
		p_owner->__pos_mask = 0x07;
		p_owner->_pos_shift = 1;
		break;

	case PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS:
		p_owner->parent._channel_count = 8;
		p_owner->parent._max_frame_period = 25;
		p_owner->__id_mask = 0x0F;
		p_owner->__id_shift = 3;
		p_owner->__pos_mask = 0x07;
		p_owner->_pos_shift = 1;
		break;

	default:
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

void pifRcSpektrum_AttachUart(PifRcSpektrum* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, p_owner, _evtParsing, NULL, NULL);
}

void pifRcSpektrum_DetachUart(PifRcSpektrum* p_owner)
{
	if (!p_owner->__p_uart) return;
	pifTransport_DetachClient(&p_owner->__p_uart->parent);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSpektrum_SendFrame(PifRcSpektrum* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t i, ch, buffer[SPEKTRUM_FRAME_SIZE];

	if (!p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// Every frame is full length, so a receiver counting bytes stays in step.
	memset(buffer, SPEKTRUM_EMPTY_SLOT, SPEKTRUM_FRAME_SIZE);
	buffer[0] = 0;
	buffer[1] = p_owner->_protocol_id;
	for (i = 0; i < SPEKTRUM_SERVO_SLOTS - 1 && i < count; i++) {
		_putServo(p_owner, buffer + 2 + 2 * i, i, p_channel[i]);
	}
	if (count > SPEKTRUM_SERVO_SLOTS - 1) {
		// The last slot takes channels 7 and 8 in turn, or channel 7 alone when there is no 8.
		ch = p_owner->__last_ch < count ? p_owner->__last_ch : SPEKTRUM_SERVO_SLOTS - 1;
		_putServo(p_owner, buffer + SPEKTRUM_FRAME_SIZE - 2, ch, p_channel[ch]);
		if (p_owner->parent._channel_count > SPEKTRUM_SERVO_SLOTS && count > SPEKTRUM_SERVO_SLOTS) p_owner->__last_ch ^= 1;
	}

	return pifUart_SendTxData(p_owner->__p_uart, buffer, SPEKTRUM_FRAME_SIZE) == SPEKTRUM_FRAME_SIZE;
}
