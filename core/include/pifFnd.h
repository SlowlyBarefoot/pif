#ifndef PIF_FND_H
#define PIF_FND_H


#include "pifPulse.h"


#define IDPF_FND_CONTROL_PERIOD_DEFAULT		25


typedef void (*PIF_actFndDisplay)(uint8_t ucSegment, uint8_t ucDigit);

/**
 * @class _PIF_stFnd
 * @brief
 */
typedef struct _PIF_stFnd
{
	// Public Member Variable
	PIF_unDeviceCode unDeviceCode;
    uint8_t ucFndCount;
    uint8_t ucDigitSize;
	struct {
		uint16_t btRun				: 1;
		uint16_t btBlink			: 1;
		uint16_t btFillZero			: 1;
		uint16_t btSubNumericDigits	: 8;
	};

	// Private Member Variable
    uint16_t __usControlPeriodMs;
	uint16_t __usPretimeMs;
	uint8_t __ucDigitIndex;
	uint8_t __ucStringSize;
    char *__pcString;
	PIF_stPulseItem *__pstTimerBlink;

	PIF_enTaskLoop __enTaskLoop;

	// Private Member Function
   	PIF_actFndDisplay __actDisplay;
} PIF_stFnd;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifFnd_Init(PIF_stPulse *pstTimer1ms, uint8_t ucSize);
void pifFnd_Exit();

PIF_stFnd *pifFnd_Add(PIF_unDeviceCode unDeviceCode, uint8_t ucDigitSize, PIF_actFndDisplay actDisplay);

BOOL pifFnd_SetControlPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs);

void pifFnd_Start(PIF_stFnd *pstOwner);
void pifFnd_Stop(PIF_stFnd *pstOwner);

BOOL pifFnd_BlinkOn(PIF_stFnd *pstOwner, uint16_t usPeriodMs);
void pifFnd_BlinkOff(PIF_stFnd *pstOwner);
void pifFnd_ChangeBlinkPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs);

void pifFnd_SetFillZero(PIF_stFnd *pstOwner, BOOL bFillZero);
void pifFnd_SetFloat(PIF_stFnd *pstOwner, double dValue);
void pifFnd_SetInterger(PIF_stFnd *pstOwner, int32_t nValue);
void pifFnd_SetString(PIF_stFnd *pstOwner, char *pcString);
void pifFnd_SetSubNumericDigits(PIF_stFnd *pstOwner, uint8_t ucSubNumericDigits);

// Task Function
void pifFnd_taskAll(PIF_stTask *pstTask);
void pifFnd_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H