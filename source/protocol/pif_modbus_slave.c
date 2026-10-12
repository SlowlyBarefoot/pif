// SPDX-License-Identifier: BSD-3-Clause
#include "protocol/pif_modbus_slave.h"


/**
 * @brief Checks a requested range the way the Modbus specification orders it: the quantity first,
 *        then the addresses it covers.
 * @param address Start address.
 * @param quantity Number of bits or registers.
 * @param max Largest quantity the function allows.
 * @param size Number of bits or registers the slave has.
 * @return MBE_NONE, MBE_ILLEGAL_DATA_VALUE or MBE_ILLEGAL_DATA_ADDRESS.
 */
static PifModbusError _checkRange(uint16_t address, uint16_t quantity, uint16_t max, uint16_t size)
{
	if (quantity < 1 || quantity > max) return MBE_ILLEGAL_DATA_VALUE;
	if ((uint32_t)address + quantity > size) return MBE_ILLEGAL_DATA_ADDRESS;
	return MBE_NONE;
}

void pifModbusSlave_AttachDiscreteInput(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadDiscreteInputs evtReadDiscreteInputs)
{
	p_owner->__p_discrete_inputs = p_buffer;
	p_owner->__discrete_inputs_size = size;
	p_owner->__evtReadDiscreteInputs = evtReadDiscreteInputs;
}

void pifModbusSlave_AttachCoils(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadCoils evtReadCoils, PifEvtModbusWriteSingleCoil evtWriteSingleCoil,
		PifEvtModbusWriteMultipleCoils evtWriteMultipleCoils)
{
	p_owner->__p_coils = p_buffer;
	p_owner->__coils_size = size;
	p_owner->__evtReadCoils = evtReadCoils;
	p_owner->__evtWriteSingleCoil = evtWriteSingleCoil;
	p_owner->__evtWriteMultipleCoils = evtWriteMultipleCoils;
}

void pifModbusSlave_AttachInputRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadInputRegisters evtReadInputRegisters)
{
	p_owner->__p_input_registers = p_buffer;
	p_owner->__input_registers_size = size;
	p_owner->__evtReadInputRegisters = evtReadInputRegisters;
}

void pifModbusSlave_AttachHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadHoldingRegisters evtReadHoldingRegisters, PifEvtModbusWriteSingleRegister evtWriteSingleRegister,
		PifEvtModbusWriteMultipleRegisters evtWriteMultipleRegisters)
{
	p_owner->__p_holding_registers = p_buffer;
	p_owner->__holding_registers_size = size;
	p_owner->__evtReadHoldingRegisters = evtReadHoldingRegisters;
	p_owner->__evtWriteSingleRegister = evtWriteSingleRegister;
	p_owner->__evtWriteMultipleRegisters = evtWriteMultipleRegisters;
}

void pifModbusSlave_AttachEvtReadWriteMultipleRegisters(PifModbusSlave *p_owner,
		PifEvtModbusReadWriteMultipleRegisters evtReadWriteMultipleRegisters)
{
	p_owner->__evtReadWriteMultipleRegisters = evtReadWriteMultipleRegisters;
}

BOOL pifModbusSlave_AllocTmpBits(PifModbusSlave *p_owner, uint16_t quantity)
{
	uint8_t *p_tmp;
	uint16_t length = (quantity + 7) / 8;

	if (!p_owner->__p_tmp_bits) {
		p_owner->__p_tmp_bits = malloc(length);
		if (!p_owner->__p_tmp_bits) return FALSE;
		p_owner->__tmp_bits_size = length;
		memset(p_owner->__p_tmp_bits, 0, length);
	}
	else if (p_owner->__tmp_bits_size < length) {
		p_tmp = realloc(p_owner->__p_tmp_bits, length);
		if (p_tmp) {
			p_owner->__p_tmp_bits = p_tmp;
			p_owner->__tmp_bits_size = length;
			memset(p_owner->__p_tmp_bits, 0, length);
		}
		else {
			free(p_owner->__p_tmp_bits);
			p_owner->__p_tmp_bits = NULL;
			p_owner->__tmp_bits_size = 0;
			return FALSE;
		}
	}
	return TRUE;
}

