#ifndef PIF_H
#define PIF_H


#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>


// -------- pif Configuration --------------------

//#define __PIF_DEBUG__

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


typedef uint16_t PIF_usId;

typedef enum _PIF_enError
{
    E_enSuccess         		= 0x00,

	E_enInvalidParam    		= 0x01,
    E_enInvalidState    		= 0x02,
    E_enOutOfHeap       		= 0x03,
    E_enOverflowBuffer			= 0x04,
	E_enEmptyInBuffer			= 0x05,
	E_enWrongData				= 0x06,
	E_enTimeout					= 0x07,
	E_enNotSetEvent				= 0x08,
	E_enCanNotUse				= 0x09,
	E_enTransferFailed			= 0x0A
} PIF_enError;


/**
 * @struct _PIF_stDateTime
 * @brief 날짜 시간 정보
 */
typedef struct _PIF_stDateTime
{
    uint8_t ucYear;
    uint8_t ucMonth;
    uint8_t ucDay;
    uint8_t ucHour;
    uint8_t ucMinute;
    uint8_t ucSecond;
    uint16_t usMilisecond;
} PIF_stDateTime;

/**
 * @struct _PIF_stPidControl
 * @brief PID Control을 계산하기 위한 구조체
 */
typedef struct _PIF_stPidControl
{
	float  fFsKp;				// Proportional gain
	float  fFsKi;				// Integral gain
	float  fFsKd; 		    	// Derivative gain
    float  fMaxIntegration;		// Maximum Integration

    struct {
		float  fErrSum;			    // Variable: Error Sum
		float  fErrPrev;	   		// History: Previous error
    } _stPrivate;
} PIF_stPidControl;

typedef struct _PIF_stPerformance
{
	// Public Member Variable

	// Read-only Member Variable
	volatile uint32_t _unCount;

	// Private Member Variable
	BOOL __bState;
#ifdef __PIF_DEBUG__
#ifndef __PIF_NO_LOG__
	uint32_t __unMaxLoopTimeUs;
#endif
#endif
} PIF_stPerformance;

typedef uint32_t (*PIF_actTimer1us)();


extern PIF_enError pif_enError;

extern volatile uint16_t pif_usTimer1ms;
extern volatile uint32_t pif_unTimer1sec;
extern volatile PIF_stDateTime pif_stDateTime;

extern volatile uint32_t pif_unCumulativeTimer1ms;

extern PIF_stPerformance pif_stPerformance;

extern PIF_usId pif_usPifId;

extern PIF_actTimer1us pif_actTimer1us;

extern const char *pif_pacMonth3[12];

extern const char *pif_pcHexUpperChar;
extern const char *pif_pcHexLowerChar;


#ifdef __cplusplus
extern "C" {
#endif

void pif_Init(PIF_actTimer1us actTimer1us);
void pif_Loop();
void pif_sigTimer1ms();

void pif_Delay1ms(uint16_t usDelay);
BOOL pif_CheckElapseTime1ms(uint32_t unStartTime, uint16_t ElapseTime);

void pif_ClearError();

#ifndef	__PIF_NO_LOG__

int pif_BinToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt);
int pif_DecToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt);
int pif_HexToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt, BOOL bUpper);
int pif_FloatToString(char *pcBuf, double dNum, uint16_t usPoint);
void pif_PrintFormat(char *pcBuffer, va_list *pstData, const char *pcFormat);
void pif_Printf(char *pcBuffer, const char *pcFormat, ...);

#endif

void pifCrc7_Init();
void pifCrc7_Calcurate(uint8_t ucData);
uint8_t pifCrc7_Result();

uint16_t pifCrc16(uint8_t *pucData, uint16_t usLength);

uint8_t pifCheckSum(uint8_t *pucData, uint16_t usLength);
uint8_t pifCheckXor(uint8_t *pucData, uint16_t usLength);

void pifPidControl_Init(PIF_stPidControl *pstOwner, float fFsKp, float fFsKi, float fFsKd, float fMaxIntegration);
float pifPidControl_Calcurate(PIF_stPidControl *pstOwner, float fErr);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
