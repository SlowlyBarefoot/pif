// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus_rtu_slave.h
 * @brief Modbus RTU slave on a PifUart.
 *
 * Answers the requests sent to its address from the data areas attached with the pifModbusSlave_Attach
 * functions.
 */
#ifndef PIF_MODBUS_RTU_SLAVE_H
#define PIF_MODBUS_RTU_SLAVE_H


#include "communication/pif_uart.h"
#include "protocol/pif_modbus_rtu.h"
#include "protocol/pif_modbus_slave.h"


/**
 * @class StPifModbusRtuSlave
 * @brief Modbus RTU slave. The common part is parent.
 */
typedef struct StPifModbusRtuSlave
{
	/// The parent variable must be at the beginning of this structure.
	PifModbusSlave parent;

	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID

	// Private Member Variable
	PifUart *__p_uart;
	PifTaskTimeout __timeout;
	uint16_t __timeout_ms;
	uint8_t __buffer[260];
	uint16_t __length;
	uint16_t __index;

	struct
	{
		PifModbusRxState state;
		uint8_t function;
		uint16_t length_pos;
		uint32_t last_time;		///< When the last byte was read
		uint32_t end_time;		///< When the last frame ended
		BOOL response;			///< The next frame is the response of the slave the last request went to
		BOOL illegal;			///< The frame ignored is a request to this slave with a function it lacks
		BOOL broadcast;			///< The request went to every slave, so it is not answered
	} __rx;
} PifModbusRtuSlave;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a slave that answers the requests to my_addr, with the default receive timeout. Attach a UART
 *        and the data areas next.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param my_addr Own slave address, 1 to 247.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, uint8_t my_addr);

/**
 * @brief Frees the buffer the slave allocated for bit responses.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuSlave_Clear(PifModbusRtuSlave *p_owner);

/**
 * @brief Changes how long the slave waits for the next byte of a frame.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Time in ms to wait for the next byte of a frame before it is dropped.
 */
void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout);

/**
 * @brief Attaches the UART. Its rx task has to run periodically (TM_PERIOD), not only when bytes
 *        come in: the receive timeout and the end of a frame that is passed over are noticed there.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifModbusRtuSlave_AttachUart(PifModbusRtuSlave *p_owner, PifUart *p_uart);

/**
 * @brief Detaches the slave from its UART.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusRtuSlave_DetachUart(PifModbusRtuSlave *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_SLAVE_H
