#include "core/pif_log.h"
#include "rc/pif_rc_ibus.h"


#define IBUS_RETRY_TIMEOUT		3		// 3ms, Packets are received very ~7ms so use ~half that for the gap


/**
 * @brief Hands a completed, checksum-verified frame to the client.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @return IBUS_FRAME_SERVO after the channels went to evt_receive, IBUS_FRAME_TELEMETRY when the
 *         frame is a sensor request for us (its command and address are in _tlm_command and
 *         _tlm_address), otherwise IBUS_FRAME_NONE.
 */
static PifRcIbusFrame _processFrame(PifRcIbus *p_owner)
{
	uint16_t channel[PIF_IBUS_EXP_CHANNEL_COUNT]; 	// servo data received
	uint8_t command, adr;
	int i, c, offset;

	p_owner->parent._last_frame_time = pif_cumulative_timer1ms;

	command = p_owner->__rx_buffer[1] & 0xf0;
	adr = p_owner->__rx_buffer[1] & 0x0f;
	if (p_owner->_model == IBUS_MODEL_IA6 || command == IBUS_COMMAND_SERVO) {
		// Valid servo command received - extract channel data
		offset = p_owner->_model == IBUS_MODEL_IA6B ? 2 : 1;
		for (c = 0, i = offset; c < PIF_IBUS_CHANNEL_COUNT; c++, i += 2) {
			channel[c] = p_owner->__rx_buffer[i] | (p_owner->__rx_buffer[i + 1] << 8);
		}
		for (c = PIF_IBUS_CHANNEL_COUNT, i = offset + 1; c < PIF_IBUS_EXP_CHANNEL_COUNT; c++, i += 6) {
			channel[c] = ((p_owner->__rx_buffer[i] & 0xF0) >> 4) | (p_owner->__rx_buffer[i + 2] & 0xF0) | ((p_owner->__rx_buffer[i + 4] & 0xF0) << 4);
		}

		if (p_owner->parent.__evt_receive) (*p_owner->parent.__evt_receive)(&p_owner->parent, channel, p_owner->parent.__p_issuer);
		return IBUS_FRAME_SERVO;
	}

	// We only take the length==IBUS_TELEMETRY_SIZE commands (=message length is 4 bytes incl
	// overhead) to prevent the case the return messages from the UART TX port loop back to the RX
	// port and are processed again. This is extra precaution as it will also be prevented by the
	// IBUS_TIMEGAP required
	if (adr > 0 && p_owner->_length == IBUS_TELEMETRY_SIZE) {
		p_owner->_tlm_command = command;
		p_owner->_tlm_address = adr;
		return IBUS_FRAME_TELEMETRY;
	}
	return IBUS_FRAME_NONE;
}

/**
 * @brief Advances the packet receive state machine by one byte.
 * @param p_owner Pointer to the receiver instance to operate on.
 * @param data Received byte.
 * @return What the byte completed; see _processFrame().
 */
static PifRcIbusFrame _parsingPacket(PifRcIbus *p_owner, uint8_t data)
{
	int i;

	// A frame is sent in one go, so a gap this long in the middle of one means it was cut short.
	if (p_owner->__rx_state != IRS_GET_LENGTH && pif_cumulative_timer1ms - p_owner->__last_time >= IBUS_RETRY_TIMEOUT) {
		p_owner->__rx_state = IRS_GET_LENGTH;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

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
			p_owner->__ptr = 1;
			p_owner->__chksum = data;
		}
		break;

	case IRS_GET_COMMAND:
		p_owner->__rx_buffer[p_owner->__ptr++] = data;
		p_owner->__chksum += data;
		p_owner->__rx_state = IRS_GET_DATA;
		break;

	case IRS_GET_DATA:
		p_owner->__rx_buffer[p_owner->__ptr++] = data;
		p_owner->__chksum += data;
		if (p_owner->__ptr == p_owner->_length - 2) {
			p_owner->__rx_state = IRS_GET_CHKSUML;
		}
		break;

	case IRS_GET_CHKSUML:
		p_owner->__lchksum = data;
		p_owner->__rx_state = IRS_GET_CHKSUMH;
		break;

	case IRS_GET_CHKSUMH:
		p_owner->__rx_state = IRS_GET_LENGTH;
		// Validate checksum
		if (p_owner->_model == IBUS_MODEL_IA6B) {
			p_owner->__chksum = 0xFFFF - p_owner->__chksum;
		}
		else {
			// IA6 sums the channel words after the 0x55 header, not the bytes.
			p_owner->__chksum = 0;
			for (i = 1; i < p_owner->__ptr; i += 2) {
				p_owner->__chksum += p_owner->__rx_buffer[i] | (p_owner->__rx_buffer[i + 1] << 8);
			}
		}
		if (p_owner->__chksum == ((uint16_t)data << 8) + p_owner->__lchksum) {
			p_owner->parent._good_frames++;
			return _processFrame(p_owner);
		}
		p_owner->parent._error_frames++;
		break;

	default:
		break;
	}
	return IBUS_FRAME_NONE;
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
	return rtn || p_owner->__rx_state > IRS_GET_LENGTH;
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
	uint8_t tx_buffer[IBUS_FRAME_SIZE + 1];		// tx message buffer
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
		// echo discover command: 0x04, 0x81, 0x7A, 0xFF
		tx_buffer[p++] = 0x04;
		tx_buffer[p++] = IBUS_COMMAND_DISCOVER + address;
		break;

	case IBUS_COMMAND_TYPE:
		if (!(*p_owner->evt_telemetry)(p_owner, command, address, &sensor)) return FALSE;
		// echo sensor type command: 0x06 0x91 0x00 0x02 0x66 0xFF
		tx_buffer[p++] = 0x06;
		tx_buffer[p++] = IBUS_COMMAND_TYPE + address;
		tx_buffer[p++] = sensor.type;
		tx_buffer[p++] = sensor.length;
		break;

	case IBUS_COMMAND_VALUE:
		if (!(*p_owner->evt_telemetry)(p_owner, command, address, &sensor)) return FALSE;
		if (sensor.length > sizeof(sensor.value)) return FALSE;
		// echo sensor value command: 0x06 0x91 0x00 0x02 0x66 0xFF
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
