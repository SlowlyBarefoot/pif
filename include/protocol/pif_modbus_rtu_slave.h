// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MODBUS_RTU_SLAVE_H
#define PIF_MODBUS_RTU_SLAVE_H


#include "communication/pif_uart.h"
#include "protocol/pif_modbus_rtu.h"
#include "protocol/pif_modbus_slave.h"


/**
 * @class StPifModbusRtuSlave
 * @brief Represents the StPifModbusRtuSlave data structure.
 */
typedef struct StPifModbusRtuSlave
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
	uint8_t __buffer[260];
	uint16_t __length;
	uint16_t __index;

	struct
	{
		PifModbusRxState state;
		uint8_t function;
		uint16_t length_pos;
		uint32_t last_time;		// When the last byte was read
		uint32_t end_time;		// When the last frame ended
		BOOL response;			// The next frame is the response of the slave the last request went to
		BOOL illegal;			// The frame ignored is a request to this slave with a function it lacks
		BOOL broadcast;			// The request went to every slave, so it is not answered
	} __rx;
} PifModbusRtuSlave;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusRtuSlave_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param my_addr Local slave address handled by this instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, uint8_t my_addr);

/**
 * @fn pifModbusRtuSlave_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuSlave_Clear(PifModbusRtuSlave *p_owner);

/**
 * @fn pifModbusRtuSlave_SetReceiveTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Time in ms to wait for the next byte of a frame before it is dropped.
 */
void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout);

/**
 * @fn pifModbusRtuSlave_AttachUart
 * @brief Attaches the UART. Its rx task has to run periodically (TM_PERIOD), not only when bytes
 *        come in: the receive timeout and the end of a frame that is passed over are noticed there.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifModbusRtuSlave_AttachUart(PifModbusRtuSlave *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusRtuSlave_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuSlave_DetachUart(PifModbusRtuSlave *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_SLAVE_H
