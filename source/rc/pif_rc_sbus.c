#include "rc/pif_rc_sbus.h"


#define SBUS_STARTBYTE         	0x0F
#define SBUS_ENDBYTE           	0x00

#define SBUS_RETRY_TIMEOUT		3		// 3ms


static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifRcSbus *p_owner = (PifRcSbus *)p_client;
	uint8_t i, data;
	uint8_t* p_buffer;
	uint16_t channels[PIF_SBUS_CHANNEL_COUNT]; 	// servo data received
	BOOL rtn = FALSE;

    if (!p_owner->parent.__evt_receive) return rtn;

	if (pif_cumulative_timer1ms - p_owner->__last_time >= SBUS_RETRY_TIMEOUT) {
		p_owner->__index = 0;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

	p_buffer = p_owner->__buffer;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		if (p_owner->__index == 0 && data != SBUS_STARTBYTE) {
			continue;
		}

		p_buffer[p_owner->__index++] = data;

	    if (p_owner->__index == SBUS_FRAME_SIZE) {
			p_owner->__index = 0;
			if (p_buffer[24] != SBUS_ENDBYTE) {
				//incorrect end byte, out of sync
				p_owner->parent._error_frames++;
				continue;
			}

			p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

			channels[0]  = (p_buffer[1]       | p_buffer[2] << 8)                       & 0x07FF;
			channels[1]  = (p_buffer[2] >> 3  | p_buffer[3] << 5)                 	    & 0x07FF;
			channels[2]  = (p_buffer[3] >> 6  | p_buffer[4] << 2  | p_buffer[5] << 10)  & 0x07FF;
			channels[3]  = (p_buffer[5] >> 1  | p_buffer[6] << 7)                 	    & 0x07FF;
			channels[4]  = (p_buffer[6] >> 4  | p_buffer[7] << 4)                 	    & 0x07FF;
			channels[5]  = (p_buffer[7] >> 7  | p_buffer[8] << 1  | p_buffer[9] << 9)   & 0x07FF;
			channels[6]  = (p_buffer[9] >> 2  | p_buffer[10] << 6)                	    & 0x07FF;
			channels[7]  = (p_buffer[10] >> 5 | p_buffer[11] << 3)                	    & 0x07FF;
			channels[8]  = (p_buffer[12]      | p_buffer[13] << 8)                	    & 0x07FF;
			channels[9]  = (p_buffer[13] >> 3 | p_buffer[14] << 5)                	    & 0x07FF;
			channels[10] = (p_buffer[14] >> 6 | p_buffer[15] << 2 | p_buffer[16] << 10) & 0x07FF;
			channels[11] = (p_buffer[16] >> 1 | p_buffer[17] << 7)                	    & 0x07FF;
			channels[12] = (p_buffer[17] >> 4 | p_buffer[18] << 4)                	    & 0x07FF;
			channels[13] = (p_buffer[18] >> 7 | p_buffer[19] << 1 | p_buffer[20] << 9)  & 0x07FF;
			channels[14] = (p_buffer[20] >> 2 | p_buffer[21] << 6)                	    & 0x07FF;
			channels[15] = (p_buffer[21] >> 5 | p_buffer[22] << 3)                	    & 0x07FF;

			channels[16] = ((p_buffer[23])      & 0x0001) ? 2047 : 0;
			channels[17] = ((p_buffer[23] >> 1) & 0x0001) ? 2047 : 0;

			if ((p_buffer[23] >> 3) & 0x0001) {
				p_owner->parent._failsafe = TRUE;
			} else {
				p_owner->parent._failsafe = FALSE;
			}

			if ((p_buffer[23] >> 2) & 0x0001) {
				p_owner->parent._lost_frames++;
			}
			else {
				p_owner->parent._good_frames++;
				for (i = 0; i < PIF_SBUS_CHANNEL_COUNT; i++) {
					channels[i] = 0.625f * channels[i] + 880;
				}
		    	if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channels, p_owner->parent.__p_issuer);
			}
	    	rtn = TRUE;
			break;
		}
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
    return TRUE;
}

void pifRcSbus_AttachUart(PifRcSbus* p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, NULL);
}

void pifRcSbus_DetachUart(PifRcSbus* p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifRcSbus_SendFrame(PifRcSbus* p_owner, uint16_t* p_channel, uint8_t count)
{
	uint8_t buffer[25];
	int i;

	for (i = 10; i < 23; i++) buffer[i] = 0;

	buffer[0] = SBUS_STARTBYTE;
	buffer[1] = p_channel[0] & 0x07FF;
	buffer[2] = ((p_channel[0] & 0x07FF) >> 8) | ((p_channel[1] & 0x07FF) << 3);
	buffer[3] = ((p_channel[1] & 0x07FF) >> 5) | ((p_channel[2] & 0x07FF) << 6);
	buffer[4] = (p_channel[2] & 0x07FF) >> 2;
	buffer[5] = ((p_channel[2] & 0x07FF) >> 10) | ((p_channel[3] & 0x07FF) << 1);
	buffer[6] = ((p_channel[3] & 0x07FF) >> 7) | ((p_channel[4] & 0x07FF) << 4);
	buffer[7] = ((p_channel[4] & 0x07FF) >> 4) | ((p_channel[5] & 0x07FF) << 7);
	buffer[8] = (p_channel[5] & 0x07FF) >> 1;
	buffer[9] = (p_channel[5] & 0x07FF) >> 9;
	if (count <= 6) goto next;
	buffer[9] |= (p_channel[6] & 0x07FF) << 2;
	buffer[10] = (p_channel[6] & 0x07FF) >> 6;
	if (count <= 7) goto next;
	buffer[10] |= (p_channel[7] & 0x07FF) << 5;
	buffer[11] = (p_channel[7] & 0x07FF) >> 3;
	if (count <= 8) goto next;
	buffer[12] = p_channel[8] & 0x07FF;
	buffer[13] = (p_channel[8] & 0x07FF) >> 8;
	if (count <= 9) goto next;
	buffer[13] |= (p_channel[9]  & 0x07FF) << 3;
	buffer[14] = (p_channel[9] & 0x07FF) >> 5;
	if (count <= 10) goto next;
	buffer[14] |= (p_channel[10] & 0x07FF) << 6;
	buffer[15] = (p_channel[10] & 0x07FF) >> 2;
	buffer[16] = (p_channel[10] & 0x07FF) >> 10;
	if (count <= 11) goto next;
	buffer[16] |= (p_channel[11] & 0x07FF) << 1;
	buffer[17] = (p_channel[11] & 0x07FF) >> 7;
	if (count <= 12) goto next;
	buffer[17] |= (p_channel[12] & 0x07FF) << 4;
	buffer[18] = (p_channel[12] & 0x07FF) >> 4;
	if (count <= 13) goto next;
	buffer[18] |= (p_channel[13] & 0x07FF) << 7;
	buffer[19] = (p_channel[13] & 0x07FF) >> 1;
	buffer[20] = (p_channel[13] & 0x07FF) >> 9;
	if (count <= 14) goto next;
	buffer[20] |= (p_channel[14] & 0x07FF) << 2;
	buffer[21] = (p_channel[14] & 0x07FF) >> 6;
	if (count <= 15) goto next;
	buffer[21] |= (p_channel[15] & 0x07FF) << 5;
	buffer[22] = (p_channel[15] & 0x07FF) >> 3;
next:
	buffer[23] = 0x00;
	buffer[24] = SBUS_ENDBYTE;

	return pifUart_SendTxData(p_owner->__p_uart, buffer, 25);
}
