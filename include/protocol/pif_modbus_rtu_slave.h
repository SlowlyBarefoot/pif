#ifndef PIF_MODBUS_RTU_SLAVE_H
#define PIF_MODBUS_RTU_SLAVE_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"
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
	PifTimerManager *__p_timer_manager;
	PifUart *__p_uart;
	PifTimer *__p_timer;
	uint16_t __timeout;
	uint32_t __delay;
	uint8_t __buffer[260];
	uint16_t __length;
	uint16_t __index;
	uint32_t __last_receive_time;

	struct
	{
		PifModbusRxState state;
		uint8_t function;
		uint16_t length_pos;
		uint32_t last_time;
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
 * @param p_timer_manager Timer manager used to allocate protocol timers.
 * @param my_addr Local slave address handled by this instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, PifTimerManager *p_timer_manager, uint8_t my_addr);

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
 * @param receive_timeout Receive timeout value in timer ticks.
 */
void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout);

/**
 * @fn pifModbusRtuSlave_AttachUart
 * @brief Attaches an interface or callback to the instance.
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
