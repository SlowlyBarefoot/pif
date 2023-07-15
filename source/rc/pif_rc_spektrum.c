#include "rc/pif_rc_spektrum.h"


#define SPEKTRUM_RETRY_TIMEOUT		5	// 5ms


static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifRcSpektrum *p_owner = (PifRcSpektrum *)p_client;
	uint8_t data, id;
	uint8_t* p_buffer;
	int index;

    if (!p_owner->parent.__evt_receive) return;

	if (pif_cumulative_timer1ms - p_owner->__last_time >= SPEKTRUM_RETRY_TIMEOUT) {
		p_owner->__index = 0;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

	p_buffer = p_owner->__p_buffer;

	while ((*act_receive_data)(p_owner->__p_uart, &data)) {
		p_buffer[p_owner->__index++] = data;

		if (p_owner->__index == 2) {
			if (p_buffer[1] != PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2 && 
					p_buffer[1] != PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSM2 && 
					p_buffer[1] != PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX && 
					p_buffer[1] != PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS) {
				p_owner->__index = 0;
				continue;
			}
		}
		else if (p_owner->__index >= SPEKTRUM_FRAME_SIZE) {
			p_owner->parent._good_frames++;

			for (index = 2; index < SPEKTRUM_FRAME_SIZE; index += 2) {
				id = (p_buffer[index] >> p_owner->__id_shift) & p_owner->__id_mask;
				if (id < p_owner->parent._channel_count) {
					p_owner->__channel[id] = 988 + (((uint16_t)(p_buffer[index] & p_owner->__pos_mask) << 8) + p_buffer[index + 1]) / p_owner->_pos_factor;
				}
			}
			p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

			p_owner->__index = 0;

			if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, p_owner->__channel, p_owner->parent.__p_issuer);
		}
	}
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
	switch (protocol_id) {
	case PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2:
		p_owner->parent._channel_count = 7;
		p_owner->parent._max_frame_period = 25;
		p_owner->__id_mask = 0x3F;
		p_owner->__id_shift = 2;
		p_owner->__pos_mask = 0x03;
		p_owner->_pos_factor = 1;
		break;

	case PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSM2:
	case PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX:
		p_owner->parent._channel_count = 8;
		p_owner->parent._max_frame_period = 15;
		p_owner->__id_mask = 0x0F;
		p_owner->__id_shift = 3;
		p_owner->__pos_mask = 0x07;
		p_owner->_pos_factor = 2;
		break;

	case PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS:
		p_owner->parent._channel_count = 8;
		p_owner->parent._max_frame_period = 25;
		p_owner->__id_mask = 0x0F;
		p_owner->__id_shift = 3;
		p_owner->__pos_mask = 0x07;
		p_owner->_pos_factor = 2;
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
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifRcSpektrum_DetachUart(PifRcSpektrum* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSpektrum_SendFrame(PifRcSpektrum* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t i, buffer[SPEKTRUM_FRAME_SIZE];
	uint8_t p = 0;
	static int last_ch = 6;

	buffer[p++] = 0;
	buffer[p++] = p_owner->_protocol_id;
	for (i = 0; i < PIF_SPEKTRUM_CHANNEL_COUNT - 2 && i < count; i++) {
		buffer[p++] = (i << p_owner->__id_shift) | ((p_channel[i] >> 8) & p_owner->__pos_mask);
		buffer[p++] = p_channel[i] & 0xFF;
	}
	if (count >= 7) {
		buffer[p++] = (last_ch << p_owner->__id_shift) | ((p_channel[last_ch] >> 8) & p_owner->__pos_mask);
		buffer[p++] = p_channel[last_ch] & 0xFF;
		if (p_owner->parent._channel_count >= 8 && count >= 8) last_ch ^= 1;
	}
	
	return pifUart_SendTxData(p_owner->__p_uart, buffer, p);
}
