#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_modbus_rtu_slave.h"


static void _sendCommand(PifModbusRtuSlave *p_owner, uint8_t function, uint16_t length)
{
	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);

	p_owner->__buffer[0] = p_owner->parent._my_addr;
	p_owner->__buffer[1] = function;
	pifModbus_ShortToStream(pifModbusRtu_CalcCrc(p_owner->__buffer, length), &p_owner->__buffer[length]);
	length += 2;

	p_owner->__length = length;
	p_owner->__index = 0;
	p_owner->parent.__state = MBSS_PRE_DELAY;
}

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

static PifModbusError _parsingPacket(PifModbusRtuSlave *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data;
	uint16_t crc[2];
	PifModbusError exception = MBE_NONE;

	if (p_owner->__rx.state == MBRS_IGNORE) {
		if ((long)((*pif_act_timer1us)() - p_owner->__last_receive_time) >= p_owner->__interval * 3.5) {
			p_owner->__rx.state = MBRS_IDLE;
		}
	}
	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		p_owner->__last_receive_time = (*pif_act_timer1us)();
		switch (p_owner->__rx.state) {
		case MBRS_IDLE:
			if (data == p_owner->parent._my_addr) {
				p_owner->__buffer[0] = data;
				p_owner->__rx.state = MBRS_FUNCTION;
				if (!pifTimer_Start(p_owner->__p_timer, p_owner->__timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_WARN, "MB(%u) Not start timer", p_owner->_id);
#endif
				}
			}
			else {
				p_owner->__rx.state = MBRS_IGNORE;
			}
			break;

		case MBRS_FUNCTION:
			p_owner->__buffer[1] = data;
			p_owner->__rx.function = data;
			p_owner->__index = 2;
			switch (data) {
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
				exception = MBE_ILLEGAL_FUNCTION;
				p_owner->__rx.state = MBRS_ERROR;
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) A:%u F:%u", p_owner->_id,
						c_cPktErr[PKT_ERR_INVALID_PACKET_NO], (unsigned int)p_owner->__buffer[0],
						(unsigned int)p_owner->__buffer[1]);
#endif
			}
			break;

		case MBRS_LENGTH:
			p_owner->__buffer[p_owner->__index] = data;
			if (p_owner->__index == p_owner->__rx.length_pos) {
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
		            p_owner->__rx.state = MBRS_FINISH;
				}
				else {
					exception = MBE_ILLEGAL_DATA_VALUE;
					p_owner->__rx.state = MBRS_ERROR;
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) %u!=%u", p_owner->_id, c_cPktErr[PKT_ERR_WRONG_CRC],
							(unsigned int)crc[0], (unsigned int)crc[1]);
#endif
				}
				p_owner->__rx.last_time = (*pif_act_timer1us)();
			}
			break;

		default:
			break;
		}
	}
	return exception;
}

static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusRtuSlave *p_owner = (PifModbusRtuSlave *)p_client;
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
		break;
	}
	return;

fail:
	_sendException(p_owner, exception);
	p_owner->__rx.state = MBRS_IDLE;
}

static uint16_t _evtSending(void *p_client, PifActUartSendData act_send_data)
{
	PifModbusRtuSlave *p_owner = (PifModbusRtuSlave *)p_client;
	uint16_t length, period = 0, interval;
	int32_t diff;

	switch (p_owner->parent.__state) {
	case MBSS_PRE_DELAY:
		p_owner->parent.__state = MBSS_SEND;
		interval = p_owner->__interval * 2.5;
		diff = (*pif_act_timer1us)() - p_owner->__rx.last_time;
		if (diff < interval) {
			period = interval - diff;
		}
		else {
			period = 10;
		}
		break;

	case MBSS_SEND:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__buffer + p_owner->__index,
    			p_owner->__length - p_owner->__index);
		p_owner->__index += length;
		if (p_owner->__index >= p_owner->__length) {
			p_owner->parent.__state = p_owner->__p_uart->__act_direction ? MBSS_WAIT : MBSS_IDLE;
		}
		period = p_owner->__interval;
		break;

	case MBSS_WAIT:
		period = p_owner->__interval;
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

static void _evtTimerRxTimeout(PifIssuerP p_issuer)
{
	PifModbusRtuSlave *p_owner = (PifModbusRtuSlave *)p_issuer;

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

BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, PifTimerManager *p_timer_manager, uint8_t my_addr)
{
    if (!p_owner || !p_timer_manager || !pif_act_timer1us || my_addr == 0 || my_addr > 248) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusRtuSlave));

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
	pifModbusRtuSlave_Clear(p_owner);
	return FALSE;
}

void pifModbusRtuSlave_Clear(PifModbusRtuSlave *p_owner)
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

void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout)
{
	p_owner->__timeout = receive_timeout;
}

void pifModbusRtuSlave_AttachUart(PifModbusRtuSlave *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
    p_owner->__interval = 1000000L / (p_uart->_baudrate / 10);
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusRtuSlave_DetachUart(PifModbusRtuSlave *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}
