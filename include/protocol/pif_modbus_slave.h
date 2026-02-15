#ifndef PIF_MODBUS_SLAVE_H
#define PIF_MODBUS_SLAVE_H


#include "protocol/pif_modbus.h"


typedef enum EnPifModbusSlaveState
{
	MBSS_IDLE			= 0,
	MBSS_PRE_DELAY		= 1,
	MBSS_SEND			= 2,
	MBSS_WAIT			= 3,
	MBSS_POST_DELAY		= 4
} PifModbusSlaveState;


typedef void (*PifEvtModbusReadCoils)(uint16_t address, uint16_t quantity);
typedef void (*PifEvtModbusReadDiscreteInputs)(uint16_t address, uint16_t quantity);
typedef void (*PifEvtModbusReadHoldingRegisters)(uint16_t address, uint16_t quantity);
typedef void (*PifEvtModbusReadInputRegisters)(uint16_t address, uint16_t quantity);
typedef BOOL (*PifEvtModbusWriteSingleCoil)(uint16_t address);
typedef BOOL (*PifEvtModbusWriteSingleRegister)(uint16_t address, uint16_t value);
typedef BOOL (*PifEvtModbusWriteMultipleCoils)(uint16_t address, uint16_t quantity);
typedef BOOL (*PifEvtModbusWriteMultipleRegisters)(uint16_t address, uint16_t quantity);
typedef BOOL (*PifEvtModbusReadWriteMultipleRegisters)(uint16_t read_address, uint16_t read_quantity, uint16_t write_address, uint16_t write_quantity);

/**
 * @class StPifModbusSlave
 * @brief Represents the StPifModbusSlave data structure.
 */
typedef struct StPifModbusSlave
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t _my_addr;

	// Private Member Variable
	uint8_t *__p_buffer;
	PifModbusSlaveState __state;
	uint8_t *__p_discrete_inputs;
	uint8_t *__p_coils;
	uint16_t *__p_input_registers;
	uint16_t *__p_holding_registers;
	uint8_t *__p_tmp_bits;
	uint16_t __discrete_inputs_size;
	uint16_t __coils_size;
	uint16_t __input_registers_size;
	uint16_t __holding_registers_size;
	uint16_t __tmp_bits_size;

	// Private Event Variable
	PifEvtModbusReadCoils __evtReadCoils;
	PifEvtModbusReadDiscreteInputs __evtReadDiscreteInputs;
	PifEvtModbusReadHoldingRegisters __evtReadHoldingRegisters;
	PifEvtModbusReadInputRegisters __evtReadInputRegisters;
	PifEvtModbusWriteSingleCoil __evtWriteSingleCoil;
	PifEvtModbusWriteSingleRegister __evtWriteSingleRegister;
	PifEvtModbusWriteMultipleCoils __evtWriteMultipleCoils;
	PifEvtModbusWriteMultipleRegisters __evtWriteMultipleRegisters;
	PifEvtModbusReadWriteMultipleRegisters __evtReadWriteMultipleRegisters;
} PifModbusSlave;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusSlave_AttachDiscreteInput
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Pointer to the protocol working buffer.
 * @param size Number of bytes to process.
 * @param evtReadDiscreteInputs Input argument used by this API.
 */
void pifModbusSlave_AttachDiscreteInput(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadDiscreteInputs evtReadDiscreteInputs);

/**
 * @fn pifModbusSlave_AttachCoils
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Pointer to the protocol working buffer.
 * @param size Number of bytes to process.
 * @param evtReadCoils Input argument used by this API.
 * @param evtWriteSingleCoil Input argument used by this API.
 * @param evtWriteMultipleCoils Input argument used by this API.
 */
void pifModbusSlave_AttachCoils(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadCoils evtReadCoils, PifEvtModbusWriteSingleCoil evtWriteSingleCoil,
		PifEvtModbusWriteMultipleCoils evtWriteMultipleCoils);

/**
 * @fn pifModbusSlave_AttachInputRegisters
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Pointer to the protocol working buffer.
 * @param size Number of bytes to process.
 * @param evtReadInputRegisters Input argument used by this API.
 */
void pifModbusSlave_AttachInputRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadInputRegisters evtReadInputRegisters);

/**
 * @fn pifModbusSlave_AttachHoldingRegisters
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_buffer Pointer to the protocol working buffer.
 * @param size Number of bytes to process.
 * @param evtReadHoldingRegisters Input argument used by this API.
 * @param evtWriteSingleRegister Input argument used by this API.
 * @param evtWriteMultipleRegisters Input argument used by this API.
 */
void pifModbusSlave_AttachHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadHoldingRegisters evtReadHoldingRegisters, PifEvtModbusWriteSingleRegister evtWriteSingleRegister,
		PifEvtModbusWriteMultipleRegisters evtWriteMultipleRegisters);

/**
 * @fn pifModbusSlave_AllocTmpBits
 * @brief Performs the pifModbusSlave_AllocTmpBits operation.
 * @param p_owner Pointer to the protocol instance.
 * @param quantity Number of coils or registers to process.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusSlave_AllocTmpBits(PifModbusSlave *p_owner, uint16_t quantity);

/**
 * @fn pifModbusSlave_ReadCoils
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadCoils(PifModbusSlave *p_owner, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadDiscreteInputs
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadDiscreteInputs(PifModbusSlave *p_owner, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadHoldingRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadInputRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadInputRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_WriteSingleCoil
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_status Pointer to coil status value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteSingleCoil(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_status);

/**
 * @fn pifModbusSlave_WriteSingleRegister
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_value Pointer to register value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteSingleRegister(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value);

/**
 * @fn pifModbusSlave_WriteMultipleCoils
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteMultipleCoils(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_WriteMultipleRegisters
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_address Pointer to address value for validation and update.
 * @param p_quantity Pointer to quantity value for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_WriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadWriteMultipleRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param p_read_address Pointer to read start address for validation and update.
 * @param p_read_quantity Pointer to read quantity for validation and update.
 * @return Modbus error code indicating the result of the operation.
 */
PifModbusError pifModbusSlave_ReadWriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_read_address, uint16_t *p_read_quantity);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_SLAVE_H
