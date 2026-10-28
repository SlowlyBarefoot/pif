// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_task_manager.h"
#include "protocol/pif_modbus_ascii_master.h"


/**
 * @brief Sets the state and records it for pifCollectSignal.
 * @param p_owner Pointer to the protocol instance.
 * @param state New state.
 */
static void _setState(PifModbusAsciiMaster *p_owner, PifModbusMasterState state)
{
	p_owner->__state = state;
#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER
	pifCollectSignal_Put(&p_owner->__cs[MBAM_CSF_STATE_IDX], state);
#endif
}

/**
 * @brief Gives the request up when its timeout has expired. Called from every path that runs on
 *        its own, the UART callbacks and pifModbusAsciiMaster_Check(), so no timer is needed.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _checkTimeout(PifModbusAsciiMaster *p_owner)
{
	if (!pifTaskTimeout_IsExpired(&p_owner->__timeout)) return;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_WARN, "MB(%u) Timeout State:%u", p_owner->_id, p_owner->__state);
#endif
	_setState(p_owner, MBMS_ERROR);
	p_owner->_error = MBE_TIMEOUT;
	p_owner->__rx_state = MBRS_IDLE;
}

/**
 * @brief Parses an incoming protocol packet and updates parser state and outputs. A complete frame
 *        is left in the buffer as bytes, its length without the LRC in length.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return None.
 */
static void _parsingPacket(PifModbusAsciiMaster *p_owner, PifActTransportReceive act_receive_data)
{
	uint8_t data;
	int16_t length;

	while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1)) {
		// The wait runs from byte to byte, so a long response at a low baud rate is not cut short.
		pifTaskTimeout_Start(&p_owner->__timeout, p_owner->__timeout_ms * 1000UL);

		// A colon always starts a frame, whatever came before it.
		if (data == ':') {
			p_owner->__rx_state = MBRS_DATA;
			p_owner->index = 0;
			continue;
		}

		switch (p_owner->__rx_state) {
		case MBRS_DATA:
			if (data == ASCII_CR) {
			}
			else if (data == ASCII_LF) {
				length = pifModbusAscii_Decode(p_owner->__buffer, p_owner->index);
				if (length < 3) {
					p_owner->_error = MBE_INVALID_RESPONSE;
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_BAD_FRAME ") Len:%u", p_owner->_id,
							(unsigned int)p_owner->index);
#endif
					goto fail;
				}
				length--;
				if (pifModbusAscii_CalcLrc(p_owner->__buffer, length) != p_owner->__buffer[length]) {
					p_owner->_error = MBE_CRC;
#ifndef PIF_NO_LOG
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_WRONG_CRC ") %X!=%X", p_owner->_id,
							(unsigned int)pifModbusAscii_CalcLrc(p_owner->__buffer, length), (unsigned int)p_owner->__buffer[length]);
#endif
					goto fail;
				}
				p_owner->length = length;
				p_owner->__rx_state = MBRS_FINISH;
				return;
			}
			else if (p_owner->index < sizeof(p_owner->__buffer)) {
				p_owner->__buffer[p_owner->index] = data;
				p_owner->index++;
			}
			else {
				p_owner->_error = MBE_INVALID_RESPONSE;
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_BAD_FRAME ") Overflow", p_owner->_id);
#endif
				goto fail;
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
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive_data)
{
	PifModbusAsciiMaster *p_owner = (PifModbusAsciiMaster *)p_client;
	uint8_t data;

	// A response that comes in after the timeout is not taken, even before Check() notices it.
	_checkTimeout(p_owner);

	switch (p_owner->__state) {
	case MBMS_REQUEST_WAIT:
	case MBMS_RESPONSE:
		if (!p_owner->__broadcast) break;
		// fall through

	default:
		// Nothing is awaited, so whatever comes in is noise or the tail of a frame already given up on.
		while ((*act_receive_data)(&p_owner->__p_uart->parent, &data, 1));
		return FALSE;
	}

	_parsingPacket(p_owner, act_receive_data);

	switch (p_owner->__rx_state) {
	case MBRS_FINISH:
		p_owner->_error = pifModbus_CheckResponse(p_owner->__request, p_owner->__buffer, p_owner->length,
				p_owner->__result_kind, p_owner->__result_quantity);
		if (p_owner->_error == MBE_INVALID_RESPONSE) {
#ifndef PIF_NO_LOG
			pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_INVALID_RESPONSE ") A:%u F:%u", p_owner->_id,
					(unsigned int)p_owner->__buffer[0],	(unsigned int)p_owner->__buffer[1]);
#endif
		}
		break;

	case MBRS_ERROR:
		break;

	default:
		return FALSE;
	}

	pifTaskTimeout_Stop(&p_owner->__timeout);
	p_owner->__rx_state = MBRS_IDLE;
	_setState(p_owner, MBMS_FINISH);
	return TRUE;
}

