// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MODBUS_H
#define PIF_MODBUS_H


#include "core/pif_task.h"


// Time in ms a master waits for the response once the request is out, and for each next byte of it.
// Sending the request gets its own transfer time on top, so a long request does not use it up.
#ifndef PIF_MODBUS_MASTER_TIMEOUT
	#define PIF_MODBUS_MASTER_TIMEOUT		500
#endif

// Time in ms a slave waits for the next byte of a frame it has started to receive, before it drops
// the frame. It runs from byte to byte, so a long frame at a low baud rate is not cut short.
// An RTU slave also waits this long for another slave to answer the request sent to it, before it
// takes the next frame for a request again.
#ifndef PIF_MODBUS_SLAVE_TIMEOUT
	#define PIF_MODBUS_SLAVE_TIMEOUT		300
#endif

// Largest quantities one request may carry, as the Modbus specification sets them.
#define PIF_MODBUS_MAXREAD_BITS			2000
#define PIF_MODBUS_MAXREAD_REGISTERS		125
#define PIF_MODBUS_MAXWRITE_BITS			1968
#define PIF_MODBUS_MAXWRITE_REGISTERS		123
#define PIF_MODBUS_MAXRW_WRITE_REGISTERS	121

// Slave address every slave carries out a write request for without answering it.
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


typedef enum EnPifModbusFunction
{
	MBF_READ_COILS						= 1,
	MBF_READ_DISCRETE_INPUTS			= 2,
	MBF_READ_HOLDING_REGISTERS			= 3,
	MBF_READ_INPUT_REGISTERS			= 4,
	MBF_WRITE_SINGLE_COIL				= 5,
	MBF_WRITE_SINGLE_REGISTER			= 6,
	MBF_WRITE_MULTIPLE_COILS			= 15,
	MBF_WRITE_MULTIPLE_REGISTERS		= 16,
	MBF_READ_WRITE_MULTIPLE_REGISTERS	= 23
} PifModbusFunction;

typedef enum EnPifModbusRxState
{
	MBRS_IDLE		= 0,
	MBRS_FUNCTION	= 1,
	MBRS_LENGTH		= 2,
	MBRS_DATA		= 3,
	MBRS_FINISH		= 4,
	MBRS_TIMEOUT	= 5,
	MBRS_ERROR		= 6,
	MBRS_IGNORE		= 7
} PifModbusRxState;

/**
 * @enum EnPifModbusMasterState
 * @brief Where a master is in carrying one request. Shared by the RTU and ASCII masters.
 */
typedef enum EnPifModbusMasterState
{
	MBMS_IDLE			= 0,
	MBMS_REQUEST		= 1,	// The frame is being handed to the UART
	MBMS_REQUEST_WAIT	= 2,	// The UART is still shifting the frame out. An RS-485 line is turned around by it.
	MBMS_RESPONSE		= 3,	// The response is awaited
	MBMS_RESPONSE_DELAY	= 4,	// RTU only: the silent interval after the response has to pass
	MBMS_FINISH			= 5,	// ASCII only: the request is over and _error says how it ended
	MBMS_ERROR			= 6		// No response came in time
} PifModbusMasterState;

/**
 * @enum EnPifModbusMasterResult
 * @brief What a master is doing with the request it was given. A request function returns as soon
 *        as the frame is queued, because sending it and receiving the answer both need the UART
 *        task to run, so this is what says whether it is over and how it ended.
 */
typedef enum EnPifModbusMasterResult
{
	MBMR_IDLE			= 0,	// No request
	MBMR_BUSY			= 1,	// The request is still being sent, or its answer is still awaited
	MBMR_DONE			= 2,	// It finished, and whatever was asked for has been handed over
	MBMR_ERROR			= 3		// It failed. _error says why
} PifModbusMasterResult;

/**
 * @enum EnPifModbusResultKind
 * @brief What the request in progress asked the slave for, which is what decides how its response
 *        is read out into the caller's buffer.
 */
typedef enum EnPifModbusResultKind
{
	MBRK_NONE			= 0,	// Nothing comes back but the acknowledgement
	MBRK_BITS			= 1,	// Coils or discrete inputs, as bytes of bits
	MBRK_REGISTERS		= 2		// Holding or input registers
} PifModbusResultKind;

/**
 * @enum EnPifModbusError
 * @brief Values <= 0 are library errors, > 0 are Modbus exception codes, whether a slave sent one
 *        back or a slave is about to send one.
 */
typedef enum EnPifModbusError
{
    // Library errors
    MBE_INVALID_UNIT_ID = -7,  /**< Received invalid unit ID in response from server */
    MBE_CRC = -5,              /**< Received invalid CRC */
    MBE_TRANSPORT = -4,        /**< Transport error */
    MBE_TIMEOUT = -3,          /**< Read/write timeout occurred */
    MBE_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    MBE_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    MBE_NONE = 0,              /**< No error */

    // Modbus exceptions
    MBE_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
	MBE_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
	MBE_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    MBE_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} PifModbusError;


/**
 * @class PifModbusBitField
 * @brief One byte of a coil or discrete input bitfield. An array of them holds 8 bits per byte.
 */
typedef uint8_t PifModbusBitField;

/**
 * @class PifModbusBitFieldP
 * @brief Pointer to a coil or discrete input bitfield.
 */
typedef uint8_t* PifModbusBitFieldP;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbus_StreamToShort
 * @brief Converts data between wire format and native format.
 * @param p_stream Pointer to a Modbus network-order byte stream.
 * @return Computed or decoded 16-bit value.
 */
uint16_t pifModbus_StreamToShort(uint8_t *p_stream);

/**
 * @fn pifModbus_ShortToStream
 * @brief Converts data between wire format and native format.
 * @param value Value to write.
 * @param p_stream Pointer to a Modbus network-order byte stream.
 */
void pifModbus_ShortToStream(uint16_t value, uint8_t *p_stream);

/**
 * @fn pifModbus_CheckResponse
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
