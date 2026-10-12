// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_modbus_rtu_slave.h"


/**
 * @brief Serializes and queues a normal response command frame for transmission. A broadcast
 *        request is carried out but never answered, so nothing goes out for one.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param function Input argument used by this operation.
 * @param length Frame length in bytes used by the current operation.
 * @return None.
 */
static void _sendCommand(PifModbusRtuSlave *p_owner, uint8_t function, uint16_t length)
{
	if (p_owner->__rx.broadcast) return;

	pifUart_SetDirection(p_owner->__p_uart, UD_TX);

	p_owner->__buffer[0] = p_owner->parent._my_addr;
	p_owner->__buffer[1] = function;
	pifModbus_ShortToStream(pifModbusRtu_CalcCrc(p_owner->__buffer, length), &p_owner->__buffer[length]);
	length += 2;

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
static PifModbusError _writeCommand(PifModbusRtuSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value))
{
	uint16_t address, value;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &address, &value);
	if (exception) return exception;

	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(value, &p_owner->__buffer[4]);
	_sendCommand(p_owner, p_owner->__rx.function, 6);
	return exception;
}

/**
 * @brief Decodes a bit-field read command and prepares the corresponding response payload.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param func Handler callback used to process the decoded command data.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _readBitField(PifModbusRtuSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_quantity))
{
	uint16_t i, pos, quantity;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &quantity);
	if (exception) return exception;

	p_owner->__buffer[2] = (quantity + 7) / 8;
	pos = 3;
	for (i = 0; i < p_owner->__buffer[2]; i++, pos++) {
		p_owner->__buffer[pos] = p_owner->parent.__p_tmp_bits[i];
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
static PifModbusError _readRegisters(PifModbusRtuSlave *p_owner, PifModbusError (*func)(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity), uint16_t *p_registers)
{
	uint16_t i, pos, address, quantity;
	PifModbusError exception;

	exception = (*func)(&p_owner->parent, &address, &quantity);
	if (exception) return exception;

	p_owner->__buffer[2] = 2 * quantity;
	pos = 3;
	for (i = 0; i < quantity; i++, pos += 2) {
		pifModbus_ShortToStream(p_registers[address + i], &p_owner->__buffer[pos]);
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
static void _sendException(PifModbusRtuSlave *p_owner, uint8_t exception)
{
	p_owner->__buffer[2] = exception;
	_sendCommand(p_owner, 0x80 | p_owner->__rx.function, 3);
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
 * @brief Drops the frame being received and waits for the next one, with nothing known about it.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _resetRx(PifModbusRtuSlave *p_owner)
{
	pifModbus_StopTimeout(&p_owner->__wait);
	p_owner->__rx.state = MBRS_IDLE;
	p_owner->__rx.response = FALSE;
	p_owner->__rx.illegal = FALSE;
}

/**
 * @brief Ignores the rest of a frame whose end cannot be told from its content. The frame is taken
 *        to be over once the line has been silent for 3.5 characters.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param illegal TRUE when it is a request to this slave with a function it lacks, which is answered
 *        with exception 1 once the frame is in and its CRC holds.
 */
static void _ignoreRx(PifModbusRtuSlave *p_owner, BOOL illegal)
{
	p_owner->__rx.state = MBRS_IGNORE;
	p_owner->__rx.illegal = illegal;
}

/**
 * @brief Sets how long a frame is from its function code. Every frame on the line is followed this
 *        way, the ones for other slaves too, because the end of a frame is only known from its
 *        content: the UART task reads bytes in batches, so the silent interval between two frames
 *        cannot be seen reliably.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param function Function code that was received.
 */
