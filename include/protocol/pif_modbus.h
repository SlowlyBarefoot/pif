// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus.h
 * @brief Definitions shared by the Modbus RTU and ASCII masters and slaves: function codes, limits, states and errors.
 */
#ifndef PIF_MODBUS_H
#define PIF_MODBUS_H


#include "core/pif_task.h"


/// Time in ms a master waits for the response once the request is out, and for each next byte of it.
/// Sending the request gets its own transfer time on top, so a long request does not use it up.
#ifndef PIF_MODBUS_MASTER_TIMEOUT
	#define PIF_MODBUS_MASTER_TIMEOUT		500
#endif

/// Time in ms a slave waits for the next byte of a frame it has started to receive, before it drops
/// the frame. It runs from byte to byte, so a long frame at a low baud rate is not cut short.
/// An RTU slave also waits this long for another slave to answer the request sent to it, before it
/// takes the next frame for a request again.
#ifndef PIF_MODBUS_SLAVE_TIMEOUT
	#define PIF_MODBUS_SLAVE_TIMEOUT		300
#endif

/** @name Request limits
 *  Largest quantities one request may carry, as the Modbus specification sets them.
 *  @{
 */
#define PIF_MODBUS_MAXREAD_BITS			2000	///< Bits a read of coils or discrete inputs may ask for
#define PIF_MODBUS_MAXREAD_REGISTERS		125	///< Registers a read may ask for
#define PIF_MODBUS_MAXWRITE_BITS			1968	///< Coils a write may carry
#define PIF_MODBUS_MAXWRITE_REGISTERS		123	///< Registers a write may carry
#define PIF_MODBUS_MAXRW_WRITE_REGISTERS	121	///< Registers the write part of function 23 may carry
/** @} */

/// Slave address every slave carries out a write request for without answering it.
#define PIF_MODBUS_BROADCAST_ADDRESS		0

/**
 * Read a bit from the bitfield bf at position b
 */
#define PIF_MODBUS_READ_BIT_FIELD(bf, b)		((BOOL) (((bf)[(b) / 8] >> ((b) % 8)) & 1))

/**
 * Write value v to the bitfield bf at position b
 */
#define PIF_MODBUS_WRITE_BIT_FIELD(bf, b, v)	(((bf)[(b) / 8]) = ((v) ? (((bf)[(b) / 8]) | (0x1 << ((b) % 8))) : (((bf)[(b) / 8]) & ~(0x1 << ((b) % 8)))))

/**
 * Reset (zero) the whole bitfield
 */
#define PIF_MODBUS_RESET_BIT_FIELD(bf)			memset(bf, 0, sizeof(bf))


/**
 * @brief Modbus function codes this implementation handles.
 */
typedef enum EnPifModbusFunction
{
	MBF_READ_COILS						= 1,	///< 0x01 Read Coils
	MBF_READ_DISCRETE_INPUTS			= 2,	///< 0x02 Read Discrete Inputs
	MBF_READ_HOLDING_REGISTERS			= 3,	///< 0x03 Read Holding Registers
	MBF_READ_INPUT_REGISTERS			= 4,	///< 0x04 Read Input Registers
	MBF_WRITE_SINGLE_COIL				= 5,	///< 0x05 Write Single Coil
	MBF_WRITE_SINGLE_REGISTER			= 6,	///< 0x06 Write Single Register
	MBF_WRITE_MULTIPLE_COILS			= 15,	///< 0x0F Write Multiple Coils
	MBF_WRITE_MULTIPLE_REGISTERS		= 16,	///< 0x10 Write Multiple Registers
	MBF_READ_WRITE_MULTIPLE_REGISTERS	= 23	///< 0x17 Read/Write Multiple Registers
} PifModbusFunction;

/**
 * @brief Receive state of a frame.
 */
typedef enum EnPifModbusRxState
{
	MBRS_IDLE		= 0,	///< Waiting for a frame
	MBRS_FUNCTION	= 1,	///< Address received, waiting for the function code
	MBRS_LENGTH		= 2,	///< Function received, waiting for the length
	MBRS_DATA		= 3,	///< Receiving the data
	MBRS_FINISH		= 4,	///< A whole frame was received
	MBRS_TIMEOUT	= 5,	///< The frame was not finished in time
	MBRS_ERROR		= 6,	///< The frame is not valid
	MBRS_IGNORE		= 7	///< The frame is for another slave and is skipped
} PifModbusRxState;

