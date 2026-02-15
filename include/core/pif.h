#ifndef PIF_H
#define PIF_H


#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pif_conf.h"


#define PIF_VERSION_MAJOR	0
#define PIF_VERSION_MINOR	1
#define PIF_VERSION_PATCH	0

#ifndef PIF_WEAK
#define PIF_WEAK __attribute__ ((weak))
#endif

#ifndef BOOL
#define BOOL   unsigned char
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE   1
#endif

#ifndef SWITCH
#define SWITCH  unsigned char
#endif

#ifndef OFF
#define OFF     0
#endif

#ifndef ON
#define ON      1
#endif

#define PIF_PI		3.14159265358979323846f

#define PIF_RAD		(PIF_PI / 180.0f)

#define PIF_ID_AUTO			0x0000
#define PIF_ID_USER(N)		(0x0100 + (N))

#define PIF_CHECK_ELAPSE_TIME_1MS(START, ELAPSE)	(pif_cumulative_timer1ms - (START) >= (ELAPSE))
#define PIF_CHECK_ELAPSE_TIME_1US(START, ELAPSE)	((*pif_act_timer1us)() - (START) >= (ELAPSE))

#define REG_VALUE(V, SM)			((V) << ((SM) >> 8))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) > 0 ? (x) : -(x))

#define _PIF_CONCAT2(x, y) 		x ## y
#define PIF_CONCAT2(x, y) 		_PIF_CONCAT2(x, y)

#define _PIF_CONCAT3(x, y, z) 	x ## y ## z
#define PIF_CONCAT3(x, y, z) 	_PIF_CONCAT3(x, y, z)

#define RESET_BIT_FILED(V, M)	(V) = (V) & ~(M)
#define SET_BIT_FILED(V, M, D)	(V) = ((V) & ~(M)) | (D)


typedef uint16_t PifId;
typedef void* PifIssuerP;


typedef enum EnPifError
{
    E_SUCCESS           		= 0x00,

	E_INVALID_PARAM     		= 0x01,
    E_INVALID_STATE     		= 0x02,
    E_OUT_OF_HEAP       		= 0x03,
    E_OVERFLOW_BUFFER			= 0x04,
	E_EMPTY_IN_BUFFER			= 0x05,
	E_WRONG_DATA				= 0x06,
	E_TIMEOUT					= 0x07,
	E_NOT_SET_EVENT				= 0x08,
	E_CANNOT_USE				= 0x09,
	E_TRANSFER_FAILED			= 0x0A,
	E_NOT_SET_TASK				= 0x0B,
	E_MISMATCH_CRC				= 0x0C,
	E_ACCESS_FAILED				= 0x0D,
	E_CANNOT_FOUND				= 0x0E,
	E_IS_NOT_FORMATED			= 0x0F,
	E_RECEIVE_NACK				= 0x10,
	E_INVALID_ID				= 0x11,
	E_ALREADY_ATTACHED			= 0x12
} PifError;

typedef enum EnPifPulseState
{
    PS_LOW_LEVEL	= 0,
    PS_FALLING_EDGE	= 1,
    PS_RISING_EDGE	= 2,
    PS_HIGH_LEVEL	= 3,

    PS_LEVEL_MASK	= 1,
    PS_EDGE_MASK	= 2
} PifPulseState;

/**
 * @struct StPifDateTime
 * @brief Represents the date time data structure used by this module.
 */
typedef struct StPifDateTime
{
    uint8_t year;				// As of 2000
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
} PifDateTime;

typedef struct StPifPerformance
{
	// Public Member Variable

	// Read-only Member Variable
	volatile uint32_t _count;
	uint8_t _use_rate;

	// Private Member Variable
	uint8_t __state;	// 0 : nomal, 1 : 1 ms, 2 : 1 second, 4: 1 minute
#ifdef PIF_DEBUG
	uint32_t __max_loop_time1us;
#endif
} PifPerformance;

typedef uint32_t (*PifActTimer1us)();

typedef SWITCH (*PifActGpioRead)(uint16_t port);
typedef void (*PifActGpioWrite)(uint16_t port, SWITCH state);

extern PifError pif_error;

extern uint32_t pif_timer1us;
extern volatile uint16_t pif_timer1ms;
extern volatile uint32_t pif_timer1sec;
extern volatile PifDateTime pif_datetime;

extern volatile uint32_t pif_cumulative_timer1ms;

extern PifPerformance pif_performance;

extern PifId pif_id;

extern PifActTimer1us pif_act_timer1us;

extern PifActGpioRead pif_act_gpio_read;
extern PifActGpioWrite pif_act_gpio_write;

extern const char* kPifMonth3[12];

extern const char* kPifHexUpperChar;
extern const char* kPifHexLowerChar;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pif_Init
 * @brief Initializes the core instance and prepares all internal fields for safe use.
 * @param act_timer1us Callback that returns the current 1us tick count.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pif_Init(PifActTimer1us act_timer1us);

