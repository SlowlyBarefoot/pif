#ifndef PIF_MODBUS_ASCII_MASTER_H
#define PIF_MODBUS_ASCII_MASTER_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"
#include "protocol/pif_modbus.h"
#include "protocol/pif_modbus_ascii.h"


typedef enum EnPifModbusMasterState
{
	MBMS_IDLE			= 0,
	MBMS_REQUEST		= 1,
	MBMS_REQUEST_WAIT	= 2,
	MBMS_REQUEST_DELAY	= 3,
	MBMS_RESPONSE		= 4,
	MBMS_FINISH			= 5,
	MBMS_ERROR			= 6
} PifModbusMasterState;

/**
 * @class StPifModbusAsciiMaster
 * @brief
 */
typedef struct StPifModbusAsciiMaster
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
	uint8_t __buffer[520];
	uint16_t length;
	uint16_t index;
	uint8_t __tx_address;
	PifModbusRxState __rx_state;
} PifModbusAsciiMaster;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusAsciiMaster_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @return
 */
BOOL pifModbusAsciiMaster_Init(PifModbusAsciiMaster *p_owner, PifId id, PifTimerManager *p_timer_manager);

/**
 * @fn pifModbusAsciiMaster_Clear
 * @brief
 * @param p_owner
 */
void pifModbusAsciiMaster_Clear(PifModbusAsciiMaster *p_owner);

/**
 * @fn pifModbusAsciiMaster_SetResponseTimeout
 * @brief
 * @param p_owner
 * @param response_timeout
 */
void pifModbusAsciiMaster_SetResponseTimeout(PifModbusAsciiMaster *p_owner, uint16_t response_timeout);

/**
 * @fn pifModbusAsciiMaster_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 * @param name
 */
void pifModbusAsciiMaster_AttachUart(PifModbusAsciiMaster *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusAsciiMaster_DetachUart
 * @brief
 * @param p_owner
 */
void pifModbusAsciiMaster_DetachUart(PifModbusAsciiMaster *p_owner);

/**
 * @fn pifModbusAsciiMaster_ReadCoils
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param coils
 * @return
 */
BOOL pifModbusAsciiMaster_ReadCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusAsciiMaster_ReadDiscreteInputs
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param inputs
 * @return
 */
BOOL pifModbusAsciiMaster_ReadDiscreteInputs(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @fn pifModbusAsciiMaster_ReadHoldingRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusAsciiMaster_ReadHoldingRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_ReadInputRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusAsciiMaster_ReadInputRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_WriteSingleCoil
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param value
 * @return
 */
BOOL pifModbusAsciiMaster_WriteSingleCoil(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, BOOL value);

/**
 * @fn pifModbusAsciiMaster_WriteSingleRegister
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param value
 * @return
 */
BOOL pifModbusAsciiMaster_WriteSingleRegister(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value);

/**
 * @fn pifModbusAsciiMaster_WriteMultipleCoils
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param coils
 * @return
 */
BOOL pifModbusAsciiMaster_WriteMultipleCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusAsciiMaster_WriteMultipleRegisters
 * @brief
 * @param p_owner
 * @param slave
 * @param address
 * @param quantity
 * @param p_registers
 * @return
 */
BOOL pifModbusAsciiMaster_WriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_ReadWriteMultipleRegisters
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
BOOL pifModbusAsciiMaster_ReadWriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_MASTER_H