/**
 * @enum EnPifModbusMasterState
 * @brief Where a master is in carrying one request. Shared by the RTU and ASCII masters.
 */
typedef enum EnPifModbusMasterState
{
	MBMS_IDLE			= 0,	///< No request in progress
	MBMS_REQUEST		= 1,	///< The frame is being handed to the UART
	MBMS_REQUEST_WAIT	= 2,	///< The UART is still shifting the frame out. An RS-485 line is turned around by it.
	MBMS_RESPONSE		= 3,	///< The response is awaited
	MBMS_RESPONSE_DELAY	= 4,	///< RTU only: the silent interval after the response has to pass
	MBMS_FINISH			= 5,	///< ASCII only: the request is over and _error says how it ended
	MBMS_ERROR			= 6		///< No response came in time
} PifModbusMasterState;

/**
 * @enum EnPifModbusMasterResult
 * @brief What a master is doing with the request it was given. A request function returns as soon
 *        as the frame is queued, because sending it and receiving the answer both need the UART
 *        task to run, so this is what says whether it is over and how it ended.
 */
typedef enum EnPifModbusMasterResult
{
	MBMR_IDLE			= 0,	///< No request
	MBMR_BUSY			= 1,	///< The request is still being sent, or its answer is still awaited
	MBMR_DONE			= 2,	///< It finished, and whatever was asked for has been handed over
	MBMR_ERROR			= 3		///< It failed. _error says why
} PifModbusMasterResult;

/**
 * @enum EnPifModbusResultKind
 * @brief What the request in progress asked the slave for, which is what decides how its response
 *        is read out into the caller's buffer.
 */
typedef enum EnPifModbusResultKind
{
	MBRK_NONE			= 0,	///< Nothing comes back but the acknowledgement
	MBRK_BITS			= 1,	///< Coils or discrete inputs, as bytes of bits
	MBRK_REGISTERS		= 2		///< Holding or input registers
} PifModbusResultKind;

/**
 * @enum EnPifModbusError
 * @brief Values <= 0 are library errors, > 0 are Modbus exception codes, whether a slave sent one
 *        back or a slave is about to send one.
 */
typedef enum EnPifModbusError
{
    /// Library errors
    MBE_INVALID_UNIT_ID = -7,  /**< Received invalid unit ID in response from server */
    MBE_CRC = -5,              /**< Received invalid CRC */
    MBE_TRANSPORT = -4,        /**< Transport error */
    MBE_TIMEOUT = -3,          /**< Read/write timeout occurred */
    MBE_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    MBE_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    MBE_NONE = 0,              /**< No error */

    /// Modbus exceptions
    MBE_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
	MBE_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
	MBE_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    MBE_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} PifModbusError;


/**
 * @brief One byte of a coil or discrete input bitfield. An array of them holds 8 bits per byte.
 */
typedef uint8_t PifModbusBitField;

/**
 * @brief Pointer to a coil or discrete input bitfield.
 */
typedef uint8_t* PifModbusBitFieldP;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads a 16 bit value stored high byte first, as Modbus sends it.
 * @param p_stream Pointer to a Modbus network-order byte stream.
 * @return The value.
 */
uint16_t pifModbus_StreamToShort(uint8_t *p_stream);

/**
 * @brief Stores a 16 bit value high byte first, as Modbus sends it.
 * @param value Value to store.
 * @param p_stream Pointer to a Modbus network-order byte stream.
 */
void pifModbus_ShortToStream(uint16_t value, uint8_t *p_stream);

/**
 * @brief Checks a response frame against the request it answers.
 * @param p_request First 6 bytes of the request: address, function and the 4 bytes after them.
 * @param p_response Response frame without its CRC or LRC, starting with the address.
 * @param length Length of p_response.
 * @param kind What the request asked for.
 * @param quantity How many bits or registers were asked for. Unused for MBRK_NONE.
 * @return MBE_NONE when it answers the request, the exception code when the slave answered with an
 *         exception, otherwise a library error.
 */
PifModbusError pifModbus_CheckResponse(const uint8_t *p_request, const uint8_t *p_response, uint16_t length,
		PifModbusResultKind kind, uint16_t quantity);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_H
