#ifndef PIF_H
#define PIF_H


#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


// -------- pif Configuration --------------------

//#define __PIF_DEBUG__
//#define __PIF_NO_USE_INLINE__

#ifndef PIF_WEAK
#define PIF_WEAK __attribute__ ((weak))
#endif

// -------- pifCollectSignal ---------------------

//#define __PIF_COLLECT_SIGNAL__

// -------- pifGpsNmea ---------------------------

#ifndef PIF_GPS_NMEA_TX_SIZE
#define PIF_GPS_NMEA_TX_SIZE			32
#endif

#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			32
#endif

// -------- pifKeypad ----------------------------

#ifndef PIF_KEYPAD_DEFAULT_HOLD_TIME
#define PIF_KEYPAD_DEFAULT_HOLD_TIME	100
#endif

#ifndef PIF_KEYPAD_DEFAULT_LONG_TIME
#define PIF_KEYPAD_DEFAULT_LONG_TIME	1000
#endif

#ifndef PIF_KEYPAD_DEFAULT_DOUBLE_TIME
#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300
#endif

// -------- pifLog -------------------------------

//#define __PIF_NO_LOG__
//#define __PIF_LOG_COMMAND__

#ifndef PIF_LOG_LINE_SIZE
#define PIF_LOG_LINE_SIZE				80
#endif

#ifndef PIF_LOG_RX_BUFFER_SIZE
#define PIF_LOG_RX_BUFFER_SIZE			32
#endif

#ifndef PIF_LOG_TX_BUFFER_SIZE
#define PIF_LOG_TX_BUFFER_SIZE			80
#endif

// -------- pifProtocol --------------------------

#ifndef PIF_PROTOCOL_RX_PACKET_SIZE
#define PIF_PROTOCOL_RX_PACKET_SIZE		32
#endif

#ifndef PIF_PROTOCOL_TX_REQUEST_SIZE
#define PIF_PROTOCOL_TX_REQUEST_SIZE	64
#endif

#ifndef PIF_PROTOCOL_TX_ANSWER_SIZE
#define PIF_PROTOCOL_TX_ANSWER_SIZE		32
#endif

// 한 packet을 전부 받는 시간 제한
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 50이고 타이머 단위가 1ms이면 50 * 1ms = 50ms이다.
#ifndef PIF_PROTOCOL_RECEIVE_TIMEOUT
#define PIF_PROTOCOL_RECEIVE_TIMEOUT	50
#endif

// Retry하기 전 delay 시간
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 10이고 타이머 단위가 1ms이면 10 * 1ms = 10ms이다.
#ifndef PIF_PROTOCOL_RETRY_DELAY
#define PIF_PROTOCOL_RETRY_DELAY		10
#endif

// -------- pifPulse -----------------------------

#ifndef PIF_PWM_MAX_DUTY
#define PIF_PWM_MAX_DUTY				1000
#endif

// -------- pifTask ------------------------------

#ifndef PIF_TASK_TABLE_SIZE
#define PIF_TASK_TABLE_SIZE				32
#endif

// -----------------------------------------------

#define PIF_VERSION_MAJOR	0
#define PIF_VERSION_MINOR	1
#define PIF_VERSION_PATCH	0

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

#define PIF_ID_AUTO		0

#define PIF_CHECK_ELAPSE_TIME_1MS(START, ELAPSE)	(pif_cumulative_timer1ms - (START) >= (ELAPSE))
#define PIF_CHECK_ELAPSE_TIME_1US(START, ELAPSE)	((*pif_act_timer1us)() - (START) >= (ELAPSE))


typedef uint16_t PifId;

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
	E_NOT_SET_TASK				= 0x0B
} PifError;


/**
 * @struct StPifDateTime
 * @brief 날짜 시간 정보
 */
typedef struct StPifDateTime
{
    uint8_t year;
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

	// Private Member Variable
	BOOL __state;
#ifdef __PIF_DEBUG__
#ifndef __PIF_NO_LOG__
	uint32_t __max_loop_time1us;
#endif
#endif
} PifPerformance;

typedef uint32_t (*PifActTimer1us)();


extern PifError pif_error;

extern volatile uint16_t pif_timer1ms;
extern volatile uint32_t pif_timer1sec;
extern volatile PifDateTime pif_datetime;

extern volatile uint32_t pif_cumulative_timer1ms;

extern PifPerformance pif_performance;

extern PifId pif_id;

extern PifActTimer1us pif_act_timer1us;

extern const char* kPifMonth3[12];

extern const char* kPifHexUpperChar;
extern const char* kPifHexLowerChar;


#ifdef __cplusplus
extern "C" {
#endif

void pif_Init(PifActTimer1us act_timer1us);
void pif_Exit();

void pif_Loop();
void pif_sigTimer1ms();

void pif_Delay1ms(uint16_t delay);
void pif_Delay1us(uint16_t delay);

void pif_ClearError();

int pif_BinToString(char* p_buffer, uint32_t value, uint16_t str_cnt);
int pif_DecToString(char* p_buffer, uint32_t value, uint16_t str_cnt);
int pif_HexToString(char* p_buffer, uint32_t value, uint16_t str_cnt, BOOL upper);
int pif_FloatToString(char* p_buffer, double value, uint16_t point);
void pif_PrintFormat(char* p_buffer, va_list* data, const char* p_format);
void pif_Printf(char* p_buffer, const char* p_format, ...);

void pifCrc7_Init();
void pifCrc7_Calcurate(uint8_t data);
uint8_t pifCrc7_Result();

uint16_t pifCrc16(uint8_t* p_data, uint16_t length);

uint8_t pifCheckSum(uint8_t* p_data, uint16_t length);
uint8_t pifCheckXor(uint8_t* p_data, uint16_t length);

void pifPidControl_Init(PifPidControl* p_owner, float kp, float ki, float kd, float max_integration);
float pifPidControl_Calcurate(PifPidControl* p_owner, float err);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
