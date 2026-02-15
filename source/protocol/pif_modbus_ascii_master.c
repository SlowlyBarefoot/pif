#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_task_manager.h"
#include "protocol/pif_modbus_ascii_master.h"


#ifndef PIF_NO_LOG

#define PKT_ERR_INVALID_PACKET_NO	0
#define PKT_ERR_WRONG_CRC    		1

static const char *c_cPktErr[] = {
		"Invalid Packet No",
		"Wrong CRC"
};

#endif

/**
 * @brief Parses an incoming protocol packet and updates parser state and outputs.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return None.
 */
static void _parsingPacket(PifModbusAsciiMaster *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data, lrc[2];
	uint16_t i;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx_state) {
		case MBRS_IDLE:
			if (data == ':') {
				p_owner->__rx_state = MBRS_DATA;
				p_owner->index = 0;
			}
			break;

		case MBRS_DATA:
			if (data == ASCII_CR) {
			}
			else if (data == ASCII_LF) {
				lrc[0] = pifModbusAscii_CalcLrc(p_owner->__buffer, p_owner->index - 2);
				lrc[1] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[p_owner->index - 2]);
				if (lrc[0] == lrc[1]) {
					for (i = 0; i < p_owner->index - 2; i += 2) {
						p_owner->__buffer[i / 2] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[i]);
					}
					if (p_owner->__buffer[1] & 0x80) {
						p_owner->_error = p_owner->__buffer[2];
						goto fail;
					}
					else {
						switch (p_owner->__buffer[1]) {
						case MBF_READ_COILS:
						case MBF_READ_DISCRETE_INPUTS:
						case MBF_READ_HOLDING_REGISTERS:
						case MBF_READ_INPUT_REGISTERS:
						case MBF_READ_WRITE_MULTIPLE_REGISTERS:
						case MBF_WRITE_SINGLE_COIL:
						case MBF_WRITE_SINGLE_REGISTER:
						case MBF_WRITE_MULTIPLE_COILS:
						case MBF_WRITE_MULTIPLE_REGISTERS:
							p_owner->__rx_state = MBRS_FINISH;
							break;

						default:
							p_owner->_error = MBE_INVALID_RESPONSE;
#ifndef PIF_NO_LOG
							pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) A:%u F:%u", p_owner->_id,
									c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)p_owner->__buffer[0],
									(unsigned int)p_owner->__buffer[1]);
#endif
							goto fail;
						}
					}
				}
				else {
					p_owner->_error = MBE_CRC;
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) %X!=%X", p_owner->_id,
							c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)lrc[0], (unsigned int)lrc[1]);
#endif
					goto fail;
				}
			}
			else {
				p_owner->__buffer[p_owner->index] = data;
				p_owner->index++;
			}
			break;

		default:
			break;
		}
	}
	return;

fail:
	p_owner->__rx_state = MBRS_ERROR;
}

/**
 * @brief Driver callback that consumes received bytes and dispatches parsed events.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return TRUE when the callback handled data; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusAsciiMaster *p_owner = (PifModbusAsciiMaster *)p_client;

	_parsingPacket(p_owner, act_receive_data);

	switch (p_owner->__rx_state) {
	case MBRS_FINISH:
		if (p_owner->__tx_address != p_owner->__buffer[0]) {
			p_owner->_error = MBE_INVALID_UNIT_ID;
		}
		break;

	case MBRS_ERROR:
		break;

	default:
		return FALSE;
	}

	pifTimer_Stop(p_owner->__p_timer);
	p_owner->__rx_state = MBRS_IDLE;
	p_owner->__state = MBMS_FINISH;
	return TRUE;
}

/**
 * @brief Driver callback that emits pending transmit bytes from the protocol state machine.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_send_data Callback used to push outgoing bytes to the underlying driver.
 * @return Number of bytes transmitted during this callback invocation.
 */