/**
 * @brief Moves on once the request is out: to the response, or straight to the end for a broadcast,
 *        which no slave answers.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _sent(PifModbusAsciiMaster *p_owner)
{
	if (p_owner->__broadcast) {
		pifTaskTimeout_Stop(&p_owner->__timeout);
		_setState(p_owner, MBMS_FINISH);
	}
	else {
		_setState(p_owner, MBMS_RESPONSE);
		// The wait for the response starts once the request is out.
		pifTaskTimeout_Start(&p_owner->__timeout, p_owner->__timeout_ms * 1000UL);
	}
}

/**
 * @brief Driver callback that emits pending transmit bytes from the protocol state machine.
 * @param p_client Opaque client pointer provided by the communication driver callback.
 * @param act_send_data Callback used to push outgoing bytes to the underlying driver.
 * @return Number of bytes transmitted during this callback invocation.
 */
static uint16_t _evtSending(void *p_client, PifActTransportSend act_send_data)
{
	PifModbusAsciiMaster *p_owner = (PifModbusAsciiMaster *)p_client;
	uint16_t length, period = 0;

	_checkTimeout(p_owner);

	switch (p_owner->__state) {
	case MBMS_REQUEST:
    	length = (*act_send_data)(&p_owner->__p_uart->parent, p_owner->__buffer + p_owner->index,
    			p_owner->length - p_owner->index);
		p_owner->index += length;
		if (p_owner->index >= p_owner->length) {
			_setState(p_owner, MBMS_REQUEST_WAIT);
		}
		period = 1;
		break;

	case MBMS_REQUEST_WAIT:
		period = 1;
		if (pifUart_CheckTxTransfer(p_owner->__p_uart)) {
			_sent(p_owner);
			period = 0;
		}
		break;

	default:
		break;
	}
	return period;
}


/**
 * @brief Notes what the response of the request about to be sent has to be read out into.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param kind What the request asks the slave for.
 * @param p_result Caller buffer the response is read out into. NULL for MBRK_NONE.
 * @param quantity How many bits or registers were asked for. Unused for MBRK_NONE.
 */
static void _setResult(PifModbusAsciiMaster *p_owner, PifModbusResultKind kind, void *p_result, uint16_t quantity)
{
	p_owner->__result_kind = kind;
	p_owner->__p_result = p_result;
	p_owner->__result_quantity = quantity;
}

/**
 * @brief Reads the response out into the buffer the caller gave the request function. The
 *        response has already been checked to carry exactly what was asked for.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _copyResult(PifModbusAsciiMaster *p_owner)
{
	uint16_t i;

	switch (p_owner->__result_kind) {
	case MBRK_BITS:
		for (i = 0; i < p_owner->__buffer[2]; i++) {
			((PifModbusBitFieldP)p_owner->__p_result)[i] = p_owner->__buffer[3 + i];
		}
		break;

	case MBRK_REGISTERS:
		for (i = 0; i < p_owner->__result_quantity; i++) {
			((uint16_t *)p_owner->__p_result)[i] = pifModbus_StreamToShort(&p_owner->__buffer[3 + i * 2]);
		}
		break;

	default:
		break;
	}
}

/**
 * @brief Checks that a request may be issued now and that it may go to that slave.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param slave Target slave address.
 * @param write TRUE for a request that only writes, which is the only kind that may be broadcast.
 * @return TRUE when the request may go ahead.
 */
