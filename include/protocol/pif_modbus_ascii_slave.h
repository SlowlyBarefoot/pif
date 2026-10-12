// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MODBUS_ASCII_SLAVE_H
#define PIF_MODBUS_ASCII_SLAVE_H


#include "communication/pif_uart.h"
#include "protocol/pif_modbus_ascii.h"
#include "protocol/pif_modbus_slave.h"


/**
 * @class StPifModbusAsciiSlave
 * @brief Represents the StPifModbusAsciiSlave data structure.
 */
typedef struct StPifModbusAsciiSlave
{
	// The parent variable must be at the beginning of this structure.
	PifModbusSlave parent;

	// Public Member Variable

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifUart *__p_uart;
	PifModbusTimeout __wait;
	uint16_t __timeout;				// In ms
	uint8_t __buffer[520];
	uint16_t __length;
	uint16_t __index;

	struct
	{
		PifModbusRxState state;
		uint8_t function;
		BOOL broadcast;			// The request went to every slave, so it is not answered
	} __rx;
} PifModbusAsciiSlave;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusAsciiSlave_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param my_addr Local slave address handled by this instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusAsciiSlave_Init(PifModbusAsciiSlave *p_owner, PifId id, uint8_t my_addr);

/**
 * @fn pifModbusAsciiSlave_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiSlave_Clear(PifModbusAsciiSlave *p_owner);

/**
 * @fn pifModbusAsciiSlave_SetReceiveTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Time in ms to wait for the next byte of a frame before it is dropped.
 */
void pifModbusAsciiSlave_SetReceiveTimeout(PifModbusAsciiSlave *p_owner, uint16_t receive_timeout);

/**
 * @fn pifModbusAsciiSlave_AttachUart
 * @brief Attaches the UART. A frame cut short is dropped by its rx task, on the first run past the
 *        receive timeout.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifModbusAsciiSlave_AttachUart(PifModbusAsciiSlave *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusAsciiSlave_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiSlave_DetachUart(PifModbusAsciiSlave *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_SLAVE_H
