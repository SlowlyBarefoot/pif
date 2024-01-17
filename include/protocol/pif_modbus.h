#ifndef PIF_MODBUS_H
#define PIF_MODBUS_H


#include "core/pif.h"


// 한 packet을 보내고 응답을 받는 시간 제한
// pifModbus[Rtu/Ascii]Master_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 500이고 타이머 단위가 1ms이기에 500 * 1ms = 500ms이다.
#ifndef PIF_MODBUS_MASTER_TIMEOUT
	#define PIF_MODBUS_MASTER_TIMEOUT		500
#endif

// 한 packet을 전부 받는 시간 제한
// pifModbus[Rtu/Ascii]Slave_Init에서 받은 타이머의 단위를 곱한 시간
// 기본값은 300이고 타이머 단위가 1ms이기에 300 * 1ms = 300ms이다.
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
 * @brief
 * @param p_stream
 * @return
 */
uint16_t pifModbus_StreamToShort(uint8_t *p_stream);

/**
 * @fn pifModbus_ShortToStream
 * @brief
 * @param value
 * @param p_stream
 */
void pifModbus_ShortToStream(uint16_t value, uint8_t *p_stream);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_H
