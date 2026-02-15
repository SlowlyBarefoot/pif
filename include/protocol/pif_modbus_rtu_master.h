#ifndef PIF_MODBUS_RTU_MASTER_H
#define PIF_MODBUS_RTU_MASTER_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"
#include "protocol/pif_modbus.h"
#include "protocol/pif_modbus_rtu.h"


typedef enum EnPifModbusMasterState
{
	MBMS_IDLE			= 0,
	MBMS_REQUEST		= 1,
	MBMS_REQUEST_WAIT	= 2,
	MBMS_REQUEST_DELAY	= 3,
	MBMS_RESPONSE		= 4,
	MBMS_RESPONSE_DELAY	= 5,
	MBMS_FINISH			= 6,
	MBMS_ERROR			= 7
} PifModbusMasterState;

/**
 * @class StPifModbusRtuMaster
 * @brief Represents the StPifModbusRtuMaster data structure.
 */
typedef struct StPifModbusRtuMaster
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifModbusError _error;

	// Private Member Variable
	PifModbusMasterState __state;
	PifTimerManager *__p_timer_manager;
	PifUart *__p_uart;
	PifTimer *__p_timer;
	uint16_t __timeout;
	uint32_t __delay;
	uint8_t __buffer[260];
	uint16_t length;
	uint16_t index;
	uint8_t __tx_address;
	PifModbusRxState __rx_state;
} PifModbusRtuMaster;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusRtuMaster_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param p_timer_manager Timer manager used to allocate protocol timers.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_Init(PifModbusRtuMaster *p_owner, PifId id, PifTimerManager *p_timer_manager);

/**
 * @fn pifModbusRtuMaster_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuMaster_Clear(PifModbusRtuMaster *p_owner);

/**
 * @fn pifModbusRtuMaster_SetResponseTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Response timeout value in timer ticks.
 */
void pifModbusRtuMaster_SetResponseTimeout(PifModbusRtuMaster *p_owner, uint16_t response_timeout);

/**
 * @fn pifModbusRtuMaster_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifModbusRtuMaster_AttachUart(PifModbusRtuMaster *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusRtuMaster_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuMaster_DetachUart(PifModbusRtuMaster *p_owner);

/**
 * @fn pifModbusRtuMaster_ReadCoils
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param coils Bitfield buffer for coil values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_ReadCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_ReadDiscreteInputs
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param inputs Bitfield buffer for discrete input values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_ReadDiscreteInputs(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @fn pifModbusRtuMaster_ReadHoldingRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_ReadHoldingRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadInputRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_ReadInputRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_WriteSingleCoil
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_WriteSingleCoil(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, BOOL value);

/**
 * @fn pifModbusRtuMaster_WriteSingleRegister
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_WriteSingleRegister(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value);

/**
 * @fn pifModbusRtuMaster_WriteMultipleCoils
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param coils Bitfield buffer for coil values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_WriteMultipleCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_WriteMultipleRegisters
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_WriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadWriteMultipleRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param read_address Start address for the read section.
 * @param read_quantity Number of registers to read.
 * @param p_read_registers Buffer for register values.
 * @param write_address Start address for the write section.
 * @param write_quantity Number of registers to write.
 * @param p_write_registers Buffer for register values.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_ReadWriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_MASTER_H
