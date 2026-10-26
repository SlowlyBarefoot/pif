// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif.h
 * @brief Core of PIF: basic types, error codes, the time base and the platform hooks every module uses.
 *
 * Call pif_Init() with a 1 us timer at startup and pif_sigTimer1ms() every millisecond. The time counters,
 * pif_error and the hooks declared here are shared by all modules.
 */
#ifndef PIF_H
#define PIF_H


#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pif_conf.h"


#define PIF_VERSION_MAJOR	0	///< Major version of PIF
#define PIF_VERSION_MINOR	1	///< Minor version of PIF
#define PIF_VERSION_PATCH	0	///< Patch version of PIF

#ifndef PIF_WEAK
#define PIF_WEAK __attribute__ ((weak))	///< Marks a function the application may replace. Define it in pif_conf.h for a compiler without __attribute__((weak)).
#endif

// Short accessors are defined in the headers with this so they can be inlined across files.
// static keeps every compiler free of duplicate symbols even when it does not inline.
#ifndef PIF_STATIC_INLINE
	#if defined(__cplusplus) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
		#define PIF_STATIC_INLINE	static inline
	#elif defined(__GNUC__)
		#define PIF_STATIC_INLINE	static __inline__
	#elif defined(_MSC_VER) || defined(__CC_ARM) || defined(__ICCARM__)
		#define PIF_STATIC_INLINE	static __inline
	#else
		#define PIF_STATIC_INLINE	static	///< Storage class of the short accessors defined in the headers
	#endif
#endif

#ifndef BOOL
#define BOOL   unsigned char	///< Boolean type, TRUE or FALSE. Define it before pif.h to use another type.
#endif

#ifndef FALSE
#define FALSE   0	///< Boolean false
#endif

#ifndef TRUE
#define TRUE   1	///< Boolean true
#endif

#ifndef SWITCH
#define SWITCH  unsigned char	///< ON or OFF state of a switch or a pin
#endif

#ifndef OFF
#define OFF     0	///< Switch off
#endif

#ifndef ON
#define ON      1	///< Switch on
#endif

#define PIF_PI		3.14159265358979323846f	///< Pi as a float

#define PIF_RAD		(PIF_PI / 180.0f)	///< Radians in one degree

#define PIF_ID_AUTO			0x0000	///< Lets the module take the next free ID from pif_id
#define PIF_ID_USER(N)		(0x0100 + (N))	///< ID number N chosen by the application, apart from the automatic ones

#define PIF_CHECK_ELAPSE_TIME_1MS(START, ELAPSE)	(pif_cumulative_timer1ms - (START) >= (ELAPSE))	///< TRUE when ELAPSE milliseconds have passed since START, a value of pif_cumulative_timer1ms
#define PIF_CHECK_ELAPSE_TIME_1US(START, ELAPSE)	((*pif_act_timer1us)() - (START) >= (ELAPSE))	///< TRUE when ELAPSE microseconds have passed since START, a value of pif_act_timer1us

#define REG_VALUE(V, SM)			((V) << ((SM) >> 8))	///< Shifts V into the place of a register field. SM holds the shift in its high byte and the mask in its low byte.

// Left alone when the host project has already defined them. These evaluate
// their arguments twice, so a host that offers safer ones - Betaflight's
// common/maths.h wraps them in a statement expression with __typeof__ - must
// win wherever both are in scope. PIF only needs them to be defined; it does
// not need them to be its own.
/** @name MIN, MAX and ABS
 *  Left alone when the host project has already defined them. They evaluate their arguments twice.
 *  @{
 */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))	///< Smaller of a and b
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))	///< Larger of a and b
#endif
#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))	///< Absolute value of x
#endif
/** @} */

#define _PIF_CONCAT2(x, y) 		x ## y	///< Helper of PIF_CONCAT2
#define PIF_CONCAT2(x, y) 		_PIF_CONCAT2(x, y)	///< Joins two tokens after expanding them

#define _PIF_CONCAT3(x, y, z) 	x ## y ## z	///< Helper of PIF_CONCAT3
#define PIF_CONCAT3(x, y, z) 	_PIF_CONCAT3(x, y, z)	///< Joins three tokens after expanding them

#define RESET_BIT_FILED(V, M)	(V) = (V) & ~(M)	///< Clears the bits M of V
#define SET_BIT_FILED(V, M, D)	(V) = ((V) & ~(M)) | (D)	///< Replaces the bits M of V with D, which must be in place

