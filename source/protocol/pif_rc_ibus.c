#include "core/pif_log.h"
#include "protocol/pif_rc_ibus.h"


#define IBUS_OVERHEAD 			3		// packet is <len><cmd><data....><chkl><chkh>, overhead=cmd+chk bytes

#define IBUS_COMMAND_SERVO		0x40	// Command to set servo or motor speed is always 0x40
#define IBUS_COMMAND_DISCOVER 	0x80 	// Command discover sensor (lowest 4 bits are sensor)
#define IBUS_COMMAND_TYPE 		0x90    // Command sensor type (lowest 4 bits are sensor)
#define IBUS_COMMAND_VALUE 		0xA0	// Command send sensor data (lowest 4 bits are sensor)

#define IBUS_RETRY_TIMEOUT		3		// 3ms, Packets are received very ~7ms so use ~half that for the gap


static void _ParsingPacket(PifRcIbus *p_owner, PifActCommReceiveData act_receive_data)
{
	uint8_t data;
	static uint8_t ptr;                      // pointer in buffer
	static uint16_t chksum;                  // checksum calculation
	static uint8_t lchksum;                  // checksum lower byte received

	while ((*act_receive_data)(p_owner->__p_comm, &data)) {
		switch (p_owner->__rx_state) {
		case IRS_GET_LENGTH:
			if (data <= IBUS_FRAME_SIZE && data > IBUS_OVERHEAD) {
				ptr = 0;
				p_owner->__rx_length = data - IBUS_OVERHEAD;
				chksum = 0xFFFF - data;
				p_owner->__rx_state = IRS_GET_DATA;
			}
			break;

		case IRS_GET_DATA:
			p_owner->__rx_buffer[ptr++] = data;
			chksum -= data;
			if (ptr == p_owner->__rx_length) {
				p_owner->__rx_state = IRS_GET_CHKSUML;
			}
			break;

		case IRS_GET_CHKSUML:
			lchksum = data;
			p_owner->__rx_state = IRS_GET_CHKSUMH;
			break;

		case IRS_GET_CHKSUMH:
			// Validate checksum
			if (chksum == ((uint16_t)data << 8) + lchksum) {
				p_owner->parent.good_frames++;
				p_owner->__rx_state = IRS_DONE;
			}
			else {
				p_owner->parent.error_frames++;
				p_owner->__rx_state = IRS_GET_LENGTH;
			}
			break;

		default:
			break;
		}
	}
}

static void _evtParsing(void *p_client, PifActCommReceiveData act_receive_data)
{
	PifRcIbus *p_owner = (PifRcIbus *)p_client;
	PifRcIbusSensorinfo* p_sensor;
    int i;
	uint16_t p = 0;
	uint8_t tx_buffer[8];					// tx message buffer
	uint16_t channel[PIF_IBUS_CHANNEL_COUNT]; 	// servo data received
	uint16_t chksum;

    if (!p_owner->evt_receive) return;

	if (pif_cumulative_timer1ms - p_owner->__last_time >= IBUS_RETRY_TIMEOUT) {
		p_owner->__rx_state = IRS_GET_LENGTH;
	}
	p_owner->__last_time = pif_cumulative_timer1ms;

    if (p_owner->__rx_state < IRS_DONE) {
    	_ParsingPacket(p_owner, act_receive_data);
    }

    if (p_owner->__rx_state == IRS_DONE) {
		p_owner->parent.last_frame_time = pif_cumulative_timer1ms;

		// Checksum is all fine Execute command - 
		uint8_t adr = p_owner->__rx_buffer[0] & 0x0f;
		if (p_owner->__rx_buffer[0] == IBUS_COMMAND_SERVO) {
			// Valid servo command received - extract channel data
			for (i = 1; i < PIF_IBUS_CHANNEL_COUNT * 2 + 1; i += 2) {
				channel[i / 2] = p_owner->__rx_buffer[i] | (p_owner->__rx_buffer[i + 1] << 8);
			}

	    	if (p_owner->evt_receive) (*p_owner->evt_receive)(&p_owner->parent, channel);
		} 
		else if (p_owner->__p_comm->_p_tx_buffer && adr <= p_owner->_number_sensors && adr > 0 && p_owner->__rx_length == 1) {
			// all sensor data commands go here
			// we only process the length==1 commands (=message length is 4 bytes incl overhead) to prevent the case the
			// return messages from the UART TX port loop back to the RX port and are processed again. This is extra
			// precaution as it will also be prevented by the IBUS_TIMEGAP required
			p_sensor = &p_owner->__sensors[adr - 1];
			switch (p_owner->__rx_buffer[0] & 0x0f0) {
			case IBUS_COMMAND_DISCOVER:
				// echo discover command: 0x04, 0x81, 0x7A, 0xFF 
				tx_buffer[p++] = 0x04;
				tx_buffer[p++] = IBUS_COMMAND_DISCOVER + adr;
				break;

			case IBUS_COMMAND_TYPE:
				// echo sensor type command: 0x06 0x91 0x00 0x02 0x66 0xFF 
				tx_buffer[p++] = 0x06;
				tx_buffer[p++] = IBUS_COMMAND_TYPE + adr;
				tx_buffer[p++] = p_sensor->type;
				tx_buffer[p++] = p_sensor->length;
				break;

			case IBUS_COMMAND_VALUE:
				// echo sensor value command: 0x06 0x91 0x00 0x02 0x66 0xFF 
				tx_buffer[p++] = 0x04 + p_sensor->length;
				tx_buffer[p++] = IBUS_COMMAND_VALUE + adr;
				tx_buffer[p++] = p_sensor->value & 0x0ff;
				tx_buffer[p++] = (p_sensor->value >> 8) & 0x0ff; 
				if (p_sensor->length == 4) {
					tx_buffer[p++] = (p_sensor->value >> 16) & 0x0ff; 
					tx_buffer[p++] = (p_sensor->value >> 24) & 0x0ff; 
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

				pifRingBuffer_PutData(p_owner->__p_comm->_p_tx_buffer, tx_buffer, p);
			}
		}

    	p_owner->__rx_state = IRS_GET_LENGTH;
    }
}

BOOL pifRcIbus_Init(PifRcIbus* p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRcIbus));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent.id = id;
	p_owner->parent.channel_count = PIF_IBUS_CHANNEL_COUNT;
	p_owner->parent.failsafe = FALSE;
    return TRUE;
}

void pifRcIbus_AttachComm(PifRcIbus* p_owner, PifComm *p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner, _evtParsing, NULL);
}

void pifRcIbus_DetachComm(PifRcIbus* p_owner)
{
	pifComm_DetachClient(p_owner->__p_comm);
	p_owner->__p_comm = NULL;
}

BOOL pifRcIbus_AddSensor(PifRcIbus* p_owner, uint8_t type, uint8_t len) 
{
	PifRcIbusSensorinfo* p_sensor;

	if (len == 2 || len == 4) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	else if (p_owner->_number_sensors >= PIF_IBUS_SENSOR_MAX) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	p_sensor = &p_owner->__sensors[p_owner->_number_sensors];
	p_sensor->type = type;
	p_sensor->length = len;
	p_sensor->value = 0;
	p_owner->_number_sensors++;
	return TRUE;
}

void pifRcIbus_SetSensorMeasurement(PifRcIbus* p_owner, uint8_t adr, int32_t value) 
{
	if (adr <= p_owner->_number_sensors && adr > 0)
		p_owner->__sensors[adr - 1].value = value;
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

	return pifComm_SendTxData(p_owner->__p_comm, buffer, p);
}