static uint16_t _evtSending(void *p_client, PifActUartSendData act_send_data)
{
	PifModbusAsciiMaster *p_owner = (PifModbusAsciiMaster *)p_client;
	uint16_t length, period = 0;

	switch (p_owner->__state) {
	case MBMS_REQUEST:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__buffer + p_owner->index,
    			p_owner->length - p_owner->index);
		p_owner->index += length;
		if (p_owner->index >= p_owner->length) {
			p_owner->__state = p_owner->__p_uart->__act_direction ? MBMS_REQUEST_WAIT : MBMS_RESPONSE;
		}
		period = 1;
		break;

	case MBMS_REQUEST_WAIT:
		period = 1;
		if (pifUart_CheckTxTransfer(p_owner->__p_uart)) {
			p_owner->__state = MBMS_REQUEST_DELAY;
			period *= 2;
		}
		break;

	case MBMS_REQUEST_DELAY:
		(*p_owner->__p_uart->__act_direction)(UD_RX);
		p_owner->__state = MBMS_RESPONSE;
		break;

	default:
		break;
	}
	return period;
}

/**
 * @brief Handles timer-expiration events and transitions the protocol state machine.
 * @param p_issuer Issuer pointer provided by the timer callback context.
 * @return None.
 */
static void _evtTimerTimeout(PifIssuerP p_issuer)
{
	PifModbusAsciiMaster *p_owner = (PifModbusAsciiMaster *)p_issuer;

	p_owner->__state = MBMS_ERROR;
	p_owner->_error = MBE_TIMEOUT;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_WARN, "MB(%u) TxTimeout", p_owner->_id);
#endif
}

/**
 * @brief Builds and sends a request frame, then waits for and validates the response.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param len Input argument used by this operation.
 * @return TRUE if the operation succeeds; otherwise FALSE.
 */
static BOOL _requestAndResponse(PifModbusAsciiMaster *p_owner, uint16_t len)
{
	p_owner->_error = MBE_NONE;

	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);

	p_owner->__buffer[0] = ':';
	pifModbusAscii_CharToAscii(pifModbusAscii_CalcLrc(&p_owner->__buffer[1], len - 1), &p_owner->__buffer[len]);
	len += 2;
	p_owner->__buffer[len] = ASCII_CR; len++;
	p_owner->__buffer[len] = ASCII_LF; len++;

	if (!pifTimer_Start(p_owner->__p_timer, p_owner->__timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "MB(%u) Not start timer", p_owner->_id);
#endif
	}

	p_owner->length = len;
	p_owner->index = 0;
	p_owner->__state = MBMS_REQUEST;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);

	while (1) {
		if (p_owner->__state == MBMS_RESPONSE) break;
		else if (p_owner->__state == MBMS_ERROR) goto fail;
		pifTaskManager_YieldUs(p_owner->__p_uart->_transfer_time);
	}

	while (1) {
		if (p_owner->__state == MBMS_FINISH)  break;
		else if (p_owner->__state == MBMS_ERROR) goto fail;
		pifTaskManager_YieldUs(p_owner->__p_uart->_transfer_time);
	}

fail:
	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);
	p_owner->__state = MBMS_IDLE;
	return p_owner->_error == MBE_NONE;
}

BOOL pifModbusAsciiMaster_Init(PifModbusAsciiMaster *p_owner, PifId id, PifTimerManager *p_timer_manager)
{
    if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusAsciiMaster));

    p_owner->__p_timer_manager = p_timer_manager;

    p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__p_timer) goto fail;

    pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerTimeout, p_owner);
    p_owner->__timeout = PIF_MODBUS_MASTER_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifModbusAsciiMaster_Clear(p_owner);
	return FALSE;
}

void pifModbusAsciiMaster_Clear(PifModbusAsciiMaster *p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
}

void pifModbusAsciiMaster_SetResponseTimeout(PifModbusAsciiMaster *p_owner, uint16_t response_timeout)
{
	p_owner->__timeout = response_timeout;
}

