#ifndef PIF_H
#define PIF_H


#include <stdint.h>
#include <stdlib.h>


//#define __PIF_DEBUG__
//#define __PIF_NO_LOG__
//#define __PIF_NO_TERMINAL__
//#define __PIF_COLLECT_SIGNAL__

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
	E_enCanNotUse				= 0x09
} PIF_enError;


/**
 * @struct _PIF_stDateTime
 * @brief 날짜 시간 정보
 */
typedef struct _PIF_stDateTime
{
    uint16_t usYear;
    uint8_t ucMonth;
    uint8_t ucDay;
    uint8_t ucHour;
    uint8_t ucMinute;
    uint8_t ucSec;
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
	uint8_t __ucState;
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


#ifdef __cplusplus
extern "C" {
#endif

void pif_Init(PIF_actTimer1us actTimer1us);
void pif_Loop();
void pif_sigTimer1ms();

void pif_Delay1ms(uint16_t usDelay);

void pif_ClearError();

#ifndef	__PIF_NO_LOG__

int pif_BinToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt);
int pif_DecToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt);
int pif_HexToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt, BOOL bUpper);
int pif_FloatToString(char *pcBuf, double dNum, uint16_t usPoint);
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