static void _setFrameLength(PifModbusRtuSlave *p_owner, uint8_t function)
{
	if (p_owner->__rx.response) {
		// The answer of another slave to the request just before.
		if (function & 0x80) {
			p_owner->__length = 2 + 1 + 2;
			p_owner->__rx.state = MBRS_DATA;
			return;
		}
		switch (function) {
		case MBF_READ_COILS:
		case MBF_READ_DISCRETE_INPUTS:
		case MBF_READ_HOLDING_REGISTERS:
		case MBF_READ_INPUT_REGISTERS:
		case MBF_READ_WRITE_MULTIPLE_REGISTERS:
			p_owner->__length = 2 + 1 + 2;
			p_owner->__rx.length_pos = 2;
			p_owner->__rx.state = MBRS_LENGTH;
			break;

		case MBF_WRITE_SINGLE_COIL:
		case MBF_WRITE_SINGLE_REGISTER:
		case MBF_WRITE_MULTIPLE_COILS:
		case MBF_WRITE_MULTIPLE_REGISTERS:
			p_owner->__length = 2 + 4 + 2;
			p_owner->__rx.state = MBRS_DATA;
			break;

		default:
			_ignoreRx(p_owner, FALSE);
			break;
		}
		return;
	}

	switch (function) {
	case MBF_READ_COILS:
	case MBF_READ_DISCRETE_INPUTS:
	case MBF_READ_HOLDING_REGISTERS:
	case MBF_READ_INPUT_REGISTERS:
	case MBF_WRITE_SINGLE_COIL:
	case MBF_WRITE_SINGLE_REGISTER:
		p_owner->__length = 2 + 4 + 2;
		p_owner->__rx.state = MBRS_DATA;
		break;

	case MBF_WRITE_MULTIPLE_COILS:
	case MBF_WRITE_MULTIPLE_REGISTERS:
		p_owner->__length = 2 + 4 + 1 + 2;
		p_owner->__rx.length_pos = 6;
		p_owner->__rx.state = MBRS_LENGTH;
		break;

	case MBF_READ_WRITE_MULTIPLE_REGISTERS:
		p_owner->__length = 2 + 8 + 1 + 2;
		p_owner->__rx.length_pos = 10;
		p_owner->__rx.state = MBRS_LENGTH;
		break;

	default:
#ifndef PIF_NO_LOG
		if (p_owner->__buffer[0] == p_owner->parent._my_addr) {
			pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) A:%u F:%u", p_owner->_id,
					c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)p_owner->__buffer[0],
					(unsigned int)p_owner->__buffer[1]);
		}
#endif
		_ignoreRx(p_owner, p_owner->__buffer[0] == p_owner->parent._my_addr);
		break;
	}
}

/**
 * @brief Decides what a frame whose CRC holds is: a request for this slave, which is carried out,
 *        or a frame between the master and another slave, which is passed over.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _completeFrame(PifModbusRtuSlave *p_owner)
{
	uint8_t address = p_owner->__buffer[0];

	pifModbus_StopTimeout(&p_owner->__wait);
	p_owner->__rx.end_time = (*pif_act_timer1us)();

	if (p_owner->__rx.response) {
		p_owner->__rx.response = FALSE;
		p_owner->__rx.state = MBRS_IDLE;
	}
	else if (address == p_owner->parent._my_addr || address == PIF_MODBUS_BROADCAST_ADDRESS) {
		p_owner->__rx.broadcast = address == PIF_MODBUS_BROADCAST_ADDRESS;
		p_owner->__rx.state = MBRS_FINISH;
	}
	else {
		// The slave it went to answers next, unless it was a broadcast.
		p_owner->__rx.response = TRUE;
		p_owner->__rx.state = MBRS_IDLE;
	}
}

/**
 * @brief Parses an incoming protocol packet and updates parser state and outputs.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return Modbus error status produced by command validation or execution.
 */
