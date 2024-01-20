#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_modbus_rtu_master.h"


#ifndef PIF_NO_LOG

#define PKT_ERR_INVALID_PACKET_NO	0
#define PKT_ERR_WRONG_CRC    		1

static const char *c_cPktErr[] = {
		"Invalid Packet No",
		"Wrong CRC"
};

#endif

static void _parsingPacket(PifModbusRtuMaster *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data;
	uint16_t crc[2];

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		switch (p_owner->__rx_state) {
		case MBRS_IDLE:
			p_owner->__buffer[0] = data;
			p_owner->__rx_state = MBRS_FUNCTION;
			break;

		case MBRS_FUNCTION:
			p_owner->__buffer[1] = data;
			p_owner->index = 2;
			if (data & 0x80) {
				p_owner->length = 2 + 1 + 2;
				p_owner->__rx_state = MBRS_DATA;
			}
			else {
				switch (data) {
				case MBF_READ_COILS:
				case MBF_READ_DISCRETE_INPUTS:
				case MBF_READ_HOLDING_REGISTERS:
				case MBF_READ_INPUT_REGISTERS:
				case MBF_READ_WRITE_MULTIPLE_REGISTERS:
					p_owner->length = 2 + 1 + 2;
					p_owner->__rx_state = MBRS_LENGTH;
					break;

				case MBF_WRITE_SINGLE_COIL:
				case MBF_WRITE_SINGLE_REGISTER:
				case MBF_WRITE_MULTIPLE_COILS:
				case MBF_WRITE_MULTIPLE_REGISTERS:
					p_owner->length = 2 + 4 + 2;
					p_owner->__rx_state = MBRS_DATA;
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
			break;

		case MBRS_LENGTH:
			p_owner->__buffer[p_owner->index++] = data;
			p_owner->length += data;
			p_owner->__rx_state = MBRS_DATA;
			break;

		case MBRS_DATA:
			p_owner->__buffer[p_owner->index++] = data;
			if (p_owner->index >= p_owner->length) {
				crc[0] = pifModbusRtu_CalcCrc(p_owner->__buffer, p_owner->index - 2);
				crc[1] = pifModbus_StreamToShort(&p_owner->__buffer[p_owner->index - 2]);
				if (crc[0] == crc[1]) {
		            p_owner->__rx_state = MBRS_FINISH;
				}
				else {
					p_owner->_error = MBE_CRC;
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(%s) %u!=%u", p_owner->_id,
							c_cPktErr[PKT_ERR_WRONG_CRC], (unsigned int)crc[0], (unsigned int)crc[1]);
#endif
					goto fail;
				}
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

static void _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusRtuMaster *p_owner = (PifModbusRtuMaster *)p_client;

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
		return;
	}

	pifTimer_Stop(p_owner->__p_timer);
	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);
	p_owner->__rx_state = MBRS_IDLE;
	p_owner->__state = MBMS_RESPONSE_DELAY;
	p_owner->__delay = (*pif_act_timer1us)() + p_owner->__interval * 3.5;
}

static uint16_t _evtSending(void *p_client, PifActUartSendData act_send_data)
{
	PifModbusRtuMaster *p_owner = (PifModbusRtuMaster *)p_client;
	uint16_t length, period = 0;

	switch (p_owner->__state) {
	case MBMS_REQUEST:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__buffer + p_owner->index,
    			p_owner->length - p_owner->index);
		p_owner->index += length;
		if (p_owner->index >= p_owner->length) {
			p_owner->__state = MBMS_REQUEST_WAIT;
		}
		period = p_owner->__interval;
		break;

	case MBMS_REQUEST_WAIT:
		period = p_owner->__interval;
		if (pifUart_CheckTxTransfer(p_owner->__p_uart)) {
			p_owner->__state = MBMS_REQUEST_DELAY;
			period *= 2;
		}
		break;

	case MBMS_REQUEST_DELAY:
		if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_RX);
		p_owner->__state = MBMS_RESPONSE;
		break;

	default:
		break;
	}
	return period;
}

static void _evtTimerTimeout(PifIssuerP p_issuer)
{
	PifModbusRtuMaster *p_owner = (PifModbusRtuMaster *)p_issuer;

	p_owner->__state = MBMS_ERROR;
	p_owner->_error = MBE_TIMEOUT;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_WARN, "MB(%u) TxTimeout", p_owner->_id);
#endif
}

static BOOL _requestAndResponse(PifModbusRtuMaster *p_owner, uint16_t len)
{
	p_owner->_error = MBE_NONE;

	if (p_owner->__p_uart->__act_direction) (*p_owner->__p_uart->__act_direction)(UD_TX);

	pifModbus_ShortToStream(pifModbusRtu_CalcCrc(p_owner->__buffer, len), &p_owner->__buffer[len]);

	if (!pifTimer_Start(p_owner->__p_timer, p_owner->__timeout * 1000L / p_owner->__p_timer_manager->_period1us)) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "MB(%u) Not start timer", p_owner->_id);
