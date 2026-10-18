// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus_ascii_master.h
 * @brief Modbus ASCII master on a PifUart.
 *
 * Sends one request at a time to a slave and waits for its response. A request function starts the request and
 * returns at once. pifModbusAsciiMaster_Check() reports when it is over, and _error tells how it ended.
 */
#ifndef PIF_MODBUS_ASCII_MASTER_H
#define PIF_MODBUS_ASCII_MASTER_H

#include "communication/pif_uart.h"
#include "protocol/pif_modbus.h"
#include "protocol/pif_modbus_ascii.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_MODBUS_ASCII_MASTER)
	#define COLLECT_SIGNAL_MODBUS_ASCII_MASTER	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_NO_MODBUS_ASCII_MASTER is not
	#include "core/pif_collect_signal.h"
#endif

#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER

/**
 * @brief Signals of a Modbus ASCII master for the CsFlag functions.
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifModbusAsciiMasterCsFlag
{
	MBAM_CSF_OFF		= 0,

	MBAM_CSF_STATE_IDX	= 0,	///< PifModbusMasterState

	MBAM_CSF_STATE_BIT	= 1,
	MBAM_CSF_ALL_BIT	= 1,

	MBAM_CSF_COUNT		= 1
} PifModbusAsciiMasterCsFlag;

#endif	// COLLECT_SIGNAL_MODBUS_ASCII_MASTER

/**
 * @class StPifModbusAsciiMaster
 * @brief Modbus ASCII master.
 */
typedef struct StPifModbusAsciiMaster
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifModbusError _error;	///< Result of the last request

	// Private Member Variable
	PifModbusMasterState __state;
	PifUart *__p_uart;
	PifTaskTimeout __timeout;
	uint16_t __timeout_ms;
	uint8_t __buffer[520];
	void* __p_result;				///< Where the response of the request in progress is read out to
	uint16_t __result_quantity;		///< How many registers were asked for
	PifModbusResultKind __result_kind;
	uint16_t length;				///< Bytes of the frame in __buffer
	uint16_t index;					///< Position in __buffer
	uint8_t __request[6];			///< Address, function and the 4 bytes after them, which the response is checked against
	BOOL __broadcast;				///< The request went to every slave, so no response is awaited
	PifModbusRxState __rx_state;
#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER
	PifCollectSignalChannel __cs[MBAM_CSF_COUNT];
#endif
} PifModbusAsciiMaster;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a master with the default response timeout, PIF_MODBUS_MASTER_TIMEOUT. Attach a UART next.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusAsciiMaster_Init(PifModbusAsciiMaster *p_owner, PifId id);

/**
 * @brief Removes the collected signals of the master. The UART is detached with
 *        pifModbusAsciiMaster_DetachUart().
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiMaster_Clear(PifModbusAsciiMaster *p_owner);

/**
 * @brief Changes how long the master waits for a response.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Time in ms to wait for the response once the request is out, and for each next byte of it.
 */
void pifModbusAsciiMaster_SetResponseTimeout(PifModbusAsciiMaster *p_owner, uint16_t response_timeout);

/**
 * @brief Makes the master the client of a UART, which carries its requests and responses.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifModbusAsciiMaster_AttachUart(PifModbusAsciiMaster *p_owner, PifUart *p_uart);

/**
 * @brief Detaches the master from its UART.
 * @param p_owner Pointer to the protocol instance.
 */
void pifModbusAsciiMaster_DetachUart(PifModbusAsciiMaster *p_owner);

/**
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
PifModbusMasterResult pifModbusAsciiMaster_Check(PifModbusAsciiMaster *p_owner);

/**
 * @brief Starts a Read Coils (0x01) request. The coils go to p_coils, 8 per byte.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @brief Starts a Read Discrete Inputs (0x02) request. The inputs go to p_inputs, 8 per byte.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_inputs Bitfield buffer for discrete input values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadDiscreteInputs(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_inputs);

/**
 * @brief Starts a Read Holding Registers (0x03) request. The registers go to p_registers.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadHoldingRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @brief Starts a Read Input Registers (0x04) request. The registers go to p_registers.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadInputRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @brief Starts a Write Single Coil (0x05) request.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteSingleCoil(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, BOOL value);

/**
 * @brief Starts a Write Single Register (0x06) request.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param value Value to write.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteSingleRegister(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t value);

/**
 * @brief Starts a Write Multiple Coils (0x0F) request with the coils of p_coils, 8 per byte.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_coils Bitfield buffer for coil values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteMultipleCoils(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, PifModbusBitFieldP p_coils);

/**
 * @brief Starts a Write Multiple Registers (0x10) request with the registers of p_registers.
 * @param p_owner Pointer to the protocol instance.
 * @param slave Target Modbus slave address, 1 to 247. A write may also go to
 *        PIF_MODBUS_BROADCAST_ADDRESS, which every slave carries out without answering.
 * @param address Start address of coils or registers.
 * @param quantity Number of coils or registers to process.
 * @param p_registers Buffer for register values.
 * @return TRUE once the request is queued; otherwise FALSE. Queued is not answered:
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_WriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t address, uint16_t quantity, uint16_t *p_registers);

/**
 * @brief Starts a Read/Write Multiple Registers (0x17) request. The write is carried out before the read.
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
 *         pifModbusAsciiMaster_Check() is what reports the outcome, and the buffers passed
 *         here have to stay valid until it does.
 */
BOOL pifModbusAsciiMaster_ReadWriteMultipleRegisters(PifModbusAsciiMaster *p_owner, uint8_t slave, uint16_t read_address, uint16_t read_quantity, uint16_t *p_read_registers,
		uint16_t write_address, uint16_t write_quantity, uint16_t *p_write_registers);

#ifdef COLLECT_SIGNAL_MODBUS_ASCII_MASTER

/**
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the protocol instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifModbusAsciiMaster_SetCsFlag(PifModbusAsciiMaster *p_owner, PifModbusAsciiMasterCsFlag flag);

/**
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the protocol instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifModbusAsciiMaster_ResetCsFlag(PifModbusAsciiMaster *p_owner, PifModbusAsciiMasterCsFlag flag);

#endif	// COLLECT_SIGNAL_MODBUS_ASCII_MASTER

#ifdef __cplusplus
}
#endif

#endif  // PIF_MODBUS_ASCII_MASTER_H