void pifModbusAsciiMaster_AttachUart(PifModbusAsciiMaster *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusAsciiMaster_DetachUart(PifModbusAsciiMaster *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifModbusAsciiMaster_ReadCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	uint16_t i;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!slave || quantity < 1 || quantity > 2000 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_READ_COILS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);

	if (!_requestAndResponse(p_owner, 13)) return FALSE;

	for (i = 0; i < p_owner->__buffer[2]; i++) {
		p_coils[i] = p_owner->__buffer[3 + i];
	}
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadDiscreteInputs(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs)
{
	uint16_t i;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || quantity < 1 || quantity > 2000 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_READ_DISCRETE_INPUTS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);

	if (!_requestAndResponse(p_owner, 13)) return FALSE;

	for (i = 0; i < p_owner->__buffer[2]; i++) {
		p_inputs[i] = p_owner->__buffer[3 + i];
	}
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadHoldingRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
{
	uint16_t i;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || quantity < 1 || quantity > 125 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_READ_HOLDING_REGISTERS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);

	if (!_requestAndResponse(p_owner, 13)) return FALSE;

	for (i = 0; i < quantity; i++) {
		p_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadInputRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
{
	uint16_t i;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || quantity < 1 || quantity > 125 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_READ_INPUT_REGISTERS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);

	if (!_requestAndResponse(p_owner, 13)) return FALSE;

	for (i = 0; i < quantity; i++) {
		p_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}

BOOL pifModbusAsciiMaster_WriteSingleCoil(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, BOOL value)
{
	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_SINGLE_COIL, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(value ? 0xFF00 : 0x0000, &p_owner->__buffer[9]);

	return _requestAndResponse(p_owner, 13);
}

BOOL pifModbusAsciiMaster_WriteSingleRegister(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value)
{
	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_SINGLE_REGISTER, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(value, &p_owner->__buffer[9]);

	return _requestAndResponse(p_owner, 13);
}

BOOL pifModbusAsciiMaster_WriteMultipleCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	uint8_t length;
	uint16_t i, pos;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || quantity < 1 || quantity > 1968 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_MULTIPLE_COILS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);
	length = (quantity + 7) / 8;
	pifModbusAscii_CharToAscii(length, &p_owner->__buffer[13]);
	pos = 15;
	for (i = 0; i < length; i++, pos += 2) {
		pifModbusAscii_CharToAscii(p_coils[i], &p_owner->__buffer[pos]);
	}

	return _requestAndResponse(p_owner, pos);
}

BOOL pifModbusAsciiMaster_WriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
{
	uint8_t length;
	uint16_t i, pos;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || quantity < 1 || quantity > 123 || (uint32_t)address + (uint32_t)quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_MULTIPLE_REGISTERS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);
	length = 2 * quantity;
	pifModbusAscii_CharToAscii(length, &p_owner->__buffer[13]);
	pos = 15;
	for (i = 0; i < quantity; i++, pos += 4) {
		pifModbusAscii_ShortToAscii(p_registers[i], &p_owner->__buffer[pos]);
	}

	return _requestAndResponse(p_owner, pos);
}

BOOL pifModbusAsciiMaster_ReadWriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers)
{
	uint8_t length;
	uint16_t i, pos;

	if (p_owner->__state != MBMS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

    if (!slave || read_quantity < 1 || read_quantity > 0x7D || (uint32_t)read_address + (uint32_t)read_quantity > ((uint32_t)0xFFFF) + 1 ||
    		 write_quantity < 1 || write_quantity > 0x79 || (uint32_t)write_address + (uint32_t)write_quantity > ((uint32_t)0xFFFF) + 1) {
		pif_error = MBE_INVALID_ARGUMENT;
		return FALSE;
    }

	p_owner->__tx_address = slave;
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_READ_WRITE_MULTIPLE_REGISTERS, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(read_address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(read_quantity, &p_owner->__buffer[9]);
	pifModbusAscii_ShortToAscii(write_address, &p_owner->__buffer[13]);
	pifModbusAscii_ShortToAscii(write_quantity, &p_owner->__buffer[17]);
	length = 2 * write_quantity;
	pifModbusAscii_CharToAscii(length, &p_owner->__buffer[21]);
	pos = 23;
	for (i = 0; i < write_quantity; i++, pos += 4) {
		pifModbusAscii_ShortToAscii(p_write_registers[i], &p_owner->__buffer[pos]);
	}

	if (!_requestAndResponse(p_owner, pos)) return FALSE;

	for (i = 0; i < read_quantity; i++) {
		p_read_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}