#endif
	}

	p_owner->length = len + 2;
	p_owner->index = 0;
	p_owner->__state = MBMS_REQUEST;

	while (1) {
		if (p_owner->__state == MBMS_RESPONSE) break;
		else if (p_owner->__state == MBMS_ERROR) goto fail;
		pifTaskManager_YieldUs(p_owner->__interval);
	}

	while (1) {
		if (p_owner->__state == MBMS_RESPONSE_DELAY) {
			if ((int32_t)(p_owner->__delay - (*pif_act_timer1us)()) <= 0) break;
		}
		else if (p_owner->__state == MBMS_ERROR) break;
		pifTaskManager_YieldUs(p_owner->__interval);
	}

fail:
	p_owner->__state = MBMS_IDLE;
	return p_owner->_error == MBE_NONE;
}

BOOL pifModbusRtuMaster_Init(PifModbusRtuMaster *p_owner, PifId id, PifTimerManager *p_timer_manager)
{
    if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusRtuMaster));

    p_owner->__p_timer_manager = p_timer_manager;

    p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__p_timer) goto fail;

    pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerTimeout, p_owner);
    p_owner->__timeout = PIF_MODBUS_MASTER_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifModbusRtuMaster_Clear(p_owner);
	return FALSE;
}

void pifModbusRtuMaster_Clear(PifModbusRtuMaster *p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
}

void pifModbusRtuMaster_SetResponseTimeout(PifModbusRtuMaster *p_owner, uint16_t response_timeout)
{
	p_owner->__timeout = response_timeout;
}

void pifModbusRtuMaster_AttachUart(PifModbusRtuMaster *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
    p_owner->__interval = 1000000L / (p_uart->_baudrate / 10);
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusRtuMaster_DetachUart(PifModbusRtuMaster *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

BOOL pifModbusRtuMaster_ReadCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_READ_COILS;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);

	if (!_requestAndResponse(p_owner, 6)) return FALSE;

	for (i = 0; i < p_owner->__buffer[2]; i++) {
		p_coils[i] = p_owner->__buffer[3 + i];
	}
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadDiscreteInputs(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_READ_DISCRETE_INPUTS;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);

	if (!_requestAndResponse(p_owner, 6)) return FALSE;

	for (i = 0; i < p_owner->__buffer[2]; i++) {
		p_inputs[i] = p_owner->__buffer[3 + i];
	}
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadHoldingRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_READ_HOLDING_REGISTERS;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);

	if (!_requestAndResponse(p_owner, 6)) return FALSE;

	for (i = 0; i < quantity; i++) {
		p_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadInputRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_READ_INPUT_REGISTERS;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);

	if (!_requestAndResponse(p_owner, 6)) return FALSE;

	for (i = 0; i < quantity; i++) {
		p_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}

BOOL pifModbusRtuMaster_WriteSingleCoil(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, BOOL value)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_SINGLE_COIL;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(value ? 0xFF00 : 0x0000, &p_owner->__buffer[4]);

	return _requestAndResponse(p_owner, 6);
}

BOOL pifModbusRtuMaster_WriteSingleRegister(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value)
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_SINGLE_REGISTER;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(value, &p_owner->__buffer[4]);

	return _requestAndResponse(p_owner, 6);
}

BOOL pifModbusRtuMaster_WriteMultipleCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_MULTIPLE_COILS;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);
    p_owner->__buffer[6] = (quantity + 7) / 8;
	pos = 7;
	for (i = 0; i < p_owner->__buffer[6]; i++, pos++) {
		p_owner->__buffer[pos] = p_coils[i];
	}

	return _requestAndResponse(p_owner, pos);
}

BOOL pifModbusRtuMaster_WriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers)
{
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_MULTIPLE_REGISTERS;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);
	p_owner->__buffer[6] = 2 * quantity;
	pos = 7;
	for (i = 0; i < quantity; i++, pos += 2) {
		pifModbus_ShortToStream(p_registers[i], &p_owner->__buffer[pos]);
	}

	return _requestAndResponse(p_owner, pos);
}

BOOL pifModbusRtuMaster_ReadWriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers)
{
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
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_READ_WRITE_MULTIPLE_REGISTERS;
	pifModbus_ShortToStream(read_address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(read_quantity, &p_owner->__buffer[4]);
	pifModbus_ShortToStream(write_address, &p_owner->__buffer[6]);
	pifModbus_ShortToStream(write_quantity, &p_owner->__buffer[8]);
	p_owner->__buffer[10] = 2 * write_quantity;
	pos = 11;
	for (i = 0; i < write_quantity; i++, pos += 2) {
		pifModbus_ShortToStream(p_write_registers[i], &p_owner->__buffer[pos]);
	}

	if (!_requestAndResponse(p_owner, pos)) return FALSE;

	for (i = 0; i < read_quantity; i++) {
		p_read_registers[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
	}
	return TRUE;
}
