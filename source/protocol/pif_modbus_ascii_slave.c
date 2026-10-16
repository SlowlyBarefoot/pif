// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_modbus_ascii_slave.h"


/**
 * @brief Serializes and queues a normal response command frame for transmission. A broadcast
 *        request is carried out but never answered, so nothing goes out for one.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param function Input argument used by this operation.
 * @param length Frame length in bytes used by the current operation.
 * @return None.
 */
static void _sendCommand(PifModbusAsciiSlave *p_owner, uint8_t function, uint16_t length)
{
	if (p_owner->__rx.broadcast) return;

	p_owner->__buffer[0] = ':';
	pifModbusAscii_CharToAscii(p_owner->parent._my_addr, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(function, &p_owner->__buffer[3]);
	pifModbusAscii_CharToAscii(pifModbusAscii_CalcLrcOfAscii(&p_owner->__buffer[1], length - 1), &p_owner->__buffer[length]);
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

/**
 * @brief Checks a frame that has come in up to its LF, decoded into bytes in the buffer, and
 *        decides what to do with it.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @return The exception to answer with, or MBE_NONE.
 */
static PifModbusError _completeFrame(PifModbusAsciiSlave *p_owner)
{
	int16_t length;
	uint16_t expected;
	uint8_t address;

	pifTaskTimeout_Stop(&p_owner->__timeout);
	p_owner->__rx.state = MBRS_IDLE;

	// A frame that arrived damaged or went to another slave is dropped without an answer.
	length = pifModbusAscii_Decode(p_owner->__buffer, p_owner->__index);
	if (length < 3) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_BAD_FRAME ") Len:%u", p_owner->_id,
				(unsigned int)p_owner->__index);
#endif
		return MBE_NONE;
	}
	length--;
	address = p_owner->__buffer[0];
	if (address != p_owner->parent._my_addr && address != PIF_MODBUS_BROADCAST_ADDRESS) return MBE_NONE;

	if (pifModbusAscii_CalcLrc(p_owner->__buffer, length) != p_owner->__buffer[length]) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_WRONG_CRC ") %X!=%X", p_owner->_id,
				(unsigned int)pifModbusAscii_CalcLrc(p_owner->__buffer, length), (unsigned int)p_owner->__buffer[length]);
#endif
		return MBE_NONE;
	}

	p_owner->__rx.broadcast = address == PIF_MODBUS_BROADCAST_ADDRESS;
	p_owner->__rx.function = p_owner->__buffer[1];
	p_owner->__rx.state = MBRS_ERROR;

	// Unlike RTU, the frame does not have to be as long as its function says, so that is checked here.
	switch (p_owner->__rx.function) {
	case MBF_READ_COILS:
	case MBF_READ_DISCRETE_INPUTS:
	case MBF_READ_HOLDING_REGISTERS:
	case MBF_READ_INPUT_REGISTERS:
	case MBF_WRITE_SINGLE_COIL:
	case MBF_WRITE_SINGLE_REGISTER:
		expected = 6;
		break;

	case MBF_WRITE_MULTIPLE_COILS:
	case MBF_WRITE_MULTIPLE_REGISTERS:
		expected = length > 6 ? 7 + p_owner->__buffer[6] : 7;
		break;

	case MBF_READ_WRITE_MULTIPLE_REGISTERS:
		expected = length > 10 ? 11 + p_owner->__buffer[10] : 11;
		break;

	default:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_UNKNOWN_FUNCTION ") A:%u F:%u", p_owner->_id,
				(unsigned int)p_owner->__buffer[0],	(unsigned int)p_owner->__buffer[1]);
#endif
		return MBE_ILLEGAL_FUNCTION;
	}
	if (length != expected) return MBE_ILLEGAL_DATA_VALUE;

	p_owner->__rx.state = MBRS_FINISH;
	return MBE_NONE;
}

