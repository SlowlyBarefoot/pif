#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_modbus_ascii_slave.h"


/**
 * @brief Serializes and queues a normal response command frame for transmission.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param function Input argument used by this operation.
 * @param length Frame length in bytes used by the current operation.
 * @return None.
 */
static void _sendCommand(PifModbusAsciiSlave *p_owner, uint8_t function, uint16_t length)
{
	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);

	p_owner->__buffer[0] = ':';
	pifModbusAscii_CharToAscii(p_owner->parent._my_addr, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(function, &p_owner->__buffer[3]);
	pifModbusAscii_CharToAscii(pifModbusAscii_CalcLrc(&p_owner->__buffer[1], length - 1), &p_owner->__buffer[length]);
	length += 2;
	p_owner->__buffer[length] = ASCII_CR; length++;
	p_owner->__buffer[length] = ASCII_LF; length++;

	p_owner->__length = length;
	p_owner->__index = 0;
	p_owner->parent.__state = MBSS_PRE_DELAY;
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
}

/**
 * @brief Decodes a write command and applies it through the supplied slave callback.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param func Handler callback used to process the decoded command data.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _writeCommand(PifModbusAsciiSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value))
{
	uint16_t address, value;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &address, &value);
	if (exception) return exception;

	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(value, &p_owner->__buffer[9]);
	_sendCommand(p_owner, p_owner->__rx.function, 13);
	return exception;
}

/**
 * @brief Decodes a bit-field read command and prepares the corresponding response payload.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param func Handler callback used to process the decoded command data.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _readBitField(PifModbusAsciiSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_quantity))
{
	uint8_t length;
	uint16_t i, pos, quantity;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &quantity);
	if (exception) return exception;

	length = (quantity + 7) / 8;
	pifModbusAscii_CharToAscii(length, &p_owner->__buffer[5]);
	pos = 7;
	for (i = 0; i < length; i++, pos += 2) {
		pifModbusAscii_CharToAscii(p_owner->parent.__p_tmp_bits[i], &p_owner->__buffer[pos]);
	}
	_sendCommand(p_owner, p_owner->__rx.function, pos);
	return exception;
}

/**
 * @brief Decodes a register read command and fills the response register payload.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param func Handler callback used to process the decoded command data.
 * @param p_registers Register buffer used for read or write operations.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _readRegisters(PifModbusAsciiSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity), uint16_t *p_registers)
{
	uint8_t length;
	uint16_t i, pos, address, quantity;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &address, &quantity);
	if (exception) return exception;

	length = 2 * quantity;
	pifModbusAscii_CharToAscii(length, &p_owner->__buffer[5]);
	pos = 7;
	for (i = 0; i < quantity; i++, pos += 4) {
		pifModbusAscii_ShortToAscii(p_registers[address + i], &p_owner->__buffer[pos]);
	}
	_sendCommand(p_owner, p_owner->__rx.function, pos);
	return exception;
}

/**
 * @brief Serializes and queues an exception response frame for transmission.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param exception Modbus exception code to transmit.
 * @return None.
 */
static void _sendException(PifModbusAsciiSlave *p_owner, uint8_t exception)
{
	pifModbusAscii_CharToAscii(exception, &p_owner->__buffer[5]);
	_sendCommand(p_owner, 0x80 | p_owner->__rx.function, 7);
}

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
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _parsingPacket(PifModbusAsciiSlave *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data, lrc[2];
	uint16_t i;
	PifModbusError exception = MBE_NONE;

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx.state) {
		case MBRS_IDLE:
			if (data == ':') {
				p_owner->__rx.state = MBRS_DATA;
				p_owner->__index = 0;
				if (!pifTimer_Start(p_owner->__p_timer, p_owner->__timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_WARN, "MB(%u) Not start timer", p_owner->_id);
#endif
				}
			}
			break;

		case MBRS_DATA:
			if (data == ASCII_CR) {
			}
			else if (data == ASCII_LF) {
				lrc[0] = pifModbusAscii_CalcLrc(p_owner->__buffer, p_owner->__index - 2);
				lrc[1] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[p_owner->__index - 2]);
				p_owner->__buffer[0] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[0]);
				if (p_owner->__buffer[0] == p_owner->parent._my_addr) {
					if (lrc[0] == lrc[1]) {
						for (i = 2; i < p_owner->__index - 2; i += 2) {
							p_owner->__buffer[i / 2] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[i]);
						}
						p_owner->__rx.function = p_owner->__buffer[1];
						switch (p_owner->__rx.function) {
						case MBF_READ_COILS:
						case MBF_READ_DISCRETE_INPUTS:
						case MBF_READ_HOLDING_REGISTERS:
						case MBF_READ_INPUT_REGISTERS:
						case MBF_READ_WRITE_MULTIPLE_REGISTERS:
						case MBF_WRITE_SINGLE_COIL:
						case MBF_WRITE_SINGLE_REGISTER:
						case MBF_WRITE_MULTIPLE_COILS:
						case MBF_WRITE_MULTIPLE_REGISTERS:
							p_owner->__rx.state = MBRS_FINISH;
							break;

						default:
							exception = MBE_INVALID_RESPONSE;
							p_owner->__rx.state = MBRS_ERROR;
#ifndef PIF_NO_LOG
							pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) A:%u F:%u", p_owner->_id,
									c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)p_owner->__buffer[0],
									(unsigned int)p_owner->__buffer[1]);
#endif
						}
					}
					else {
						exception = MBE_CRC;
						p_owner->__rx.state = MBRS_ERROR;
#ifndef PIF_NO_LOG
						pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) %X!=%X", p_owner->_id,
								c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)lrc[0], (unsigned int)lrc[1]);
