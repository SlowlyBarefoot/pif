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
 * @brief Represents the StPifModbusAsciiMaster data structure.
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
	void* __p_result;				// Where the response of the request in progress is read out to
	uint16_t __result_quantity;		// How many registers were asked for
	PifModbusResultKind __result_kind;
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
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param p_timer_manager Timer manager used to allocate protocol timers.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusAsciiMaster_Init(PifModbusAsciiMaster *p_owner, PifId id, PifTimerManager *p_timer_manager);

/**
 * @fn pifModbusAsciiMaster_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiMaster_Clear(PifModbusAsciiMaster *p_owner);

/**
 * @fn pifModbusAsciiMaster_SetResponseTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Response timeout value in timer ticks.
 */
void pifModbusAsciiMaster_SetResponseTimeout(PifModbusAsciiMaster *p_owner, uint16_t response_timeout);

/**
 * @fn pifModbusAsciiMaster_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 * @param name Optional name used for logging or identification.
 */
void pifModbusAsciiMaster_AttachUart(PifModbusAsciiMaster *p_owner, PifUart *p_uart);

/**
 * @fn pifModbusAsciiMaster_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiMaster_DetachUart(PifModbusAsciiMaster *p_owner);

/**
 * @fn pifModbusAsciiMaster_Check
 * @brief Carries the request that is on its way the rest of the way, and reports how far it has
 *        got. Every request function returns as soon as its frame is queued, because sending it
 *        and receiving the answer both need the UART task to run, so call this from a task until
 *        it stops returning MBMR_BUSY.
 *        On MBMR_DONE whatever the request asked for has been written into the buffer that was
 *        handed to the request function, so that buffer has to stay valid until then. On
 *        MBMR_ERROR the reason is in `_error`. Either one frees the master for the next request,
 *        which means a request whose result is never collected blocks every later one.
 * @param p_owner Pointer to the protocol instance.
 * @return MBMR_BUSY while the request is still in progress.
 */
PifModbusMasterResult pifModbusAsciiMaster_Check(PifModbusAsciiMaster *p_owner);

/**
 * @fn pifModbusAsciiMaster_ReadCoils
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusAsciiMaster_ReadDiscreteInputs
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param inputs Bitfield buffer for discrete input values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadDiscreteInputs(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @fn pifModbusAsciiMaster_ReadHoldingRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadHoldingRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_ReadInputRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadInputRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_WriteSingleCoil
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteSingleCoil(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, BOOL value);

/**
 * @fn pifModbusAsciiMaster_WriteSingleRegister
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteSingleRegister(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t value);

/**
 * @fn pifModbusAsciiMaster_WriteMultipleCoils
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteMultipleCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusAsciiMaster_WriteMultipleRegisters
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusAsciiMaster_ReadWriteMultipleRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address.
 * @param read_address Start address for the read section.
 * @param read_quantity Number of registers to read.
 * @param p_read_registers Buffer for register values.
 * @param write_address Start address for the write section.
 * @param write_quantity Number of registers to write.
 * @param p_write_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadWriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint8_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint8_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_MASTER_H
