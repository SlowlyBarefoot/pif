// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_task_manager.h"
#include "protocol/pif_modbus_rtu_master.h"


/**
 * @brief Sets the state and records it for pifCollectSignal.
 * @param p_owner Pointer to the protocol instance.
 * @param state New state.
 */
static void _setState(PifModbusRtuMaster *p_owner, PifModbusMasterState state)
{
	p_owner->__state = state;
#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER
	pifCollectSignal_Put(&p_owner->__cs[MBRM_CSF_STATE_IDX], state);
#endif
}

/**
 * @brief Starts the silent interval of 3.5 characters the line has to keep before the next request.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _startResponseDelay(PifModbusRtuMaster *p_owner)
{
	_setState(p_owner, MBMS_RESPONSE_DELAY);
	p_owner->__delay = (*pif_act_timer1us)() + p_owner->__p_uart->_transfer_time * 7 / 2;
}

/**
 * @brief Gives the request up when its timeout has expired. Called from every path that runs on
 *        its own, the UART callbacks and pifModbusRtuMaster_Check(), so no timer is needed.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 */
static void _checkTimeout(PifModbusRtuMaster *p_owner)
{
	if (!pifModbus_IsExpired(&p_owner->__wait)) return;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_WARN, "MB(%u) Timeout State:%u", p_owner->_id, p_owner->__state);
#endif
	_setState(p_owner, MBMS_ERROR);
	p_owner->_error = MBE_TIMEOUT;
	p_owner->__rx_state = MBRS_IDLE;
}

/**
 * @brief Parses an incoming protocol packet and updates parser state and outputs.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param act_receive_data Callback used to pull incoming bytes from the underlying driver.
 * @return None.
 */
static void _parsingPacket(PifModbusRtuMaster *p_owner, PifActUartReceiveData act_receive_data)
{
	uint8_t data;
	uint16_t crc[2];

	while ((*act_receive_data)(p_owner->__p_uart, &data, 1)) {
		// The wait runs from byte to byte, so a long response at a low baud rate is not cut short.
		pifModbus_StartTimeout(&p_owner->__wait, p_owner->__timeout * 1000UL);

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
					pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_UNKNOWN_FUNCTION ") A:%u F:%u", p_owner->_id,
							(unsigned int)p_owner->__buffer[0],	(unsigned int)p_owner->__buffer[1]);
#endif
					goto fail;
				}
			}
			break;

		case MBRS_LENGTH:
			// At most 2 + 1 + 255 + 2 bytes, which is what the buffer holds.
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
		            return;
				}
				p_owner->_error = MBE_CRC;
