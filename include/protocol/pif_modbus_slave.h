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
 * @brief
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
 * @brief
 * @param p_owner
 * @param p_buffer
 * @param size
 * @param evtReadDiscreteInputs
 */
void pifModbusSlave_AttachDiscreteInput(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadDiscreteInputs evtReadDiscreteInputs);

/**
 * @fn pifModbusSlave_AttachCoils
 * @brief
 * @param p_owner
 * @param p_buffer
 * @param size
 * @param evtReadCoils
 * @param evtWriteSingleCoil
 * @param evtWriteMultipleCoils
 */
void pifModbusSlave_AttachCoils(PifModbusSlave *p_owner, uint8_t *p_buffer, uint16_t size,
		PifEvtModbusReadCoils evtReadCoils, PifEvtModbusWriteSingleCoil evtWriteSingleCoil,
		PifEvtModbusWriteMultipleCoils evtWriteMultipleCoils);

/**
 * @fn pifModbusSlave_AttachInputRegisters
 * @brief
 * @param p_owner
 * @param p_buffer
 * @param size
 * @param evtReadInputRegisters
 */
void pifModbusSlave_AttachInputRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadInputRegisters evtReadInputRegisters);

/**
 * @fn pifModbusSlave_AttachHoldingRegisters
 * @brief
 * @param p_owner
 * @param p_buffer
 * @param size
 * @param evtReadHoldingRegisters
 * @param evtWriteSingleRegister
 * @param evtWriteMultipleRegisters
 */
void pifModbusSlave_AttachHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_buffer, uint16_t size,
		PifEvtModbusReadHoldingRegisters evtReadHoldingRegisters, PifEvtModbusWriteSingleRegister evtWriteSingleRegister,
		PifEvtModbusWriteMultipleRegisters evtWriteMultipleRegisters);

/**
 * @fn pifModbusSlave_AllocTmpBits
 * @brief
 * @param p_owner
 * @param quantity
 * @return
 */
BOOL pifModbusSlave_AllocTmpBits(PifModbusSlave *p_owner, uint16_t quantity);

/**
 * @fn pifModbusSlave_ReadCoils
 * @brief
 * @param p_owner
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_ReadCoils(PifModbusSlave *p_owner, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadDiscreteInputs
 * @brief
 * @param p_owner
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_ReadDiscreteInputs(PifModbusSlave *p_owner, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadHoldingRegisters
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_ReadHoldingRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadInputRegisters
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_ReadInputRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_WriteSingleCoil
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_status
 * @return
 */
PifModbusError pifModbusSlave_WriteSingleCoil(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_status);

/**
 * @fn pifModbusSlave_WriteSingleRegister
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_value
 * @return
 */
PifModbusError pifModbusSlave_WriteSingleRegister(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_value);

/**
 * @fn pifModbusSlave_WriteMultipleCoils
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_WriteMultipleCoils(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_WriteMultipleRegisters
 * @brief
 * @param p_owner
 * @param p_address
 * @param p_quantity
 * @return
 */
PifModbusError pifModbusSlave_WriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_address, uint16_t *p_quantity);

/**
 * @fn pifModbusSlave_ReadWriteMultipleRegisters
 * @brief
 * @param p_owner
 * @param p_read_address
 * @param p_read_quantity
 * @return
 */
PifModbusError pifModbusSlave_ReadWriteMultipleRegisters(PifModbusSlave *p_owner, uint16_t *p_read_address, uint16_t *p_read_quantity);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_SLAVE_H