/// CRC-8 polynomials for pifCrc8_Add() (MSB first, no reflection).
#define PIF_CRC8_POLY_DVB_S2	0xD5	///< MSPv2, CRSF, GHST
#define PIF_CRC8_POLY_BA		0xBA	///< CRSF command frames


/**
 * @brief ID of an object or a task, unique in the application.
 *
 * PIF_ID_AUTO lets the module take the next free one.
 */
typedef uint16_t PifId;
/**
 * @brief Opaque pointer handed back to a callback, usually the object that registered it.
 */
typedef void* PifIssuerP;


/**
 * @brief Error codes. The functions that fail set pif_error to one of them.
 */
typedef enum EnPifError
{
    E_SUCCESS           		= 0x00,	///< No error

	E_INVALID_PARAM     		= 0x01,	///< A parameter is out of range
    E_INVALID_STATE     		= 0x02,	///< The object is not in a state that allows the call
    E_OUT_OF_HEAP       		= 0x03,	///< Memory allocation failed
    E_OVERFLOW_BUFFER			= 0x04,	///< A buffer has no room left
	E_EMPTY_IN_BUFFER			= 0x05,	///< A buffer holds no data
	E_WRONG_DATA				= 0x06,	///< The data is malformed
	E_TIMEOUT					= 0x07,	///< The operation timed out
	E_NOT_SET_EVENT				= 0x08,	///< A required event callback is not set
	E_CANNOT_USE				= 0x09,	///< The function cannot be used in this configuration
	E_TRANSFER_FAILED			= 0x0A,	///< A transfer on a bus failed
	E_NOT_SET_TASK				= 0x0B,	///< A required task is not set
	E_MISMATCH_CRC				= 0x0C,	///< CRC check failed
	E_ACCESS_FAILED				= 0x0D,	///< Access to a device or memory failed
	E_CANNOT_FOUND				= 0x0E,	///< The item was not found
	E_IS_NOT_FORMATED			= 0x0F,	///< The storage is not formatted
	E_RECEIVE_NACK				= 0x10,	///< The device answered with NACK
	E_INVALID_ID				= 0x11,	///< The ID is not valid
	E_ALREADY_ATTACHED			= 0x12	///< Already attached to something else
} PifError;

/**
 * @brief Level and edge of a sampled signal.
 *
 * Bit 0 is the level, bit 1 tells that it changed.
 */
typedef enum EnPifPulseState
{
    PS_LOW_LEVEL	= 0,	///< Low, no change
    PS_FALLING_EDGE	= 1,	///< Changed to low
    PS_RISING_EDGE	= 2,	///< Changed to high
    PS_HIGH_LEVEL	= 3,	///< High, no change

    PS_LEVEL_MASK	= 1,	///< Bit of the level
    PS_EDGE_MASK	= 2	///< Bit of the edge
} PifPulseState;

/**
 * @struct StPifDateTime
 * @brief Date and time kept by pif_sigTimer1ms().
 */
typedef struct StPifDateTime
{
    uint8_t year;				///< As of 2000
    uint8_t month;	///< 1 to 12
    uint8_t day;	///< 1 to 31
    uint8_t hour;	///< 0 to 23
    uint8_t minute;	///< 0 to 59
    uint8_t second;	///< 0 to 59
    uint16_t millisecond;	///< 0 to 999
} PifDateTime;
/**
 * @brief Load and timing of the main loop, measured by the task manager.
 */

typedef struct StPifPerformance
{
	// Public Member Variable

	// Read-only Member Variable
	volatile uint32_t _count;	///< Passes of the main loop in the current second
	uint32_t _task_time1us;     ///< Accumulated task time in the current CPU load measurement window.
	uint32_t _max_delay;		///< Longest delay in microseconds from the release to the dispatch
	uint32_t _miss_count;		///< Number of releases that were delayed by more than one period
	uint8_t _task_load;         ///< Ratio of the last measurement window spent in tasks and in the
								///< timer and idle callbacks, as a percentage. Scheduling is not
								///< a task and is excluded, so a saturated system stays a little
								///< under 100%.

	// Private Member Variable
	uint8_t __state;	///< 0 : nomal, 1 : 1 ms, 2 : 1 second, 4: 1 minute
#ifdef PIF_DEBUG
	uint32_t __max_loop_time1us;
#endif
} PifPerformance;

/**
 * @brief Returns a free-running 1 us counter of the platform. It wraps around at 2^32.
 */
typedef uint32_t (*PifActTimer1us)();

/**
 * @brief Reads the state of a GPIO port.
 */