#ifndef PIF_NO_LOG
				pifLog_Printf(LT_ERROR, "MB(%u) ParsingPacket(" PIF_LOG_MSG_WRONG_CRC ") %u!=%u", p_owner->_id,
						(unsigned int)crc[0], (unsigned int)crc[1]);
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
static BOOL _evtParsing(void *p_client, PifActUartReceiveData act_receive_data)
{
	PifModbusRtuMaster *p_owner = (PifModbusRtuMaster *)p_client;
	uint8_t data;

	// A response that comes in after the timeout is not taken, even before Check() notices it.
	_checkTimeout(p_owner);

	switch (p_owner->__state) {
	case MBMS_REQUEST_WAIT:
	case MBMS_REQUEST_DELAY:
	case MBMS_RESPONSE:
		if (!p_owner->__broadcast) break;
		// fall through

	default:
		// Nothing is awaited, so whatever comes in is noise or the tail of a frame already given up on.
		while ((*act_receive_data)(p_owner->__p_uart, &data, 1));
		return FALSE;
	}

	_parsingPacket(p_owner, act_receive_data);

	switch (p_owner->__rx_state) {
	case MBRS_FINISH:
		p_owner->_error = pifModbus_CheckResponse(p_owner->__request, p_owner->__buffer, p_owner->index - 2,
				p_owner->__result_kind, p_owner->__result_quantity);
		break;

	case MBRS_ERROR:
		break;

	default:
		return FALSE;
	}

	pifModbus_StopTimeout(&p_owner->__wait);
	pifUart_SetDirection(p_owner->__p_uart, UD_TX);
	p_owner->__rx_state = MBRS_IDLE;
	_startResponseDelay(p_owner);
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
	PifModbusRtuMaster *p_owner = (PifModbusRtuMaster *)p_client;
	uint16_t length, period = 0;

	_checkTimeout(p_owner);

	switch (p_owner->__state) {
	case MBMS_REQUEST:
    	length = (*act_send_data)(p_owner->__p_uart, p_owner->__buffer + p_owner->index,
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
			_setState(p_owner, MBMS_REQUEST_DELAY);
			period *= 2;
		}
		break;

	case MBMS_REQUEST_DELAY:
		if (p_owner->__broadcast) {
			// No slave answers a broadcast, so it is over once it is out.
			pifModbus_StopTimeout(&p_owner->__wait);
			_startResponseDelay(p_owner);
		}
		else {
			pifUart_SetDirection(p_owner->__p_uart, UD_RX);
			_setState(p_owner, MBMS_RESPONSE);
			// The wait for the response starts once the request is out.
			pifModbus_StartTimeout(&p_owner->__wait, p_owner->__timeout * 1000UL);
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
static void _setResult(PifModbusRtuMaster *p_owner, PifModbusResultKind kind, void *p_result, uint16_t quantity)
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
static void _copyResult(PifModbusRtuMaster *p_owner)
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
static BOOL _canRequest(PifModbusRtuMaster *p_owner, uint8_t slave, BOOL write)
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
 *        this call. pifModbusRtuMaster_Check() is what carries it the rest of the way.
 * @param p_owner Pointer to the protocol instance that owns this operation.
 * @param len Length of the frame that was built in the buffer.
 */
static void _request(PifModbusRtuMaster *p_owner, uint16_t len)
{
	p_owner->_error = MBE_NONE;
	memcpy(p_owner->__request, p_owner->__buffer, sizeof(p_owner->__request));
	p_owner->__broadcast = p_owner->__buffer[0] == PIF_MODBUS_BROADCAST_ADDRESS;
	// A frame the last request gave up on halfway must not run into the response of this one.
	p_owner->__rx_state = MBRS_IDLE;

	pifUart_SetDirection(p_owner->__p_uart, UD_TX);

	pifModbus_ShortToStream(pifModbusRtu_CalcCrc(p_owner->__buffer, len), &p_owner->__buffer[len]);

	// Guards the sending itself: the time the frame takes on the line, and the timeout on top.
	pifModbus_StartTimeout(&p_owner->__wait, p_owner->__timeout * 1000UL + (uint32_t)(len + 2) * p_owner->__p_uart->_transfer_time);

	p_owner->length = len + 2;
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
static void _requestRead(PifModbusRtuMaster *p_owner, uint8_t function, uint8_t slave, uint16_t address,
		uint16_t quantity, PifModbusResultKind kind, void *p_result)
{
	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = function;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);

	_setResult(p_owner, kind, p_result, quantity);
	_request(p_owner, 6);
}

BOOL pifModbusRtuMaster_Init(PifModbusRtuMaster *p_owner, PifId id)
{
    if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifModbusRtuMaster));

    p_owner->__timeout = PIF_MODBUS_MASTER_TIMEOUT;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifModbusRtuMaster_Clear(PifModbusRtuMaster *p_owner)
{
#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER
	pifModbusRtuMaster_ResetCsFlag(p_owner, MBRM_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifModbusRtuMaster_SetResponseTimeout(PifModbusRtuMaster *p_owner, uint16_t response_timeout)
{
	p_owner->__timeout = response_timeout;
}

void pifModbusRtuMaster_AttachUart(PifModbusRtuMaster *p_owner, PifUart *p_uart)
{
	p_owner->__p_uart = p_uart;
	pifUart_AttachClient(p_uart, p_owner, _evtParsing, _evtSending);
}

void pifModbusRtuMaster_DetachUart(PifModbusRtuMaster *p_owner)
{
	pifUart_DetachClient(p_owner->__p_uart);
	p_owner->__p_uart = NULL;
}

PifModbusMasterResult pifModbusRtuMaster_Check(PifModbusRtuMaster *p_owner)
{
	if (p_owner->__state == MBMS_IDLE) return MBMR_IDLE;

	_checkTimeout(p_owner);
	if (p_owner->__state == MBMS_ERROR) {
		_setState(p_owner, MBMS_IDLE);
		return MBMR_ERROR;
	}

	// Everything before this is the request still going out or the response still coming in.
	if (p_owner->__state != MBMS_RESPONSE_DELAY) return MBMR_BUSY;

	// The response is in, and the silent interval after it has to pass before the line may carry
	// the next request.
	if ((int32_t)(p_owner->__delay - (*pif_act_timer1us)()) > 0) return MBMR_BUSY;

	if (p_owner->_error == MBE_NONE) _copyResult(p_owner);
	_setState(p_owner, MBMS_IDLE);
	return p_owner->_error == MBE_NONE ? MBMR_DONE : MBMR_ERROR;
}

BOOL pifModbusRtuMaster_ReadCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_BITS)) return FALSE;

	_requestRead(p_owner, MBF_READ_COILS, slave, address, quantity, MBRK_BITS, p_coils);
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadDiscreteInputs(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_inputs)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_BITS)) return FALSE;

	_requestRead(p_owner, MBF_READ_DISCRETE_INPUTS, slave, address, quantity, MBRK_BITS, p_inputs);
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadHoldingRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;

	_requestRead(p_owner, MBF_READ_HOLDING_REGISTERS, slave, address, quantity, MBRK_REGISTERS, p_registers);
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadInputRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;

	_requestRead(p_owner, MBF_READ_INPUT_REGISTERS, slave, address, quantity, MBRK_REGISTERS, p_registers);
	return TRUE;
}

