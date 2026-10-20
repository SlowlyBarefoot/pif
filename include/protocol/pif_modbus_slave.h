// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus_slave.h
 * @brief Common part of the Modbus RTU and ASCII slaves: the data areas and the handling of each function.
 *
 * The application attaches its coils, discrete inputs, input registers and holding registers. The slave reads
 * and writes them directly, and the events let the application refresh a value before it is read or act on a
 * value after it is written.
 *
 * A slave handles only the data areas, and the functions on them, that these options in pif_conf.h put in the
 * build. A request for any other function is answered with exception 1, as for a function the slave lacks.
 * - PIF_USE_MODBUS_SLAVE_COILS: Read Coils (1), Write Single Coil (5) and Write Multiple Coils (15)
 * - PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS: Read Discrete Inputs (2)
 * - PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS: Read Input Registers (4)
 * - PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS: Read Holding Registers (3), Write Single Register (6) and Write
 *   Multiple Registers (16)
 * - PIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE: Read/Write Multiple Registers (23), on the holding registers, which
 *   it puts in as well
 */
#ifndef PIF_MODBUS_SLAVE_H
#define PIF_MODBUS_SLAVE_H


#include "protocol/pif_modbus.h"


#ifdef PIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE
	#ifndef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS
		#define PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS
	#endif
#endif

// What the RTU and ASCII slaves build their responses from, by the data areas put in.
#if defined(PIF_USE_MODBUS_SLAVE_COILS) || defined(PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS)
	#define PIF_MODBUS_SLAVE_READ_BITS			///< A function reads bits
#endif
#if defined(PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS) || defined(PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS)
	#define PIF_MODBUS_SLAVE_READ_REGISTERS		///< A function reads registers
#endif
#if defined(PIF_USE_MODBUS_SLAVE_COILS) || defined(PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS)
	#define PIF_MODBUS_SLAVE_WRITE				///< A function writes coils or registers
#endif


/**
 * @brief Send state of a slave.
 */
typedef enum EnPifModbusSlaveState
{
	MBSS_IDLE			= 0,	///< Waiting for a request
	MBSS_PRE_DELAY		= 1,	///< Waiting before the response is sent
	MBSS_SEND			= 2	///< Sending the response
} PifModbusSlaveState;


/**
 * @brief Called before coils are read, to refresh them.
 */
typedef void (*PifEvtModbusReadCoils)(uint16_t address, uint16_t quantity);
/**
 * @brief Called before discrete inputs are read, to refresh them.
 */
typedef void (*PifEvtModbusReadDiscreteInputs)(uint16_t address, uint16_t quantity);
/**
 * @brief Called before holding registers are read, to refresh them.
 */
typedef void (*PifEvtModbusReadHoldingRegisters)(uint16_t address, uint16_t quantity);
/**
 * @brief Called before input registers are read, to refresh them.
 */
typedef void (*PifEvtModbusReadInputRegisters)(uint16_t address, uint16_t quantity);
/**
 * @brief Called after a coil was written.
 *
 * Returns FALSE to answer with a server device failure.
 */
typedef BOOL (*PifEvtModbusWriteSingleCoil)(uint16_t address);
/**
 * @brief Called after a holding register was written with value.
 *
 * Returns FALSE to answer with a server device failure.
 */
typedef BOOL (*PifEvtModbusWriteSingleRegister)(uint16_t address, uint16_t value);
/**
 * @brief Called after coils were written.
 *
 * Returns FALSE to answer with a server device failure.
 */
typedef BOOL (*PifEvtModbusWriteMultipleCoils)(uint16_t address, uint16_t quantity);
/**
 * @brief Called after holding registers were written.
 *
 * Returns FALSE to answer with a server device failure.
 */
typedef BOOL (*PifEvtModbusWriteMultipleRegisters)(uint16_t address, uint16_t quantity);
/**
 * @brief Called after the write part of a read/write request was stored, before the read part is taken.
 *
 * Returns FALSE to answer with a server device failure.
 */
typedef BOOL (*PifEvtModbusReadWriteMultipleRegisters)(uint16_t read_address, uint16_t read_quantity, uint16_t write_address, uint16_t write_quantity);

/**
 * @class StPifModbusSlave
 * @brief Data areas and events of a Modbus slave.
 */
typedef struct StPifModbusSlave
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t _my_addr;	///< Own slave address

	// Private Member Variable
	uint8_t *__p_buffer;
	PifModbusSlaveState __state;
#ifdef PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS
	uint8_t *__p_discrete_inputs;
	uint16_t __discrete_inputs_size;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_COILS
	uint8_t *__p_coils;
	uint16_t __coils_size;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS
	uint16_t *__p_input_registers;
	uint16_t __input_registers_size;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS
	uint16_t *__p_holding_registers;
	uint16_t __holding_registers_size;
#endif
#ifdef PIF_MODBUS_SLAVE_READ_BITS
	uint8_t *__p_tmp_bits;
	uint16_t __tmp_bits_size;
#endif

	/// Private Event Variable
#ifdef PIF_USE_MODBUS_SLAVE_COILS
	PifEvtModbusReadCoils __evtReadCoils;
	PifEvtModbusWriteSingleCoil __evtWriteSingleCoil;
	PifEvtModbusWriteMultipleCoils __evtWriteMultipleCoils;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS
	PifEvtModbusReadDiscreteInputs __evtReadDiscreteInputs;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS
	PifEvtModbusReadInputRegisters __evtReadInputRegisters;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS
	PifEvtModbusReadHoldingRegisters __evtReadHoldingRegisters;
	PifEvtModbusWriteSingleRegister __evtWriteSingleRegister;
	PifEvtModbusWriteMultipleRegisters __evtWriteMultipleRegisters;