typedef SWITCH (*PifActGpioRead)(uint16_t port);
/**
 * @brief Writes the state of a GPIO port.
 */
typedef void (*PifActGpioWrite)(uint16_t port, SWITCH state);

/**
 * @brief Disables the interrupts of the platform, to begin a section that an interrupt must not enter.
 * @return Interrupt state before the call, such as PRIMASK on Cortex-M or SREG on AVR, for PifActIrqRestore.
 */
typedef uint32_t (*PifActIrqDisable)();
/**
 * @brief Puts the interrupts of the platform back in the state PifActIrqDisable returned, to end the section.
 *        They stay disabled when they already were, so sections may nest and may run inside an interrupt.
 * @param state Value returned by the PifActIrqDisable call that began the section.
 */
typedef void (*PifActIrqRestore)(uint32_t state);

extern PifError pif_error;	///< Error of the last function that failed

/// TRUE from pif_Init() until pif_Exit(). Until then pif_sigTimer1ms() and pifTaskManager_Loop() return at once,
/// so a timer interrupt or a main loop that starts before the setup is done needs no guard of its own.
extern volatile BOOL pif_ready;

extern uint32_t pif_timer1us;	///< Time in us at the start of the current task run, from pif_act_timer1us
extern volatile uint16_t pif_timer1ms;	///< Millisecond in the current second, 0 to 999
extern volatile uint32_t pif_timer1sec;	///< Seconds since pif_Init()
extern volatile PifDateTime pif_datetime;	///< Date and time kept by pif_sigTimer1ms(), set with pif_SetDateTime()

extern volatile uint32_t pif_cumulative_timer1ms;	///< Milliseconds since pif_Init(). Wraps around after about 49 days.

extern PifPerformance pif_performance;	///< Load and timing of the main loop

extern PifId pif_id;	///< Next ID given for PIF_ID_AUTO

extern PifActTimer1us pif_act_timer1us;	///< 1 us timer given to pif_Init()

extern PifActGpioRead pif_act_gpio_read;	///< Optional GPIO read for debugging. Some modules use it with their ID as the port.
extern PifActGpioWrite pif_act_gpio_write;	///< Optional GPIO write for debugging. Some modules toggle their ID as the port to show timing.

/// Optional interrupt disable of the platform. NULL leaves a section unguarded. Set it together with
/// pif_act_irq_restore. Modules call the pair through pif_IrqDisable() and pif_IrqRestore().
extern PifActIrqDisable pif_act_irq_disable;
extern PifActIrqRestore pif_act_irq_restore;	///< Optional interrupt restore of the platform, paired with pif_act_irq_disable.

extern const char* kPifMonth3[12];	///< Three letter English names of the months

extern const char* kPifHexUpperChar;	///< "0123456789ABCDEF"
extern const char* kPifHexLowerChar;	///< "0123456789abcdef"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts PIF with the 1 us timer of the platform. Call it once at startup, before any other module.
 * @param act_timer1us Callback that returns the current 1us tick count.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when act_timer1us is NULL.
 */
BOOL pif_Init(PifActTimer1us act_timer1us);

/**
 * @brief Ends PIF: clears pif_ready, then removes every task, timer and idle callback from the task manager.
 */
void pif_Exit();

/**
 * @brief Advances the time base by 1 ms: pif_cumulative_timer1ms, pif_timer1ms, pif_timer1sec and pif_datetime.
 *        Call it every millisecond, from a timer interrupt for example. Does nothing while pif_ready is FALSE.
 */
void pif_sigTimer1ms();

/**
 * @brief Waits for the requested time with the CPU held. Nothing else runs meanwhile, so the whole
 *        wait is added to the block time of whichever task is executing and delays a TM_REALTIME
 *        release by that much. Keep it to initialization and to waits a device cannot be asked
 *        about; a wait a task can be released from again belongs in the return value of its loop
 *        callback instead.
 * @param delay Delay duration in milliseconds.
 */
void pif_Delay1ms(uint32_t delay);

/**
 * @brief Waits for the requested time with the CPU held, as pif_Delay1ms() does.
 * @param delay Delay duration in microseconds.
 */
void pif_Delay1us(uint32_t delay);

/**
 * @brief Turns a status LED on or off. A weak function that does nothing: the application overrides it to drive
 *        its LEDs.
 * @param num Number of the status LED.
 * @param state TRUE to turn it on, FALSE to turn it off.
 */
void PIF_WEAK pif_ChangeStatusLed(int num, BOOL state);

