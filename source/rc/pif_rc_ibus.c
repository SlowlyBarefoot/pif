#include "core/pif_log.h"
#include "rc/pif_rc_ibus.h"


#define IBUS_RETRY_TIMEOUT		3		// 3ms, Packets are received very ~7ms so use ~half that for the gap


/**
 * @brief Consumes UART bytes for iBUS and advances the packet receive state machine until a frame is completed.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return None.
 */
static void _ParsingPacket(PifRcIbus *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data;
	static uint8_t ptr;                      // pointer in buffer
	static uint16_t chksum;                  // checksum calculation
	static uint8_t lchksum;                  // checksum lower byte received

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx_state) {
		case IRS_GET_LENGTH:
			if (data == IBUS_FRAME_SIZE || data == IBUS_TELEMETRY_SIZE) {
				p_owner->_model = IBUS_MODEL_IA6B;
				p_owner->_length = data;
				p_owner->__rx_state = IRS_GET_COMMAND;
			}
			else if (data == 0x55) {
				p_owner->_model = IBUS_MODEL_IA6;
				p_owner->_length = 31;
				p_owner->__rx_state = IRS_GET_DATA;
			}
			if (p_owner->__rx_state != IRS_GET_LENGTH) {
				p_owner->__rx_buffer[0] = data;
				ptr = 1;
				chksum = data;
			}
			break;

		case IRS_GET_COMMAND:
			p_owner->__rx_buffer[ptr++] = data;
			chksum += data;
			p_owner->__rx_state = IRS_GET_DATA;
			break;

		case IRS_GET_DATA:
			p_owner->__rx_buffer[ptr++] = data;
			chksum += data;
			if (ptr == p_owner->_length - 2) {
				p_owner->__rx_state = IRS_GET_CHKSUML;
			}
			break;

		case IRS_GET_CHKSUML:
			lchksum = data;
			p_owner->__rx_state = IRS_GET_CHKSUMH;
			break;

		case IRS_GET_CHKSUMH:
			// Validate checksum
			if (p_owner->_model == IBUS_MODEL_IA6B) {
				chksum = 0xFFFF - chksum;
			}
			if (chksum == ((uint16_t)data << 8) + lchksum) {
				p_owner->parent._good_frames++;
				p_owner->__rx_state = IRS_DONE;
			}
			else {
				p_owner->parent._error_frames++;
				p_owner->__rx_state = IRS_GET_LENGTH;
			}
			return;

		default:
			break;
		}
	}
}

/**
 * @brief Parses incoming UART bytes, validates the protocol frame, updates receiver status, and dispatches channel data.
 * @param p_client Pointer to the protocol receiver instance registered as UART client.
 * @param act_receive_data UART receive function used to pull available bytes.
 * @return TRUE when a full frame is parsed or parser state remains active; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifRcIbus *p_owner = (PifRcIbus *)p_client;
	PifRcIbusSensorinfo sensor;
    int i, c, offset;
	uint16_t p = 0;
	uint8_t command, adr;
	uint8_t tx_buffer[33];							// tx message buffer
	uint16_t channel[PIF_IBUS_EXP_CHANNEL_COUNT]; 	// servo data received
	uint16_t chksum;
	BOOL rtn = FALSE;

    if (!p_owner->parent.__evt_receive) return rtn;

	if (pif_cumulative_timer1ms - p_owner->__last_time >= IBUS_RETRY_TIMEOUT) {
		p_owner->__rx_state = IRS_GET_LENGTH;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

    if (p_owner->__rx_state < IRS_DONE) {
    	_ParsingPacket(p_owner, act_receive_data);
    }

    rtn = p_owner->__rx_state > IRS_GET_LENGTH;
    if (p_owner->__rx_state == IRS_DONE) {
		p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

		// Checksum is all fine Execute command - 
		command = p_owner->__rx_buffer[1] & 0xf0;
		adr = p_owner->__rx_buffer[1] & 0x0f;
		if (command == IBUS_COMMAND_SERVO) {
			// Valid servo command received - extract channel data
			offset = p_owner->_model == IBUS_MODEL_IA6B ? 2 : 1;
			for (c = 0, i = offset; c < PIF_IBUS_CHANNEL_COUNT; c++, i += 2) {
				channel[c] = p_owner->__rx_buffer[i] | (p_owner->__rx_buffer[i + 1] << 8);
			}
			for (c = PIF_IBUS_CHANNEL_COUNT, i = offset + 1; c < PIF_IBUS_EXP_CHANNEL_COUNT; c++, i += 6) {
				channel[c] = ((p_owner->__rx_buffer[i] & 0xF0) >> 4) | (p_owner->__rx_buffer[i + 2] & 0xF0) | ((p_owner->__rx_buffer[i + 4] & 0xF0) << 4);
			}

	    	if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
		} 
		else if (p_owner->__p_uart->_p_tx_buffer && adr <= p_owner->_number_sensors && adr > 0 && p_owner->_length == 1) {
			// all sensor data commands go here
			// we only process the length==1 commands (=message length is 4 bytes incl overhead) to prevent the case the
			// return messages from the UART TX port loop back to the RX port and are processed again. This is extra
			// precaution as it will also be prevented by the IBUS_TIMEGAP required
			switch (command) {
			case IBUS_COMMAND_DISCOVER:
				// echo discover command: 0x04, 0x81, 0x7A, 0xFF 
				tx_buffer[p++] = 0x04;
				tx_buffer[p++] = IBUS_COMMAND_DISCOVER + adr;
				break;

			case IBUS_COMMAND_TYPE:
		    	if (p_owner->evt_telemetry) (*p_owner->evt_telemetry)(p_owner, command, adr, &sensor);
				// echo sensor type command: 0x06 0x91 0x00 0x02 0x66 0xFF 
				tx_buffer[p++] = 0x06;
				tx_buffer[p++] = IBUS_COMMAND_TYPE + adr;
				tx_buffer[p++] = sensor.type;
				tx_buffer[p++] = sensor.length;
				break;

			case IBUS_COMMAND_VALUE:
		    	if (p_owner->evt_telemetry) (*p_owner->evt_telemetry)(p_owner, command, adr, &sensor);
				// echo sensor value command: 0x06 0x91 0x00 0x02 0x66 0xFF 
				tx_buffer[p++] = 0x04 + sensor.length;
				tx_buffer[p++] = IBUS_COMMAND_VALUE + adr;
				for (i = 0; i < sensor.length; i++) {
					tx_buffer[p++] = sensor.value[i];
				}
				break;

			default:
				adr = 0; // unknown command, prevent sending chksum
				break;
			}
			if (adr > 0) {
				chksum = 0xFFFF - pifCheckSum(tx_buffer, p);
				tx_buffer[p++] = chksum & 0x0ff;
				tx_buffer[p++] = chksum >> 8;

				pifRingBuffer_PutData(p_owner->__p_uart->_p_tx_buffer, tx_buffer, p);
			}
		}

    	p_owner->__rx_state = IRS_GET_LENGTH;
    }
    return rtn;
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