PifModbusError pifModbusSlave_ReadCoils(PifModbusSlave *p_owner, uint16_t *p_quantity)
{
	uint16_t i, address;
	BOOL status;
	PifModbusError exception;

	address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	exception = _checkRange(address, *p_quantity, PIF_MODBUS_MAXREAD_BITS, p_owner->__coils_size);
	if (exception) return exception;

	if (p_owner->__evtReadCoils) {
		(*p_owner->__evtReadCoils)(address, *p_quantity);
	}
	if (!pifModbusSlave_AllocTmpBits(p_owner, *p_quantity)) return MBE_SERVER_DEVICE_FAILURE;

	for (i = 0; i < *p_quantity; i++) {
		status = PIF_MODBUS_READ_BIT_FIELD(p_owner->__p_coils, address + i);
		PIF_MODBUS_WRITE_BIT_FIELD(p_owner->__p_tmp_bits, i, status);
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_ReadDiscreteInputs(PifModbusSlave *p_owner, uint16_t *p_quantity)
{
	uint16_t i, address;
	BOOL status;
	PifModbusError exception;

	address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	exception = _checkRange(address, *p_quantity, PIF_MODBUS_MAXREAD_BITS, p_owner->__discrete_inputs_size);
	if (exception) return exception;

	if (p_owner->__evtReadDiscreteInputs) {
		(*p_owner->__evtReadDiscreteInputs)(address, *p_quantity);
	}
	if (!pifModbusSlave_AllocTmpBits(p_owner, *p_quantity)) return MBE_SERVER_DEVICE_FAILURE;

	for (i = 0; i < *p_quantity; i++) {
		status = PIF_MODBUS_READ_BIT_FIELD(p_owner->__p_discrete_inputs, address + i);
		PIF_MODBUS_WRITE_BIT_FIELD(p_owner->__p_tmp_bits, i, status);
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_ReadHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity)
{
	PifModbusError exception;

	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	exception = _checkRange(*p_address, *p_quantity, PIF_MODBUS_MAXREAD_REGISTERS, p_owner->__holding_registers_size);
	if (exception) return exception;

	if (p_owner->__evtReadHoldingRegisters) {
		(*p_owner->__evtReadHoldingRegisters)(*p_address, *p_quantity);
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_ReadInputRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity)
{
	PifModbusError exception;

	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	exception = _checkRange(*p_address, *p_quantity, PIF_MODBUS_MAXREAD_REGISTERS, p_owner->__input_registers_size);
	if (exception) return exception;

	if (p_owner->__evtReadInputRegisters) {
		(*p_owner->__evtReadInputRegisters)(*p_address, *p_quantity);
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_WriteSingleCoil(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value)
{
	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_value = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	if (*p_value != 0xFF00 && *p_value != 0x0000) return MBE_ILLEGAL_DATA_VALUE;
	if (*p_address >= p_owner->__coils_size) return MBE_ILLEGAL_DATA_ADDRESS;

	PIF_MODBUS_WRITE_BIT_FIELD(p_owner->__p_coils, *p_address, *p_value);
	if (p_owner->__evtWriteSingleCoil) {
		if (!(*p_owner->__evtWriteSingleCoil)(*p_address)) return MBE_SERVER_DEVICE_FAILURE;
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_WriteSingleRegister(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value)
{
	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	if (*p_address >= p_owner->__holding_registers_size) return MBE_ILLEGAL_DATA_ADDRESS;

	*p_value = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	p_owner->__p_holding_registers[*p_address] = *p_value;
	if (p_owner->__evtWriteSingleRegister) {
		if (!(*p_owner->__evtWriteSingleRegister)(*p_address, *p_value)) return MBE_SERVER_DEVICE_FAILURE;
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_WriteMultipleCoils(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity)
{
	uint16_t i;
	BOOL status;
	PifModbusError exception;

	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	if (p_owner->__p_buffer[6] != (*p_quantity + 7) / 8) return MBE_ILLEGAL_DATA_VALUE;
	exception = _checkRange(*p_address, *p_quantity, PIF_MODBUS_MAXWRITE_BITS, p_owner->__coils_size);
	if (exception) return exception;

	for (i = 0; i < *p_quantity; i++) {
		status = PIF_MODBUS_READ_BIT_FIELD(&p_owner->__p_buffer[7], i);
		PIF_MODBUS_WRITE_BIT_FIELD(p_owner->__p_coils, *p_address + i, status);
	}
	if (p_owner->__evtWriteMultipleCoils) {
		if (!(*p_owner->__evtWriteMultipleCoils)(*p_address, *p_quantity)) return MBE_SERVER_DEVICE_FAILURE;
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_WriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity)
{
	uint16_t i, pos;
	PifModbusError exception;

	*p_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	if (p_owner->__p_buffer[6] != 2 * *p_quantity) return MBE_ILLEGAL_DATA_VALUE;
	exception = _checkRange(*p_address, *p_quantity, PIF_MODBUS_MAXWRITE_REGISTERS, p_owner->__holding_registers_size);
	if (exception) return exception;

	pos = 7;
	for (i = 0; i < *p_quantity; i++, pos += 2) {
		p_owner->__p_holding_registers[*p_address + i] = pifModbus_StreamToShort(&p_owner->__p_buffer[pos]);
	}
	if (p_owner->__evtWriteMultipleRegisters) {
		if (!(*p_owner->__evtWriteMultipleRegisters)(*p_address, *p_quantity)) return MBE_SERVER_DEVICE_FAILURE;
	}
	return MBE_NONE;
}

PifModbusError pifModbusSlave_ReadWriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_read_address, uint16_t *p_read_quantity)
{
	uint16_t i, pos, write_address, write_quantity;
	PifModbusError exception;

	*p_read_address = pifModbus_StreamToShort(&p_owner->__p_buffer[2]);
	*p_read_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[4]);
	write_address = pifModbus_StreamToShort(&p_owner->__p_buffer[6]);
	write_quantity = pifModbus_StreamToShort(&p_owner->__p_buffer[8]);
	if (p_owner->__p_buffer[10] != 2 * write_quantity) return MBE_ILLEGAL_DATA_VALUE;
	exception = _checkRange(*p_read_address, *p_read_quantity, PIF_MODBUS_MAXREAD_REGISTERS, p_owner->__holding_registers_size);
	if (exception) return exception;
	exception = _checkRange(write_address, write_quantity, PIF_MODBUS_MAXRW_WRITE_REGISTERS, p_owner->__holding_registers_size);
	if (exception) return exception;

	// The write is carried out before the read, so the read sees what was just written.
	pos = 11;
	for (i = 0; i < write_quantity; i++, pos += 2) {
		p_owner->__p_holding_registers[write_address + i] = pifModbus_StreamToShort(&p_owner->__p_buffer[pos]);
	}
	if (p_owner->__evtReadWriteMultipleRegisters) {
		if (!(*p_owner->__evtReadWriteMultipleRegisters)(*p_read_address, *p_read_quantity, write_address, write_quantity)) return MBE_SERVER_DEVICE_FAILURE;
	}
	return MBE_NONE;
}
