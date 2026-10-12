// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MODBUS_RTU_MASTER_H
#define PIF_MODBUS_RTU_MASTER_H

#include "communication/pif_uart.h"
#include "protocol/pif_modbus.h"
#include "protocol/pif_modbus_rtu.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_MODBUS_RTU_MASTER)
	#define COLLECT_SIGNAL_MODBUS_RTU_MASTER
	#include "core/pif_collect_signal.h"
#endif

#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER

typedef enum EnPifModbusRtuMasterCsFlag
{
	MBRM_CSF_OFF		= 0,

	MBRM_CSF_STATE_IDX	= 0,	// PifModbusMasterState

	MBRM_CSF_STATE_BIT	= 1,
	MBRM_CSF_ALL_BIT	= 1,

	MBRM_CSF_COUNT		= 1
} PifModbusRtuMasterCsFlag;

#endif	// COLLECT_SIGNAL_MODBUS_RTU_MASTER

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
	PifUart *__p_uart;
	PifModbusTimeout __wait;
	uint16_t __timeout;				// In ms
	uint32_t __delay;
	uint8_t __buffer[260];
	void* __p_result;				// Where the response of the request in progress is read out to
	uint16_t __result_quantity;		// How many registers were asked for
	PifModbusResultKind __result_kind;
	uint16_t length;
	uint16_t index;
	uint8_t __request[6];			// Address, function and the 4 bytes after them, which the response is checked against
	BOOL __broadcast;				// The request went to every slave, so no response is awaited
	PifModbusRxState __rx_state;
#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER
	PifCollectSignalChannel __cs[MBRM_CSF_COUNT];
#endif
} PifModbusRtuMaster;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusRtuMaster_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_Init(PifModbusRtuMaster *p_owner, PifId id);

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
 * @param response_timeout Time in ms to wait for the response once the request is out, and for each next byte of it.
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
 * @fn pifModbusRtuMaster_Check
 * @brief Carries the request that is on its way the rest of the way, and reports how far it has
 *        got. Every request function returns as soon as its frame is queued, because sending it
 *        and receiving the answer both need the UART task to run, so call this from a task until
 *        it stops returning MBMR_BUSY.
 *        On MBMR_DONE whatever the request asked for has been written into the buffer that was
 *        handed to the request function, so that buffer has to stay valid until then. On
 *        MBMR_ERROR the reason is in `_error`. Either one frees the master for the next request,
 *        which means a request whose result is never collected blocks every later one.
 *        The timeout is checked here and in the UART callbacks, so it needs no timer: a request
 *        that gets no answer ends in MBMR_ERROR with MBE_TIMEOUT on the first call past it.
 * @param p_owner Pointer to the protocol instance.
 * @return MBMR_BUSY while the request is still in progress.
 */
PifModbusMasterResult pifModbusRtuMaster_Check(PifModbusRtuMaster *p_owner);

/**
 * @fn pifModbusRtuMaster_ReadCoils
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_ReadCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_ReadDiscreteInputs
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_inputs Bitfield buffer for discrete input values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_ReadDiscreteInputs(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @fn pifModbusRtuMaster_ReadHoldingRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_ReadHoldingRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadInputRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_ReadInputRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_WriteSingleCoil
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_WriteSingleCoil(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, BOOL value);

/**
 * @fn pifModbusRtuMaster_WriteSingleRegister
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_WriteSingleRegister(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t value);

/**
 * @fn pifModbusRtuMaster_WriteMultipleCoils
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_WriteMultipleCoils(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @fn pifModbusRtuMaster_WriteMultipleRegisters
 * @brief Writes data through the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_WriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @fn pifModbusRtuMaster_ReadWriteMultipleRegisters
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param read_address Start address for the read section.
 * @param read_quantity Number of registers to read.
 * @param p_read_registers Buffer for register values.
 * @param write_address Start address for the write section.
 * @param write_quantity Number of registers to write.
 * @param p_write_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusRtuMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusRtuMaster_ReadWriteMultipleRegisters(PifModbusRtuMaster *p_owner, uint8_t slave, uint16_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint16_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef COLLECT_SIGNAL_MODBUS_RTU_MASTER

/**
 * @fn pifModbusRtuMaster_SetCsFlag
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the protocol instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusRtuMaster_SetCsFlag(PifModbusRtuMaster *p_owner, PifModbusRtuMasterCsFlag flag);

/**
 * @fn pifModbusRtuMaster_ResetCsFlag
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the protocol instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifModbusRtuMaster_ResetCsFlag(PifModbusRtuMaster *p_owner, PifModbusRtuMasterCsFlag flag);

#endif	// COLLECT_SIGNAL_MODBUS_RTU_MASTER

#ifdef __cplusplus
}
#endif

#endif  // PIF_MODBUS_RTU_MASTER_H