static BOOL _canRequest(PifModbusAsciiMaster *p_owner, uint8_t slave, BOOL write)
{
	if (p_owner->__state != MBMS_IDLE || !p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (slave > 247 || (slave == PIF_MODBUS_BROADCAST_ADDRESS && !write)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @brief Checks a quantity against its limit and the range it covers against the address space.
 * @param address Start address.
 * @param quantity Number of bits or registers.
 * @param max Largest quantity the function allows.
 * @return TRUE when it fits.
 */
static BOOL _isValidRange(uint16_t address, uint16_t quantity, uint16_t max)
{
	if (quantity < 1 || quantity > max || (uint32_t)address + quantity > 0x10000UL) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @brief Builds a request frame and hands it to the UART. It does not wait: sending the frame and
 *        receiving the response both need the UART task to run, which cannot happen from inside
 *        this call. pifModbusAsciiMaster_Check() is what carries it the rest of the way.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param len Length of the frame that was built in the buffer, the leading colon included.
 */
static void _request(PifModbusAsciiMaster *p_owner, uint16_t len)
{
	int i;

	p_owner->_error = MBE_NONE;
	for (i = 0; i < (int)sizeof(p_owner->__request); i++) {
		p_owner->__request[i] = pifModbusAscii_AsciiToChar(&p_owner->__buffer[1 + 2 * i]);
	}
	p_owner->__broadcast = p_owner->__request[0] == PIF_MODBUS_BROADCAST_ADDRESS;
	// A frame the last request gave up on halfway must not run into the response of this one.
	p_owner->__rx_state = MBRS_IDLE;

	p_owner->__buffer[0] = ':';
	pifModbusAscii_CharToAscii(pifModbusAscii_CalcLrcOfAscii(&p_owner->__buffer[1], len - 1), &p_owner->__buffer[len]);
	len += 2;
	p_owner->__buffer[len] = ASCII_CR; len++;
	p_owner->__buffer[len] = ASCII_LF; len++;

	// Guards the sending itself: the time the frame takes on the line, and the timeout on top.
	pifTaskTimeout_Start(&p_owner->__timeout, p_owner->__timeout_ms * 1000UL + (uint32_t)len * p_owner->__p_uart->_transfer_time);

	p_owner->length = len;
	p_owner->index = 0;
	_setState(p_owner, MBMS_REQUEST);
	pifTask_SetTrigger(p_owner->__p_uart->_p_tx_task, 0);
}

/**
 * @brief Builds and sends a request that reads bits or registers.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param function Function code.
 * @param slave Target slave address.
 * @param address Start address.
 * @param quantity Number of bits or registers.
 * @param kind MBRK_BITS or MBRK_REGISTERS.
 * @param p_result Caller buffer the response is read out into.
 */
static void _requestRead(PifModbusAsciiMaster *p_owner, uint8_t function, uint8_t slave, uint16_t address,
		uint16_t quantity, PifModbusResultKind kind, void *p_result)
{
	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(function, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(quantity, &p_owner->__buffer[9]);

	_setResult(p_owner, kind, p_result, quantity);
	_request(p_owner, 13);
}

BOOL pifModbusAsciiMaster_Init(PifModbusAsciiMaster *p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusAsciiMaster));

    p_owner->__timeout_ms = PIF_MODBUS_MASTER_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifModbusAsciiMaster_Clear(PifModbusAsciiMaster *p_owner)
{
#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER
	pifModbusAsciiMaster_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, MBAM_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifModbusAsciiMaster_SetResponseTimeout(PifModbusAsciiMaster *p_owner, uint16_t response_timeout)
{
	p_owner->__timeout_ms = response_timeout;
}

void pifModbusAsciiMaster_AttachUart(PifModbusAsciiMaster *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, p_owner, _evtParsing, _evtSending, NULL);
}

void pifModbusAsciiMaster_DetachUart(PifModbusAsciiMaster *p_owner)
{
	pifTransport_DetachClient(&p_owner->__p_uart->parent);
	p_owner->__p_uart = NULL;
}

PifModbusMasterResult pifModbusAsciiMaster_Check(PifModbusAsciiMaster *p_owner)
{
	if (p_owner->__state == MBMS_IDLE) return MBMR_IDLE;

	_checkTimeout(p_owner);

	// Everything before this is the request still going out or the response still coming in.
	if (p_owner->__state != MBMS_FINISH && p_owner->__state != MBMS_ERROR) return MBMR_BUSY;

	if (p_owner->__state == MBMS_FINISH && p_owner->_error == MBE_NONE) _copyResult(p_owner);
	_setState(p_owner, MBMS_IDLE);
	return p_owner->_error == MBE_NONE ? MBMR_DONE : MBMR_ERROR;
}

BOOL pifModbusAsciiMaster_ReadCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_BITS)) return FALSE;

	_requestRead(p_owner, MBF_READ_COILS, slave, address, quantity, MBRK_BITS, p_coils);
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadDiscreteInputs(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_inputs)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_BITS)) return FALSE;

	_requestRead(p_owner, MBF_READ_DISCRETE_INPUTS, slave, address, quantity, MBRK_BITS, p_inputs);
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadHoldingRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;

	_requestRead(p_owner, MBF_READ_HOLDING_REGISTERS, slave, address, quantity, MBRK_REGISTERS, p_registers);
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadInputRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;

	_requestRead(p_owner, MBF_READ_INPUT_REGISTERS, slave, address, quantity, MBRK_REGISTERS, p_registers);
	return TRUE;
}

