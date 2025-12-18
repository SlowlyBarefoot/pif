#ifndef PIF_MODBUS_ASCII_SLAVE_H
#define PIF_MODBUS_ASCII_SLAVE_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"
#include "protocol/pif_modbus_ascii.h"
#include "protocol/pif_modbus_slave.h"


/**
 * @class StPifModbusAsciiSlave
 * @brief
 */
typedef struct StPifModbusAsciiSlave
{
	// The parent variable must be at the beginning of this structure.
	PifModbusSlave parent;

	// Public Member Variable

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifTimerManager *__p_timer_manager;
	PifUart *__p_uart;
	PifTimer *__p_timer;
	uint16_t __timeout;
	uint8_t __buffer[520];
	uint16_t __length;
	uint16_t __index;

	struct
	{
		PifModbusRxState state;
		uint8_t function;
		uint16_t length_pos;
	} __rx;
} PifModbusAsciiSlave;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusAsciiSlave_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param my_addr
 * @return
 */
BOOL pifModbusAsciiSlave_Init(PifModbusAsciiSlave *p_owner, PifId id, PifTimerManager *p_timer_manager, uint8_t my_addr);

/**
 * @fn pifModbusAsciiSlave_Clear
 * @brief
 * @param p_owner
 */
void pifModbusAsciiSlave_Clear(PifModbusAsciiSlave *p_owner);

/**
 * @fn pifModbusAsciiSlave_SetReceiveTimeout
 * @brief
 * @param p_owner
 * @param receive_timeout
 */
void pifModbusAsciiSlave_SetReceiveTimeout(PifModbusAsciiSlave *p_owner, uint16_t receive_timeout);

/**
 * @fn pifModbusAsciiSlave_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 * @param name
 */
void pifModbusAsciiSlave_AttachUart(PifModbusAsciiSlave *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusAsciiSlave_DetachUart
 * @brief
 * @param p_owner
 */
void pifModbusAsciiSlave_DetachUart(PifModbusAsciiSlave *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_SLAVE_H
