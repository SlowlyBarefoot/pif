#ifndef PIF_H
#define PIF_H


#include <stdint.h>
#include <stdlib.h>


#define PIF_VERSION_MAJOR	0
#define PIF_VERSION_MINOR	1

#define USE_LOG		0

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


typedef uint16_t PIF_unDeviceCode;

typedef enum _PIF_enError
{
    E_enSuccess         		= 0x00,

	E_enInvalidParam    		= 0x01,
    E_enInvalidState    		= 0x02,
    E_enOutOfHeap       		= 0x03,
    E_enOverflowBuffer			= 0x04,
	E_enEmptyInBuffer			= 0x05,
	E_enWrongData				= 0x06,
} PIF_enError;


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
 * @class _PIF_stLogFlag
 * @author SlowlyBarefoot
 * @date 26/04/20
 * @file pif.h
 * @brief 항목별 Log 출력 여부
 */
typedef struct _PIF_stLogFlag
{
	uint32_t btPerformance			: 1;
	uint32_t btTask					: 1;
} PIF_stLogFlag;

/**
 * @class _PIF_stPidControl
 * @author SlowlyBarefoot
 * @date 26/04/20
 * @file pif.h
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


extern PIF_enError pif_enError;

extern volatile uint16_t pif_usTimer1ms;
extern volatile uint32_t pif_unTimer1sec;
extern volatile PIF_stDateTime pif_stDateTime;

extern PIF_stLogFlag pif_stLogFlag;


#ifdef __cplusplus
extern "C" {
#endif

void pif_Init();
void pif_Loop();
void pif_sigTimer1ms();

void pif_ClearError();

void pifCrc7_Init();
void pifCrc7_Calcurate(uint8_t ucData);
uint8_t pifCrc7_Result();

void pifPidControl_Init(PIF_stPidControl *pstOwner, float fFsKp, float fFsKi, float fFsKd, float fMaxIntegration);
float pifPidControl_Calcurate(PIF_stPidControl *pstOwner, float fErr);

#ifdef __cplusplus
}
#endif


#endif  // PIF_H