BOOL pifModbusRtuMaster_WriteSingleCoil(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, BOOL value)
{
	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;

	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_SINGLE_COIL;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(value ? 0xFF00 : 0x0000, &p_owner->__buffer[4]);

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, 6);
	return TRUE;
}

BOOL pifModbusRtuMaster_WriteSingleRegister(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t value)
{
	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;

	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_SINGLE_REGISTER;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(value, &p_owner->__buffer[4]);

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, 6);
	return TRUE;
}

BOOL pifModbusRtuMaster_WriteMultipleCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils)
{
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXWRITE_BITS)) return FALSE;

	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_MULTIPLE_COILS;
    pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
    pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);
    p_owner->__buffer[6] = (quantity + 7) / 8;
	pos = 7;
	for (i = 0; i < p_owner->__buffer[6]; i++, pos++) {
		p_owner->__buffer[pos] = p_coils[i];
	}

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, pos);
	return TRUE;
}

BOOL pifModbusRtuMaster_WriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers)
{
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, TRUE)) return FALSE;
	if (!_isValidRange(address, quantity, PIF_MODBUS_MAXWRITE_REGISTERS)) return FALSE;

	p_owner->__buffer[0] = slave;
	p_owner->__buffer[1] = MBF_WRITE_MULTIPLE_REGISTERS;
	pifModbus_ShortToStream(address, &p_owner->__buffer[2]);
	pifModbus_ShortToStream(quantity, &p_owner->__buffer[4]);
	p_owner->__buffer[6] = 2 * quantity;
	pos = 7;
	for (i = 0; i < quantity; i++, pos += 2) {
		pifModbus_ShortToStream(p_registers[i], &p_owner->__buffer[pos]);
	}

	_setResult(p_owner, MBRK_NONE, NULL, 0);
	_request(p_owner, pos);
	return TRUE;
}

BOOL pifModbusRtuMaster_ReadWriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint16_t write_address, uint16_t write_quantity, uint16_t *p_write_registers)
{
	uint16_t i, pos;

	if (!_canRequest(p_owner, slave, FALSE)) return FALSE;
	if (!_isValidRange(read_address, read_quantity, PIF_MODBUS_MAXREAD_REGISTERS)) return FALSE;
	if (!_isValidRange(write_address, write_quantity, PIF_MODBUS_MAXRW_WRITE_REGISTERS)) return FALSE;

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

	_setResult(p_owner, MBRK_REGISTERS, p_read_registers, read_quantity);
	_request(p_owner, pos);
	return TRUE;
}

#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER

BOOL pifModbusRtuMaster_SetCsFlag(PifModbusRtuMaster *p_owner, PifModbusRtuMasterCsFlag flag)
{
	if (flag & MBRM_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[MBRM_CSF_STATE_IDX], "MRS", p_owner->_id, CSVT_REG, 3,
				p_owner->__state)) return FALSE;
	}
	return TRUE;
}

void pifModbusRtuMaster_ResetCsFlag(PifModbusRtuMaster *p_owner, PifModbusRtuMasterCsFlag flag)
{
	if (flag & MBRM_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[MBRM_CSF_STATE_IDX]);
}

#endif	// COLLECT_SIGNAL_MODBUS_RTU_MASTER