/**
 * @brief Parses an incoming protocol packet and updates parser state and outputs.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _parsingPacket(PifModbusAsciiSlave *p_owner, PifActTransportReceive act_receive_data)
{
	uint8_t data;
	PifModbusError exception;

	// No character of the frame came for the whole timeout: it is dropped without an answer.
	if (pifTaskTimeout_IsExpired(&p_owner->__timeout)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(Timeout) State:%u Cnt:%u", p_owner->_id,
				p_owner->__rx.state, p_owner->__index);
#endif
		p_owner->__rx.state = MBRS_IDLE;
	}

	while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1)) {
		// A colon always starts a frame, whatever came before it.
		if (data == ':') {
			p_owner->__rx.state = MBRS_DATA;
			p_owner->__index = 0;
			pifTaskTimeout_Start(&p_owner->__timeout, p_owner->__timeout_ms * 1000UL);
			continue;
		}

		switch (p_owner->__rx.state) {
		case MBRS_DATA:
			// The wait runs from character to character, so a long frame at a low baud rate is not
			// cut short.
			pifTaskTimeout_Start(&p_owner->__timeout, p_owner->__timeout_ms * 1000UL);

			if (data == ASCII_CR) {
			}
			else if (data == ASCII_LF) {
				exception = _completeFrame(p_owner);
				if (p_owner->__rx.state != MBRS_IDLE) return exception;
			}
			else if (p_owner->__index < sizeof(p_owner->__buffer)) {
				p_owner->__buffer[p_owner->__index] = data;
				p_owner->__index++;
			}
			else {
				// Longer than any valid frame.
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_BAD_FRAME ") Overflow", p_owner->_id);
#endif
		        pifTaskTimeout_Stop(&p_owner->__timeout);
				p_owner->__rx.state = MBRS_IDLE;
			}
			break;

		default:
			break;
		}
	}
	return MBE_NONE;
}

/**
 * @brief Driver callback that consumes received bytes and dispatches parsed events.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return TRUE when the callback handled data; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive_data)
{
	PifModbusAsciiSlave *p_owner = (PifModbusAsciiSlave *)p_client;
	PifModbusSlave *p_parent = &p_owner->parent;
	PifModbusError exception;

	exception = _parsingPacket(p_owner, act_receive_data);

	switch (p_owner->__rx.state) {
	case MBRS_FINISH:
		switch (p_owner->__buffer[1]) {
		case MBF_READ_COILS:
			exception = _readBitField(p_owner, pifModbusSlave_ReadCoils);
			break;

		case MBF_READ_DISCRETE_INPUTS:
			exception = _readBitField(p_owner, pifModbusSlave_ReadDiscreteInputs);
			break;

		case MBF_READ_HOLDING_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadHoldingRegisters, p_parent->__p_holding_registers);
			break;

		case MBF_READ_INPUT_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadInputRegisters, p_parent->__p_input_registers);
			break;

		case MBF_WRITE_SINGLE_COIL:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteSingleCoil);
			break;

		case MBF_WRITE_SINGLE_REGISTER:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteSingleRegister);
			break;

		case MBF_WRITE_MULTIPLE_COILS:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteMultipleCoils);
			break;

		case MBF_WRITE_MULTIPLE_REGISTERS:
			exception = _writeCommand(p_owner, pifModbusSlave_WriteMultipleRegisters);
			break;

		case MBF_READ_WRITE_MULTIPLE_REGISTERS:
			exception = _readRegisters(p_owner, pifModbusSlave_ReadWriteMultipleRegisters, p_parent->__p_holding_registers);
			break;

		default:
			exception = MBE_ILLEGAL_FUNCTION;
			break;
		}
		break;

	case MBRS_ERROR:
		break;

	default:
		return FALSE;
	}

	if (exception) _sendException(p_owner, exception);
	p_owner->__rx.state = MBRS_IDLE;
	return TRUE;
}

/**
 * @brief Driver callback that emits pending transmit bytes from the protocol state machine.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_send_data Callback used to push outgoing bytes to the underlying driver.
 * @return Number of bytes transmitted during this callback invocation.
 */
static uint16_t _evtSending(void *p_client, PifActTransportSend act_send_data)
{
	PifModbusAsciiSlave *p_owner = (PifModbusAsciiSlave *)p_client;
	uint16_t length, period = 0;

	switch (p_owner->parent.__state) {
	case MBSS_PRE_DELAY:
		p_owner->parent.__state = MBSS_SEND;
		period = 1;
		break;

	case MBSS_SEND:
    	length = (*act_send_data)(&p_owner->__p_uart->parent, p_owner->__buffer + p_owner->__index,
    			p_owner->__length - p_owner->__index);
		p_owner->__index += length;
		if (p_owner->__index >= p_owner->__length) {
			p_owner->parent.__state = MBSS_IDLE;
		}
		period = 1;
		break;

	default:
		break;
	}
	return period;
}

BOOL pifModbusAsciiSlave_Init(PifModbusAsciiSlave *p_owner, PifId id, uint8_t my_addr)
{
    if (!p_owner || my_addr == 0 || my_addr > 247) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusAsciiSlave));

	p_owner->parent._my_addr = my_addr;
	p_owner->parent.__p_buffer = p_owner->__buffer;
    p_owner->__timeout_ms = PIF_MODBUS_SLAVE_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifModbusAsciiSlave_Clear(PifModbusAsciiSlave *p_owner)
{
	PifModbusSlave *p_parent = &p_owner->parent;

	if (p_parent->__p_tmp_bits) {
		free(p_parent->__p_tmp_bits);
		p_parent->__p_tmp_bits = NULL;
	}
}

void pifModbusAsciiSlave_SetReceiveTimeout(PifModbusAsciiSlave *p_owner, uint16_t receive_timeout)
{
	p_owner->__timeout_ms = receive_timeout;
}

void pifModbusAsciiSlave_AttachUart(PifModbusAsciiSlave *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, p_owner, _evtParsing, _evtSending, NULL);
}

void pifModbusAsciiSlave_DetachUart(PifModbusAsciiSlave *p_owner)
{
	pifTransport_DetachClient(&p_owner->__p_uart->parent);
	p_owner->__p_uart = NULL;
}