/**
 * @fn pif_Exit
 * @brief Executes the pif_Exit operation for the core module according to the API contract.
 */
void pif_Exit();

/**
 * @fn pif_sigTimer1ms
 * @brief Processes an external signal or tick for the core and updates runtime timing state.
 */
void pif_sigTimer1ms();

/**
 * @fn pif_Delay1ms
 * @brief Performs a delay operation in the core context for the requested time interval.
 * @param delay Delay duration value.
 */
void pif_Delay1ms(uint16_t delay);

/**
 * @fn pif_Delay1us
 * @brief Performs a delay operation in the core context for the requested time interval.
 * @param delay Delay duration value.
 */
void pif_Delay1us(uint16_t delay);

/**
 * @fn pif_ChangeStatusLed
 * @brief Changes runtime configuration of the core while preserving object ownership semantics.
 * @param num Index number of the status LED to control.
 * @param state Target state value to apply.
 */
void PIF_WEAK pif_ChangeStatusLed(int num, BOOL state);

/**
 * @fn pif_ToggleStatusLed
 * @brief Executes the pif_ToggleStatusLed operation for the core module according to the API contract.
 * @param num Index number of the status LED to control.
 */
void PIF_WEAK pif_ToggleStatusLed(int num);

/**
 * @fn pif_ClearError
 * @brief Executes the pif_ClearError operation for the core module according to the API contract.
 */
void pif_ClearError();

/**
 * @fn pif_BinToString
 * @brief Executes the pif_BinToString operation for the core module according to the API contract.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param value Input numeric value to convert or process.
 * @param str_cnt Maximum number of characters to write to the output string.
 * @return Result value returned by this API.
 */
int pif_BinToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @fn pif_DecToString
 * @brief Executes the pif_DecToString operation for the core module according to the API contract.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param value Input numeric value to convert or process.
 * @param str_cnt Maximum number of characters to write to the output string.
 * @return Result value returned by this API.
 */
int pif_DecToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @fn pif_HexToString
 * @brief Executes the pif_HexToString operation for the core module according to the API contract.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param value Input numeric value to convert or process.
 * @param str_cnt Maximum number of characters to write to the output string.
 * @param upper Set to TRUE to use uppercase alphabetic digits.
 * @return Result value returned by this API.
 */
int pif_HexToString(char* p_buffer, uint32_t value, uint16_t str_cnt, BOOL upper);

/**
 * @fn pif_FloatToString
 * @brief Executes the pif_FloatToString operation for the core module according to the API contract.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param value Input numeric value to convert or process.
 * @param point Number of digits to place after the decimal point.
 * @return Result value returned by this API.
 */
int pif_FloatToString(char* p_buffer, double value, uint16_t point);

/**
 * @fn pif_PrintFormat
 * @brief Formats and writes output related to the core using the provided destination.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param p_data Pointer to input or output data buffer.
 * @param buffer_size Size of the destination buffer in bytes.
 * @param p_format Format string used for text generation.
 */
void pif_PrintFormat(char* p_buffer, size_t buffer_size, va_list* data, const char* p_format);

/**
 * @fn pif_Printf
 * @brief Formats and writes output related to the core using the provided destination.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param buffer_size Size of the destination buffer in bytes.
 * @param p_format Format string used for text generation.
 */
void pif_Printf(char* p_buffer, size_t buffer_size, const char* p_format, ...);

/**
 * @fn pifCrc7_Add
 * @brief Adds an item to the crc7 and updates internal bookkeeping for subsequent operations.
 * @param crc Current CRC accumulator value.
 * @param data Input byte value used in CRC accumulation.
 * @return Return value of this API.
 */
uint8_t pifCrc7_Add(uint8_t crc, uint8_t data);

/**
 * @fn pifCrc7_Result
 * @brief Executes the pifCrc7_Result operation for the crc7 module according to the API contract.
 * @param crc Current CRC accumulator value.
 * @return Return value of this API.
 */
uint8_t pifCrc7_Result(uint8_t crc);

/**
 * @fn pifCrc7
 * @brief Executes the pifCrc7 operation for the crc7 module according to the API contract.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Return value of this API.
 */
uint8_t pifCrc7(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCrc16_Add
 * @brief Adds an item to the crc16 and updates internal bookkeeping for subsequent operations.
 * @param crc Current CRC accumulator value.
 * @param data Input byte value used in CRC accumulation.
 * @return Return value of this API.
 */
uint16_t pifCrc16_Add(uint16_t crc, uint8_t data);

/**
 * @fn pifCrc16
 * @brief Executes the pifCrc16 operation for the crc16 module according to the API contract.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Return value of this API.
 */
uint16_t pifCrc16(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCheckSum
 * @brief Executes the pifCheckSum operation for the check sum module according to the API contract.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Return value of this API.
 */
uint32_t pifCheckSum(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCheckXor
 * @brief Executes the pifCheckXor operation for the check xor module according to the API contract.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Return value of this API.
 */
uint8_t pifCheckXor(uint8_t* p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