BOOL pifModbusAsciiMaster_WriteSingleCoil(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, BOOL value)
{
	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;

	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_SINGLE_COIL, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(value ? 0xFF00 : 0x0000, &p_owner->__buffer[9]);

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, 13);
	return TRUE;
}

BOOL pifModbusAsciiMaster_WriteSingleRegister(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t value)
{
	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;

	pifModbusAscii_CharToAscii(slave, &p_owner->__buffer[1]);
	pifModbusAscii_CharToAscii(MBF_WRITE_SINGLE_REGISTER, &p_owner->__buffer[3]);
	pifModbusAscii_ShortToAscii(address, &p_owner->__buffer[5]);
	pifModbusAscii_ShortToAscii(value, &p_owner->__buffer[9]);

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, 13);
	return TRUE;
}

BOOL pifModbusAsciiMaster_WriteMultipleCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	uint8_t length;
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXWRITE_BITS)) return FALSE;

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

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, pos);
	return TRUE;
}

BOOL pifModbusAsciiMaster_WriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	uint8_t length;
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXWRITE_REGISTERS)) return FALSE;

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

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, pos);
	return TRUE;
}

BOOL pifModbusAsciiMaster_ReadWriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint16_t write_address, uint16_t write_quantity, uint16_t *p_write_registers)
{
	uint8_t length;
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(read_address, read_quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;
	if (!_isValidRange(write_address, write_quantity, PIF_MODBUS_MAXRW_WRITE_REGISTERS)) return FALSE;

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

	_setResult(p_owner, MBRK_REGISTERS, p_read_registers, read_quantity);
	_request(p_owner, pos);
	return TRUE;
}

#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER

BOOL pifModbusAsciiMaster_SetCsFlag(PifModbusAsciiMaster *p_owner, uint8_t index, PifModbusAsciiMasterCsFlag flag)
{
	(void)index;
	if (flag & MBAM_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[MBAM_CSF_STATE_IDX], "MAS", p_owner->_id, CSVT_REG, 3,
				p_owner->__state)) return FALSE;
	}
	return TRUE;
}

void pifModbusAsciiMaster_ResetCsFlag(PifModbusAsciiMaster *p_owner, uint8_t index, PifModbusAsciiMasterCsFlag flag)
{
	(void)index;
	if (flag & MBAM_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[MBAM_CSF_STATE_IDX]);
}

#endif	// COLLECT_SIGNAL_MODBUS_ASCII_MASTER