/**
 * @brief Toggles a status LED. A weak function that does nothing: the application overrides it to drive its
 *        LEDs.
 * @param num Number of the status LED.
 */
void PIF_WEAK pif_ToggleStatusLed(int num);

/**
 * @brief Sets pif_error back to E_SUCCESS.
 */
void pif_ClearError();

/**
 * @brief Begins a section that an interrupt must not enter, with pif_act_irq_disable when it is set.
 * @return Interrupt state to pass to pif_IrqRestore(), or 0 when pif_act_irq_disable is NULL.
 */
PIF_STATIC_INLINE uint32_t pif_IrqDisable()
{
	return pif_act_irq_disable ? (*pif_act_irq_disable)() : 0;
}

/**
 * @brief Ends the section begun by pif_IrqDisable(), with pif_act_irq_restore when it is set.
 * @param state Value returned by the pif_IrqDisable() call that began the section.
 */
PIF_STATIC_INLINE void pif_IrqRestore(uint32_t state)
{
	if (pif_act_irq_restore) (*pif_act_irq_restore)(state);
}

/**
 * @brief Writes value as binary digits. No NUL is added.
 * @param p_buffer Destination, large enough for the digits.
 * @param value Value to write.
 * @param str_cnt Number of low bits to write, or 0 for the digits without leading zeros.
 * @return Number of characters written.
 */
int pif_BinToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @brief Writes value as decimal digits. No NUL is added.
 * @param p_buffer Destination, large enough for the digits.
 * @param value Value to write.
 * @param str_cnt Smallest number of digits, padded with leading zeros, or 0 for no padding.
 * @return Number of characters written.
 */
int pif_DecToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @brief Writes value as hexadecimal digits. No NUL is added.
 * @param p_buffer Destination, large enough for the digits.
 * @param value Value to write.
 * @param str_cnt Smallest number of digits, padded with leading zeros, or 0 for no padding.
 * @param upper TRUE for A to F, FALSE for a to f.
 * @return Number of characters written.
 */
int pif_HexToString(char* p_buffer, uint32_t value, uint16_t str_cnt, BOOL upper);

/**
 * @brief Writes value as a decimal number with a fraction, truncated. No NUL is added.
 * @param p_buffer Destination, large enough for the number.
 * @param value Value to write.
 * @param point Digits after the decimal point, 0 for 6.
 * @return Number of characters written.
 */
int pif_FloatToString(char* p_buffer, double value, uint16_t point);

/**
 * @brief Compares two strings without regard to the case of the ASCII letters, as the POSIX strcasecmp()
 *        does, which not every C library has.
 * @param p_str1 First string.
 * @param p_str2 Second string.
 * @return 0 if they match, otherwise a value whose sign is that of the first difference.
 */
int pif_StrCaseCmp(const char* p_str1, const char* p_str2);

/**
 * @brief Compares at most n characters of two strings without regard to the case of the ASCII letters,
 *        as the POSIX strncasecmp() does.
 * @param p_str1 First string.
 * @param p_str2 Second string.
 * @param n Most characters to compare.
 * @return 0 if they match, otherwise a value whose sign is that of the first difference.
 */
int pif_StrNCaseCmp(const char* p_str1, const char* p_str2, size_t n);

/**
 * @brief Formats a string like vsnprintf with the conversions %d, %u, %x, %X, %b, %f, %s, %c and %%. A width
 *        sets the zero padded digits, or the digits after the point for %f, and l takes a long. The result always ends
 *        with NUL. %f is written only with PIF_USE_PRINTF_FLOAT; without it %f takes its argument and writes
 *        nothing.
 * @param p_buffer Destination.
 * @param buffer_size Size of p_buffer in bytes, the NUL included. What does not fit is dropped.
 * @param p_data Arguments of the format.
 * @param p_format Format string.
 */
void pif_PrintFormat(char* p_buffer, size_t buffer_size, va_list* p_data, const char* p_format);

/**
 * @brief Formats a string like snprintf, with the conversions of pif_PrintFormat().
 * @param p_buffer Destination.
 * @param buffer_size Size of p_buffer in bytes, the NUL included. What does not fit is dropped.
 * @param p_format Format string, followed by its arguments.
 */
void pif_Printf(char* p_buffer, size_t buffer_size, const char* p_format, ...);

/**
 * @brief Adds one byte to a CRC-7 (polynomial 0x09, as SD cards use it). Start with 0.
 * @param crc CRC so far.
 * @param data Byte to add.
 * @return New CRC.
 */
uint8_t pifCrc7_Add(uint8_t crc, uint8_t data);

