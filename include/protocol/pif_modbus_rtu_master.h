#ifndef PIF_MODBUS_RTU_MASTER_H
#define PIF_MODBUS_RTU_MASTER_H


#include "communication/pif_uart.h"
#include "core/pif_timer.h"
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
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @return
 */
BOOL pifModbusRtuMaster_Init(PifModbusRtuMaster *p_owner, PifId id, PifTimerManager *p_timer_manager);

/**
 * @fn pifModbusRtuMaster_Clear
 * @brief
 * @param p_owner
 */
void pifModbusRtuMaster_Clear(PifModbusRtuMaster *p_owner);

/**
 * @fn pifModbusRtuMaster_SetResponseTimeout
 * @brief
 * @param p_owner
 * @param response_timeout
 */
void pifModbusRtuMaster_SetResponseTimeout(PifModbusRtuMaster *p_owner, uint16_t response_timeout);

/**
 * @fn pifModbusRtuMaster_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifModbusRtuMaster_AttachUart(PifModbusRtuMaster *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusRtuMaster_DetachUart
 * @brief
 * @param p_owner
 */
void pifModbusRtuMaster_DetachUart(PifModbusRtuMaster *p_owner);

/**
 * @fn pifModbusRtuMaster_ReadCoils
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param coils
 * @return
 */
BOOL pifModbusRtuMaster_ReadCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_ReadDiscreteInputs
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param inputs
 * @return
 */
BOOL pifModbusRtuMaster_ReadDiscreteInputs(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @fn pifModbusRtuMaster_ReadHoldingRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusRtuMaster_ReadHoldingRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadInputRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusRtuMaster_ReadInputRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_WriteSingleCoil
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param value
 * @return
 */
BOOL pifModbusRtuMaster_WriteSingleCoil(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, BOOL value);

/**
 * @fn pifModbusRtuMaster_WriteSingleRegister
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param value
 * @return
 */
BOOL pifModbusRtuMaster_WriteSingleRegister(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value);

/**
 * @fn pifModbusRtuMaster_WriteMultipleCoils
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param coils
 * @return
 */
BOOL pifModbusRtuMaster_WriteMultipleCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_WriteMultipleRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusRtuMaster_WriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadWriteMultipleRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param read_address
 * @param read_quantity
 * @param p_read_registers
 * @param write_address
 * @param write_quantity
 * @param p_write_registers
 * @return
 */
BOOL pifModbusRtuMaster_ReadWriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_MASTER_H
