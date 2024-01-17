#ifndef PIF_MODBUS_RTU_SLAVE_H
#define PIF_MODBUS_RTU_SLAVE_H


#include "communication/pif_uart.h"
#include "core/pif_timer.h"
#include "protocol/pif_modbus_rtu.h"
#include "protocol/pif_modbus_slave.h"


/**
 * @class StPifModbusRtuSlave
 * @brief
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
	uint16_t __interval;
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param my_addr
 * @return
 */
BOOL pifModbusRtuSlave_Init(PifModbusRtuSlave *p_owner, PifId id, PifTimerManager *p_timer_manager, uint8_t my_addr);

/**
 * @fn pifModbusRtuSlave_Clear
 * @brief
 * @param p_owner
 */
void pifModbusRtuSlave_Clear(PifModbusRtuSlave *p_owner);

/**
 * @fn pifModbusRtuSlave_SetReceiveTimeout
 * @brief
 * @param p_owner
 * @param receive_timeout
 */
void pifModbusRtuSlave_SetReceiveTimeout(PifModbusRtuSlave *p_owner, uint16_t receive_timeout);

/**
 * @fn pifModbusRtuSlave_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifModbusRtuSlave_AttachUart(PifModbusRtuSlave *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusRtuSlave_DetachUart
 * @brief
 * @param p_owner
 */
void pifModbusRtuSlave_DetachUart(PifModbusRtuSlave *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_SLAVE_H
