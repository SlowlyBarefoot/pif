#include "rc/pif_rc_sumd.h"


#define SUMD_VENDOR_ID         	0xA8
#define SUMD_STATUS_VALID      	0x01
#define SUMD_STATUS_FAILSAFE  	0x81

#define SUMD_MAX_FRAME_PERIOD   750 	// 750ms, above this delay, switch to failsafe
#define SUMD_RETRY_TIMEOUT		4		// 4ms


static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifRcSumd *p_owner = (PifRcSumd *)p_client;
	uint8_t data;
	uint8_t* p_buffer;
    uint16_t channel[PIF_SUMD_CHANNEL_COUNT];
	uint16_t crc;
	int index;

    if (!p_owner->parent.__evt_receive) return;

	if (pif_cumulative_timer1ms - p_owner->__last_time >= SUMD_RETRY_TIMEOUT) {
		p_owner->__index = 0;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

	p_buffer = p_owner->__p_buffer;

	while ((*act_receive_data)(p_owner->__p_uart, &data)) {
		//add byte to the ring buffer
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
			else {
				p_owner->parent._failsafe = (p_buffer[1] == SUMD_STATUS_FAILSAFE);
			}
		}
		else if (p_owner->__index == 3) {
			if (p_buffer[2] < 2 || p_buffer[2] > PIF_SUMD_CHANNEL_COUNT) {		// 2 < channels < PIF_SUMD_CHANNEL_COUNT
				p_owner->parent._error_frames++;
				p_owner->__index = 0;
				continue;
			}
			else {
				p_owner->parent._channel_count = p_buffer[2];
			}
		}
		else if (p_owner->__index >= SUMD_HEADER_SIZE + p_buffer[2] * 2 + SUMD_CRC_SIZE) {
			//compute CRC with header and data
			crc = pifCrc16(p_buffer, SUMD_HEADER_SIZE + 2 * p_owner->parent._channel_count + SUMD_CRC_SIZE);
			//if frame is valid
			if (crc == 0) {
				p_owner->parent._good_frames++;

				//update channel output values
				for (index = 0; index < p_owner->parent._channel_count; index++) {
					channel[index] = ((p_buffer[SUMD_HEADER_SIZE + 2 * index] << 8) + p_buffer[SUMD_HEADER_SIZE + 2 * index + 1]) / 8;
				}
				p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

				//forgot decoded bytes from the ring buffer
				p_owner->__index = 0;

		    	if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
			}
			else {
				p_owner->parent._error_frames++;
				p_owner->__index = 0;
			}
		}
	}
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
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifRcSumd_DetachUart(PifRcSumd* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSumd_SendFrame(PifRcSumd* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t i, buffer[SUMD_FRAME_SIZE];
	uint8_t p = 0;
	uint16_t crc;

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
	
	return pifUart_SendTxData(p_owner->__p_uart, buffer, p);
}
