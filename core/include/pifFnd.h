#ifndef PIF_FND_H
#define PIF_FND_H


#include "pifPulse.h"


typedef void (*PIF_actFndDisplay)(uint8_t ucSegment, uint8_t ucDigit);

/**
 * @class _PIF_stFnd
 * @brief
 */
typedef struct _PIF_stFnd
{
	// Public Member Variable
	uint8_t ucSubNumericDigits;

	// Read-only Member Variable
	PIF_usId _usPifId;
    uint8_t _ucFndCount;
    uint8_t _ucDigitSize;

	// Private Member Variable
	struct {
		uint8_t Run			: 1;
		uint8_t Blink		: 1;
		uint8_t FillZero	: 1;
	} __bt;
    uint16_t __usControlPeriodMs;
	uint16_t __usPretimeMs;
	uint8_t __ucDigitIndex;
	uint8_t __ucStringSize;
    char *__pcString;
	PIF_stPulseItem *__pstTimerBlink;

	// Private Action Function
   	PIF_actFndDisplay __actDisplay;
} PIF_stFnd;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifFnd_Init(uint8_t ucSize, PIF_stPulse *pstTimer);
void pifFnd_Exit();

void pifFnd_SetUserChar(const uint8_t *pucUserChar, uint8_t ucCount);

PIF_stFnd *pifFnd_Add(PIF_usId usPifId, uint8_t ucDigitSize, PIF_actFndDisplay actDisplay);

BOOL pifFnd_SetControlPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs);

void pifFnd_Start(PIF_stFnd *pstOwner);
void pifFnd_Stop(PIF_stFnd *pstOwner);

BOOL pifFnd_BlinkOn(PIF_stFnd *pstOwner, uint16_t usPeriodMs);
void pifFnd_BlinkOff(PIF_stFnd *pstOwner);
BOOL pifFnd_ChangeBlinkPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs);

void pifFnd_SetFillZero(PIF_stFnd *pstOwner, BOOL bFillZero);
void pifFnd_SetFloat(PIF_stFnd *pstOwner, double dValue);
void pifFnd_SetInterger(PIF_stFnd *pstOwner, int32_t nValue);
void pifFnd_SetString(PIF_stFnd *pstOwner, char *pcString);

// Task Function
uint16_t pifFnd_Task(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FND_H