static PifModbusError _parsingPacket(PifModbusRtuSlave *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data;
	uint16_t crc[2];
	BOOL received = FALSE;
	uint32_t now;

	// No byte of the frame came for the whole timeout: it is dropped without an answer, and
	// whatever comes now starts a new one.
	if (pifModbus_IsExpired(&p_owner->__wait)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(Timeout) State:%u Cnt:%u Len:%d", p_owner->_id,
				p_owner->__rx.state, p_owner->__index, p_owner->__length);
#endif
		_resetRx(p_owner);
	}

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		received = TRUE;
		now = (*pif_act_timer1us)();
		p_owner->__rx.last_time = now;
		// The wait runs from byte to byte, so a long frame at a low baud rate is not cut short.
		pifModbus_StartTimeout(&p_owner->__wait, p_owner->__timeout * 1000UL);

		switch (p_owner->__rx.state) {
		case MBRS_IDLE:
			// An answer that was due never came if the line stayed quiet that long, so this is a request.
			if (p_owner->__rx.response && (uint32_t)(now - p_owner->__rx.end_time) >= p_owner->__timeout * 1000UL) {
				p_owner->__rx.response = FALSE;
			}
			p_owner->__buffer[0] = data;
			p_owner->__rx.state = MBRS_FUNCTION;
			break;

		case MBRS_FUNCTION:
			p_owner->__buffer[1] = data;
			p_owner->__rx.function = data;
			p_owner->__index = 2;
			_setFrameLength(p_owner, data);
			break;

		case MBRS_LENGTH:
			p_owner->__buffer[p_owner->__index] = data;
			if (p_owner->__index == p_owner->__rx.length_pos) {
				if (p_owner->__length + data > sizeof(p_owner->__buffer)) {
					// Longer than any valid frame.
					_ignoreRx(p_owner, FALSE);
					break;
				}
				p_owner->__length += data;
				p_owner->__rx.state = MBRS_DATA;
			}
			p_owner->__index++;
			break;

		case MBRS_DATA:
			p_owner->__buffer[p_owner->__index++] = data;
			if (p_owner->__index >= p_owner->__length) {
				crc[0] = pifModbusRtu_CalcCrc(p_owner->__buffer, p_owner->__index - 2);
				crc[1] = pifModbus_StreamToShort(&p_owner->__buffer[p_owner->__index - 2]);
				if (crc[0] == crc[1]) {
					_completeFrame(p_owner);
					if (p_owner->__rx.state == MBRS_FINISH) return MBE_NONE;
				}
				else {
					// A frame that arrived damaged is dropped without an answer.
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) %u!=%u", p_owner->_id, c_cPktErr[PKT_ERR_WRONG_CRC],
							(unsigned int)crc[0], (unsigned int)crc[1]);
#endif
					_ignoreRx(p_owner, FALSE);
				}
			}
			break;

		case MBRS_IGNORE:
			if (p_owner->__rx.illegal) {
				if (p_owner->__index < sizeof(p_owner->__buffer)) {
					p_owner->__buffer[p_owner->__index++] = data;
				}
				else {
					p_owner->__rx.illegal = FALSE;
				}
			}
			break;

		default:
			break;
		}
	}

	// A frame being ignored is over once a whole call reads nothing and 3.5 characters have passed.
	if (!received && p_owner->__rx.state == MBRS_IGNORE &&
			(long)((*pif_act_timer1us)() - p_owner->__rx.last_time) >= p_owner->__p_uart->_transfer_time * 7L / 2) {
		if (p_owner->__rx.illegal && p_owner->__index >= 4 &&
				pifModbusRtu_CalcCrc(p_owner->__buffer, p_owner->__index - 2) == pifModbus_StreamToShort(&p_owner->__buffer[p_owner->__index - 2])) {
			pifModbus_StopTimeout(&p_owner->__wait);
			p_owner->__rx.end_time = p_owner->__rx.last_time;
			p_owner->__rx.illegal = FALSE;
			p_owner->__rx.broadcast = FALSE;
			p_owner->__rx.state = MBRS_ERROR;
			return MBE_ILLEGAL_FUNCTION;
		}
		_resetRx(p_owner);
	}
	return MBE_NONE;
}

/**
 * @brief Driver callback that consumes received bytes and dispatches parsed events.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return TRUE when the callback handled data; otherwise FALSE.
 */
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusRtuSlave *p_owner = (PifModbusRtuSlave *)p_client;
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
static uint16_t _evtSending(void *p_client, PifActUartSendData act_send_data)
{
	PifModbusRtuSlave *p_owner = (PifModbusRtuSlave *)p_client;
	uint16_t length, period = 0;
	uint32_t interval, diff;

	switch (p_owner->parent.__state) {
	case MBSS_PRE_DELAY:
		p_owner->parent.__state = MBSS_SEND;
		// The line has to stay silent for 3.5 characters after the request. The period returned is
		// counted in characters.
		interval = p_owner->__p_uart->_transfer_time * 7UL / 2;
		diff = (*pif_act_timer1us)() - p_owner->__rx.end_time;
		if (diff < interval) {
			period = (interval - diff) / p_owner->__p_uart->_transfer_time + 1;
		}
		else {
			period = 1;
		}
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
		pifUart_SetDirection(p_owner->__p_uart, UD_RX);
		p_owner->parent.__state = MBSS_IDLE;
		break;

	default:
		break;
	}
	return period;
}

BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, uint8_t my_addr)
{
    if (!p_owner || my_addr == 0 || my_addr > 247) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusRtuSlave));

	p_owner->parent._my_addr = my_addr;
	p_owner->parent.__p_buffer = p_owner->__buffer;
    p_owner->__timeout = PIF_MODBUS_SLAVE_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifModbusRtuSlave_Clear(PifModbusRtuSlave *p_owner)
{
	PifModbusSlave *p_parent = &p_owner->parent;

	if (p_parent->__p_tmp_bits) {
		free(p_parent->__p_tmp_bits);
		p_parent->__p_tmp_bits = NULL;
	}
}

void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout)
{
	p_owner->__timeout = receive_timeout;
}

void pifModbusRtuSlave_AttachUart(PifModbusRtuSlave *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
    pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusRtuSlave_DetachUart(PifModbusRtuSlave *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}