#endif
#ifdef PIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE
	PifEvtModbusReadWriteMultipleRegisters __evtReadWriteMultipleRegisters;
#endif
} PifModbusSlave;


#ifdef __cplusplus
extern "C" {
#endif

#ifdef PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS

/**
 * @brief Gives the slave the discrete inputs the master reads, 8 per byte.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Data the master reads and writes.
 * @param size Number of bits or registers the buffer holds.
 * @param evtReadDiscreteInputs Called before inputs are read, to refresh them. May be NULL.
 */
void pifModbusSlave_AttachDiscreteInput(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadDiscreteInputs evtReadDiscreteInputs);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_COILS

/**
 * @brief Gives the slave the coils the master reads and writes, 8 per byte.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Data the master reads and writes.
 * @param size Number of bits or registers the buffer holds.
 * @param evtReadCoils Called before coils are read, to refresh them. May be NULL.
 * @param evtWriteSingleCoil Called after a coil was written. May be NULL.
 * @param evtWriteMultipleCoils Called after coils were written. May be NULL.
 */
void pifModbusSlave_AttachCoils(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadCoils evtReadCoils, PifEvtModbusWriteSingleCoil evtWriteSingleCoil,
		PifEvtModbusWriteMultipleCoils evtWriteMultipleCoils);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS

/**
 * @brief Gives the slave the input registers the master reads.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Data the master reads and writes.
 * @param size Number of bits or registers the buffer holds.
 * @param evtReadInputRegisters Called before registers are read, to refresh them. May be NULL.
 */
void pifModbusSlave_AttachInputRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadInputRegisters evtReadInputRegisters);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS

/**
 * @brief Gives the slave the holding registers the master reads and writes.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Data the master reads and writes.
 * @param size Number of bits or registers the buffer holds.
 * @param evtReadHoldingRegisters Called before registers are read, to refresh them. May be NULL.
 * @param evtWriteSingleRegister Called after a register was written. May be NULL.
 * @param evtWriteMultipleRegisters Called after registers were written. May be NULL.
 */
void pifModbusSlave_AttachHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadHoldingRegisters evtReadHoldingRegisters, PifEvtModbusWriteSingleRegister evtWriteSingleRegister,
		PifEvtModbusWriteMultipleRegisters evtWriteMultipleRegisters);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE

/**
 * @brief Attaches the callback a Read/Write Multiple Registers request (function 23) calls once it
 *        has written the holding registers. It works on the buffer given to
 *        pifModbusSlave_AttachHoldingRegisters().
 * @param p_owner Pointer to the protocol instance.
 * @param evtReadWriteMultipleRegisters Callback, or NULL. Returning FALSE answers with exception 4.
 */
void pifModbusSlave_AttachEvtReadWriteMultipleRegisters(PifModbusSlave *p_owner,
		PifEvtModbusReadWriteMultipleRegisters evtReadWriteMultipleRegisters);

#endif

#ifdef PIF_MODBUS_SLAVE_READ_BITS

/**
 * @brief Makes sure the buffer for bit responses holds quantity bits. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param quantity Number of coils or registers to process.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusSlave_AllocTmpBits(PifModbusSlave *p_owner, uint16_t quantity);

/**
 * @brief Frees the buffer for bit responses. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusSlave_FreeTmpBits(PifModbusSlave *p_owner);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_COILS

/**
 * @brief Handles the Read Coils request in the receive buffer: checks the range, calls the read event and
 *        gathers the coils for the response. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadCoils(PifModbusSlave *p_owner, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_DISCRETE_INPUTS

/**
 * @brief Handles the Read Discrete Inputs request in the receive buffer: checks the range, calls the read event
 *        and gathers the inputs for the response. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadDiscreteInputs(PifModbusSlave *p_owner, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS

/**
 * @brief Handles the Read Holding Registers request in the receive buffer: checks the range and calls the read
 *        event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the start address of the request.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_INPUT_REGISTERS

/**
 * @brief Handles the Read Input Registers request in the receive buffer: checks the range and calls the read
 *        event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the start address of the request.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadInputRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_COILS

/**
 * @brief Handles the Write Single Coil request in the receive buffer: checks it, writes the coil and calls the
 *        write event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the address of the coil.
 * @param p_value The value as received, 0xFF00 or 0x0000, which the response echoes.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteSingleCoil(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS

/**
 * @brief Handles the Write Single Register request in the receive buffer: checks it, writes the register and
 *        calls the write event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the address of the register.
 * @param p_value Receives the value written, which the response echoes.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteSingleRegister(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_COILS

/**
 * @brief Handles the Write Multiple Coils request in the receive buffer: checks it, writes the coils and calls
 *        the write event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the start address of the request.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteMultipleCoils(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_HOLDING_REGISTERS

/**
 * @brief Handles the Write Multiple Registers request in the receive buffer: checks it, writes the registers and
 *        calls the write event. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Receives the start address of the request.
 * @param p_quantity Receives the quantity of the request.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

#endif

#ifdef PIF_USE_MODBUS_SLAVE_READ_WRITE_MULTIPLE

/**
 * @brief Handles the Read/Write Multiple Registers request in the receive buffer: checks it, writes the
 *        registers, calls the event and leaves the read part for the response. Used by the RTU and ASCII slaves.
 * @param p_owner Pointer to the protocol instance.
 * @param p_read_address Receives the start address of the read part.
 * @param p_read_quantity Receives the quantity of the read part.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadWriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_read_address, uint16_t *p_read_quantity);

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_SLAVE_H
