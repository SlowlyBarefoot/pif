// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc_sumd.h"


#define SUMD_VENDOR_ID         	0xA8
#define SUMD_STATUS_VALID      	0x01
#define SUMD_STATUS_FAILSAFE  	0x81

#define SUMD_MIN_CHANNELS		2
#define SUMD_MAX_FRAME_PERIOD   750 	// ms without a valid frame before the link counts as lost

// Frames come every 10 ms and leave about 4 ms between them even with 32 channels at 115200
// baud, so a pause of this many milliseconds inside a frame means the frame was cut.
#define SUMD_FRAME_GAP_MS		4


/**
 * @brief Parses incoming UART bytes, validates the protocol frame, updates receiver status, and dispatches channel data.
 * @param p_client Pointer to the protocol receiver instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a full frame is parsed or parser state remains active; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive_data)
{
	PifRcSumd *p_owner = (PifRcSumd *)p_client;
	uint8_t data, count;
	uint8_t* p_buffer;
    uint16_t channel[PIF_SUMD_CHANNEL_COUNT];
	uint16_t crc;
	int index;
	BOOL rtn = FALSE;

	p_buffer = p_owner->__p_buffer;

	while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1)) {
		// Timed per byte: the UART keeps calling this while a frame is open, so a time taken per
		// call would never show the pause.
		if (p_owner->__index && pif_cumulative_timer1ms - p_owner->__last_time >= SUMD_FRAME_GAP_MS) {
			p_owner->__index = 0;
		}
		p_owner->__last_time = pif_cumulative_timer1ms;

		p_buffer[p_owner->__index++] = data;

		if (p_owner->__index == 1) {
			if (p_buffer[0] != SUMD_VENDOR_ID) {
				p_owner->__index = 0;
				continue;
			}
		}
		else if (p_owner->__index == 2) {
			if (p_buffer[1] != SUMD_STATUS_VALID && p_buffer[1] != SUMD_STATUS_FAILSAFE) {
				p_owner->parent._error_frames++;
				p_owner->__index = 0;
				continue;
			}
		}
		else if (p_owner->__index == 3) {
			if (p_buffer[2] < SUMD_MIN_CHANNELS || p_buffer[2] > PIF_SUMD_CHANNEL_COUNT) {
				p_owner->parent._error_frames++;
				p_owner->__index = 0;
				continue;
			}
		}
		else if (p_owner->__index >= SUMD_HEADER_SIZE + p_buffer[2] * 2 + SUMD_CRC_SIZE) {
			p_owner->__index = 0;
			count = p_buffer[2];

			// The CRC covers the header and the channel data; run over the CRC bytes as well it ends at 0.
			crc = pifCrc16(p_buffer, SUMD_HEADER_SIZE + 2 * count + SUMD_CRC_SIZE);
			if (crc == 0) {
				// The header is only trusted once the CRC has passed.
				p_owner->parent._failsafe = (p_buffer[1] == SUMD_STATUS_FAILSAFE);
				p_owner->parent._channel_count = count;
				p_owner->parent._good_frames++;

				// Each channel is 16 bits big-endian in 1/8 us steps. Channels the frame does not
				// carry are handed out as 0.
				for (index = 0; index < count; index++) {
					channel[index] = ((p_buffer[SUMD_HEADER_SIZE + 2 * index] << 8) + p_buffer[SUMD_HEADER_SIZE + 2 * index + 1]) / 8;
				}
				for (; index < PIF_SUMD_CHANNEL_COUNT; index++) {
					channel[index] = 0;
				}
				p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

		    	if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
			}
			else {
				p_owner->parent._error_frames++;
			}
			rtn = TRUE;
			break;
		}
	}
	return rtn || p_owner->__index > 0;
}

BOOL pifRcSumd_Init(PifRcSumd* p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcSumd));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;
	p_owner->parent._channel_count = PIF_SUMD_CHANNEL_COUNT;
	p_owner->parent._failsafe = TRUE;
	p_owner->parent._max_frame_period = SUMD_MAX_FRAME_PERIOD;
    return TRUE;
}

void pifRcSumd_AttachUart(PifRcSumd* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, p_owner, _evtParsing, NULL, NULL);
}

void pifRcSumd_DetachUart(PifRcSumd* p_owner)
{
	if (!p_owner->__p_uart) return;
	pifTransport_DetachClient(&p_owner->__p_uart->parent);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSumd_SendFrame(PifRcSumd* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t i, buffer[SUMD_FRAME_SIZE];
	uint8_t p = 0;
	uint16_t crc;

	if (count < SUMD_MIN_CHANNELS || count > PIF_SUMD_CHANNEL_COUNT) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	buffer[p++] = SUMD_VENDOR_ID;
	buffer[p++] = SUMD_STATUS_VALID;
	buffer[p++] = count;
	for (i = 0; i < count; i++) {
		buffer[p++] = p_channel[i] >> 8;
		buffer[p++] = p_channel[i] & 0xFF;
	}
	crc = pifCrc16(buffer, SUMD_HEADER_SIZE + 2 * count);
	buffer[p++] = crc >> 8;
	buffer[p++] = crc & 0xFF;
	
	return pifUart_SendTxData(p_owner->__p_uart, buffer, p) == p;
}
