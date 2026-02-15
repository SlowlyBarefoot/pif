#ifndef PIF_MODBUS_H
#define PIF_MODBUS_H


#include "core/pif.h"


// Timeout used after sending one packet while waiting for a response.
// This value is multiplied by the timer unit configured in pifModbus[Rtu/Ascii]Master_Init().
// Default is 500 ticks, which equals 500 ms when the timer unit is 1 ms.
#ifndef PIF_MODBUS_MASTER_TIMEOUT
	#define PIF_MODBUS_MASTER_TIMEOUT		500
#endif

// Timeout used to receive one complete packet.
// This value is multiplied by the timer unit configured in pifModbus[Rtu/Ascii]Slave_Init().
// Default is 300 ticks, which equals 300 ms when the timer unit is 1 ms.
#ifndef PIF_MODBUS_SLAVE_TIMEOUT
	#define PIF_MODBUS_SLAVE_TIMEOUT		300
#endif

/**
 * Read a bit from the nmbs_bitfield bf at position b
 */
#define PIF_MODBUS_READ_BIT_FIELD(bf, b)		((BOOL) (((bf)[(b) / 8] >> ((b) % 8)) & 1))

/**
 * Write value v to the nmbs_bitfield bf at position b
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
 * nanoMODBUS errors.
 * Values <= 0 are library errors, > 0 are modbus exceptions.
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
 * @brief Bitfield consisting of 2000 coils/discrete inputs
 */
typedef uint8_t PifModbusBitField;

/**
 * @class PifModbusBitField
 * @brief Bitfield consisting of 2000 coils/discrete inputs
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

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_H
