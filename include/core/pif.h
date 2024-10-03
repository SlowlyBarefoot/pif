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
#define PIF_ID_USER			0x0100

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
 * @brief 날짜 시간 정보
 */
typedef struct StPifDateTime
{
    uint8_t year;				// 2000년 기준
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t millisecond;
} PifDateTime;

/**
 * @struct StPifPidControl
 * @brief PID Control을 계산하기 위한 구조체
 */
typedef struct StPifPidControl
{
	float kp;				// Proportional gain
	float ki;				// Integral gain
	float kd; 		    	// Derivative gain
    float max_integration;	// Maximum Integration

	float err_sum;		    // Variable: Error Sum
	float err_prev;	   		// History: Previous error
} PifPidControl;

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
 * @brief pif의 전역 변수를 초기화한다.
 * @param act_timer1us
 * @return
 */
BOOL pif_Init(PifActTimer1us act_timer1us);

/**
 * @fn pif_Exit
 * @brief
 */
void pif_Exit();

/**
 * @fn pif_sigTimer1ms
 * @brief 1ms 타이머 인터럽트에서 실행할 pif 함수이다.
 */
void pif_sigTimer1ms();

/**
 * @fn pif_Delay1ms
 * @brief
 * @param delay
 */
void pif_Delay1ms(uint16_t delay);

/**
 * @fn pif_Delay1us
 * @brief
 * @param delay
 */
void pif_Delay1us(uint16_t delay);

/**
 * @fn pif_ChangeStatusLed
 * @brief 
 * @param num
 * @param state
 */
void PIF_WEAK pif_ChangeStatusLed(int num, BOOL state);

/**
 * @fn pif_ToggleStatusLed
 * @brief 
 * @param num
 */
void PIF_WEAK pif_ToggleStatusLed(int num);

/**
 * @fn pif_ClearError
 * @brief Error를 정리하다.
 */
void pif_ClearError();

/**
 * @fn pif_BinToString
 * @brief
 * @param p_buffer
 * @param value
 * @param str_cnt
 * @return
 */
int pif_BinToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @fn pif_DecToString
 * @brief
 * @param p_buffer
 * @param value
 * @param str_cnt
 * @return
 */
int pif_DecToString(char* p_buffer, uint32_t value, uint16_t str_cnt);

/**
 * @fn pif_HexToString
 * @brief
 * @param p_buffer
 * @param value
 * @param str_cnt
 * @param upper
 * @return
 */
int pif_HexToString(char* p_buffer, uint32_t value, uint16_t str_cnt, BOOL upper);

/**
 * @fn pif_FloatToString
 * @brief
 * @param p_buffer
 * @param value
 * @param point
 * @return
 */
int pif_FloatToString(char* p_buffer, double value, uint16_t point);

/**
 * @fn pif_PrintFormat
 * @brief
 * @param p_buffer
 * @param p_data
 * @param buffer_size
 * @param p_format
 */
void pif_PrintFormat(char* p_buffer, size_t buffer_size, va_list* data, const char* p_format);

/**
 * @fn pif_Printf
 * @brief
 * @param p_buffer
 * @param buffer_size
 * @param p_format
 */
void pif_Printf(char* p_buffer, size_t buffer_size, const char* p_format, ...);

/**
 * @fn pifCrc7_Add
 * @brief 이전 7비트 CRC값에 데이타를 추가한다.
 * @param crc 7비트 CRC의 이전값.
 * @param data 7비트 CRC할 데이터.
 * @return 데이타를 추가한 7비트 CRC값.
 */
uint8_t pifCrc7_Add(uint8_t crc, uint8_t data);

/**
 * @fn pifCrc7_Result
 * @brief 계산된 7비트 CRC 결과를 반환한다.
 * @param crc 7비트 CRC의 이전값.
 * @return 7비트 CRC 결과값.
 */
uint8_t pifCrc7_Result(uint8_t crc);

/**
 * @fn pifCrc7
 * @brief Buffer에 있는 데이타의 7비트 CRC 결과를 반환한다.
 * @param p_data 7비트 CRC할 데이터의 시작 포인터.
 * @param length 7비트 CRC할 데이터의 크기.
 * @return 7비트 CRC 결과값.
 */
uint8_t pifCrc7(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCrc16_Add
 * @brief 이전 16비트 CRC값에 데이타를 추가한다.
 * @param crc 16비트 CRC의 이전값.
 * @param data 16비트 CRC할 데이터.
 * @return 데이타를 추가한 16비트 CRC값.
 */
uint16_t pifCrc16_Add(uint16_t crc, uint8_t data);

/**
 * @fn pifCrc16
 * @brief Buffer에 있는 데이타의 16비트 CRC 결과를 반환한다.
 * @param p_data 16비트 CRC할 데이터의 시작 포인터.
 * @param length 16비트 CRC할 데이터의 크기.
 * @return 데이타를 추가한 16비트 CRC값.
 */
uint16_t pifCrc16(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCheckSum
 * @brief Buffer에서의 byte 단위로 합산한 결과를 반환한다.
 * @param p_data Checksum할 데이터의 시작 포인터.
 * @param length Checksum할 데이터의 크기.
 * @return 데이타를 추가한 16비트 CRC값.
 */
uint32_t pifCheckSum(uint8_t* p_data, uint16_t length);

/**
 * @fn pifCheckXor
 * @brief Buffer에서의 byte 단위로 Xor한 결과를 반환한다.
 * @param p_data CheckXor할 데이터의 시작 포인터.
 * @param length CheckXor할 데이터의 크기.
 * @return 데이타를 추가한 16비트 CRC값.
 */
uint8_t pifCheckXor(uint8_t* p_data, uint16_t length);

/**
 * @fn pifPidControl_Init
 * @brief PID 컨트롤에 사용되는 전역 변수를 초기화한다.
 * @param p_owner PidControl 자신
 * @param kp 비례 계수
 * @param ki 적분 계수
 * @param kd 미분 계수
 * @param max_integration 최대 적분 오차값
 */
void pifPidControl_Init(PifPidControl* p_owner, float kp, float ki, float kd, float max_integration);

/**
 * @fn pifPidControl_Calcurate
 * @brief 입력된 오차로 조정값을 계산한다.
 * @param p_owner PidControl 자신
 * @param err 오차
 * @return 조정값
 */
float pifPidControl_Calcurate(PifPidControl* p_owner, float err);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