/**
 * @brief Turns a CRC-7 into the byte that is sent: the CRC shifted left with the end bit set.
 * @param crc CRC from pifCrc7_Add().
 * @return Byte to send.
 */
uint8_t pifCrc7_Result(uint8_t crc);

/**
 * @brief Calculates the CRC-7 byte of a block, as pifCrc7_Result() gives it.
 * @param p_data Data.
 * @param length Number of bytes.
 * @return Byte to send.
 */
uint8_t pifCrc7(uint8_t* p_data, uint16_t length);

/**
 * @brief Adds one byte to a CRC-16/XMODEM (polynomial 0x1021, MSB first). Start with 0.
 * @param crc CRC so far.
 * @param data Byte to add.
 * @return New CRC.
 */
uint16_t pifCrc16_Add(uint16_t crc, uint8_t data);

/**
 * @brief Calculates the CRC-16/XMODEM of a block, starting from 0.
 * @param p_data Data.
 * @param length Number of bytes.
 * @return CRC.
 */
uint16_t pifCrc16(uint8_t* p_data, uint16_t length);

/**
 * @brief Adds a block of bytes to a CRC-16/CCITT (polynomial 0x1021, MSB first).
 *
 * pifCrc16() is this function started from 0 (CRC-16/XMODEM). Start from 0xFFFF
 * for CRC-16/CCITT-FALSE.
 * @param crc Current CRC accumulator value.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Updated CRC accumulator value.
 */
uint16_t pifCrc16_Update(uint16_t crc, const uint8_t* p_data, uint16_t length);

/**
 * @brief Adds one byte to a CRC-8 with the given polynomial (MSB first, no reflection, no final XOR).
 * @param crc Current CRC accumulator value.
 * @param data Input byte value used in CRC accumulation.
 * @param poly CRC polynomial without the x^8 term, for example PIF_CRC8_POLY_DVB_S2.
 * @return Updated CRC accumulator value.
 */
uint8_t pifCrc8_Add(uint8_t crc, uint8_t data, uint8_t poly);

/**
 * @brief Adds a block of bytes to a CRC-8 with the given polynomial.
 * @param crc Current CRC accumulator value.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @param poly CRC polynomial without the x^8 term, for example PIF_CRC8_POLY_DVB_S2.
 * @return Updated CRC accumulator value.
 */
uint8_t pifCrc8_Update(uint8_t crc, const uint8_t* p_data, uint16_t length, uint8_t poly);

/**
 * @brief Calculates a CRC-8 with the given polynomial over a block of bytes, starting from 0.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @param poly CRC polynomial without the x^8 term, for example PIF_CRC8_POLY_DVB_S2.
 * @return CRC of the block.
 */
uint8_t pifCrc8(const uint8_t* p_data, uint16_t length, uint8_t poly);

/**
 * @brief Adds a block of bytes to a CRC-32 (IEEE 802.3: reflected polynomial 0xEDB88320, initial value and
 *        final XOR 0xFFFFFFFF), the CRC of Ethernet, zlib, PNG and ZIP.
 *
 * As with zlib's crc32(), crc is a finished CRC: start with 0 and pass each result on to continue, so
 * pifCrc32_Update(pifCrc32_Update(0, a, n), b, m) is the CRC of a followed by b. It is weak, so a platform
 * with a CRC unit can replace it.
 * @param crc CRC of the bytes before, or 0.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return CRC of everything so far.
 */
uint32_t pifCrc32_Update(uint32_t crc, const uint8_t* p_data, uint32_t length);

/**
 * @brief Calculates the CRC-32 (IEEE 802.3) of a block of bytes.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return CRC of the block.
 */
uint32_t pifCrc32(const uint8_t* p_data, uint32_t length);

/**
 * @brief Adds a block of bytes to an XOR checksum.
 * @param val Current XOR accumulator value.
 * @param p_data Pointer to the data buffer used by this operation.
 * @param length Number of bytes to process.
 * @return Updated XOR accumulator value.
 */
uint8_t pifCheckXor_Update(uint8_t val, const uint8_t* p_data, uint16_t length);

/**
 * @brief Adds the bytes of a block.
 * @param p_data Data.
 * @param length Number of bytes.
 * @return Sum of the bytes.
 */
uint32_t pifCheckSum(uint8_t* p_data, uint16_t length);

/**
 * @brief XORs the bytes of a block.
 * @param p_data Data.
 * @param length Number of bytes.
 * @return XOR of the bytes.
 */
uint8_t pifCheckXor(uint8_t* p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