#endif
					}
				}
				else {
			        pifTimer_Stop(p_owner->__p_timer);
					p_owner->__rx.state = MBRS_IDLE;
				}
			}
			else {
				p_owner->__buffer[p_owner->__index] = data;
				p_owner->__index++;
			}
			break;

		default:
			break;
		}
	}
	return exception;
}

/**
 * @brief Driver callback that consumes received bytes and dispatches parsed events.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return TRUE when the callback handled data; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusAsciiSlave *p_owner = (PifModbusAsciiSlave *)p_client;
	PifModbusSlave *p_parent = &p_owner->parent;
	uint8_t *p_coils;
	uint16_t i, read_address, read_quantity, write_address, write_quantity, pos, value;
	BOOL status;
	PifModbusError exception;

	exception = _parsingPacket(p_owner, act_receive_data);

	switch (p_owner->__rx.state) {
	case MBRS_FINISH:
        pifTimer_Stop(p_owner->__p_timer);
		switch (p_owner->__buffer[1]) {
		case MBF_READ_COILS:
			exception = _readBitField(p_owner, pifModbusSlave_ReadCoils);
			if (exception) goto fail;
			break;

		case MBF_READ_DISCRETE_INPUTS:
			exception = _readBitField(p_owner, pifModbusSlave_ReadDiscreteInputs);
			if (exception) goto fail;
			break;

		case MBF_READ_HOLDING_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadHoldingRegisters, p_parent->__p_holding_registers);
			if (exception) goto fail;
			break;

		case MBF_READ_INPUT_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadInputRegisters, p_parent->__p_input_registers);
			if (exception) goto fail;
			break;

		case MBF_WRITE_SINGLE_COIL:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteSingleCoil);
			if (exception) goto fail;
			break;

		case MBF_WRITE_SINGLE_REGISTER:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteSingleRegister);
			if (exception) goto fail;
			break;

		case MBF_WRITE_MULTIPLE_COILS:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteMultipleCoils);
			if (exception) goto fail;
			break;

		case MBF_WRITE_MULTIPLE_REGISTERS:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteMultipleRegisters);
			if (exception) goto fail;
			break;

		case MBF_READ_WRITE_MULTIPLE_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadWriteMultipleRegisters, p_parent->__p_holding_registers);
			if (exception) goto fail;
			break;

		default:
			exception = MBE_ILLEGAL_FUNCTION;
			goto fail;
		}
		p_owner->__rx.state = MBRS_IDLE;
		break;

	case MBRS_TIMEOUT:
		exception = MBE_ILLEGAL_DATA_VALUE;
		goto fail;

	case MBRS_ERROR:
        pifTimer_Stop(p_owner->__p_timer);
		goto fail;

	default:
		return FALSE;
	}
	return TRUE;

fail:
	_sendException(p_owner, exception);
	p_owner->__rx.state = MBRS_IDLE;
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
	PifModbusAsciiSlave *p_owner = (PifModbusAsciiSlave *)p_client;
	uint16_t length, period = 0;

	switch (p_owner->parent.__state) {
	case MBSS_PRE_DELAY:
		p_owner->parent.__state = MBSS_SEND;
		period = 1;
		break;

	case MBSS_SEND:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__buffer + p_owner->__index,
    			p_owner->__length - p_owner->__index);
		p_owner->__index += length;
		if (p_owner->__index >= p_owner->__length) {
			p_owner->parent.__state = p_owner->__p_uart->__act_direction ? MBSS_WAIT : MBSS_IDLE;
		}
		period = 1;
		break;

	case MBSS_WAIT:
		period = 1;
		if (pifUart_CheckTxTransfer(p_owner->__p_uart)) {
			p_owner->parent.__state = MBSS_POST_DELAY;
			period *= 2;
		}
		break;

	case MBSS_POST_DELAY:
		(*p_owner->__p_uart->__act_direction)(UD_RX);
		p_owner->parent.__state = MBSS_IDLE;
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
static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	PifModbusAsciiSlave *p_owner = (PifModbusAsciiSlave *)p_issuer;

	switch (p_owner->parent.__state) {
	default:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(Timeout) State:%u Cnt:%u Len:%d", p_owner->_id,
				p_owner->__rx.state, p_owner->__index, p_owner->__length);
#endif
		p_owner->__rx.state = MBRS_TIMEOUT;
		break;
	}
}

BOOL pifModbusAsciiSlave_Init(PifModbusAsciiSlave *p_owner, PifId id, PifTimerManager *p_timer_manager, uint8_t my_addr)
{
    if (!p_owner || !p_timer_manager || my_addr == 0 || my_addr > 248) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusAsciiSlave));

	p_owner->parent._my_addr = my_addr;
	p_owner->parent.__p_buffer = p_owner->__buffer;
    p_owner->__p_timer_manager = p_timer_manager;

    p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__p_timer) goto fail;

    pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerRxTimeout, p_owner);
    p_owner->__timeout = PIF_MODBUS_SLAVE_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifModbusAsciiSlave_Clear(p_owner);
	return FALSE;
}

void pifModbusAsciiSlave_Clear(PifModbusAsciiSlave *p_owner)
{
	PifModbusSlave *p_parent = &p_owner->parent;

	if (p_parent->__p_tmp_bits) {
		free(p_parent->__p_tmp_bits);
		p_parent->__p_tmp_bits = NULL;
	}
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
}

void pifModbusAsciiSlave_SetReceiveTimeout(PifModbusAsciiSlave *p_owner, uint16_t receive_timeout)
{
	p_owner->__timeout = receive_timeout;
}

void pifModbusAsciiSlave_AttachUart(PifModbusAsciiSlave *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusAsciiSlave_DetachUart(PifModbusAsciiSlave *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}
