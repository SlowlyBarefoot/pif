#ifndef PIF_LED_H
#define PIF_LED_H


#include "pifPulse.h"


typedef void (*PIF_actLedState)(PIF_usId usPifId, uint8_t ucIndex, SWITCH swState);

/**
 * @class _PIF_stLed
 * @brief
 */
typedef struct _PIF_stLed
{
	// Public Member Variable
    uint8_t ucLedCount;

	// Read-only Member Variable
	PIF_usId _usPifId;

	// Private Member Variable
	uint32_t __unState;
	SWITCH __swBlink;
	uint32_t __unBlinkFlag;
	PIF_stPulseItem *__pstTimerBlink;

	// Private Action Function
	PIF_actLedState __actState;
} PIF_stLed;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifLed_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifLed_Exit();

PIF_stLed *pifLed_Add(PIF_usId usPifId, uint8_t ucCount, PIF_actLedState actState);

void pifLed_On(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_Off(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_Change(PIF_stLed *pstOwner, uint8_t ucIndex, SWITCH swState);

BOOL pifLed_AttachBlink(PIF_stLed *pstOwner, uint16_t usPeriodMs);
void pifLed_DetachBlink(PIF_stLed *pstOwner);
BOOL pifLed_ChangeBlinkPeriod(PIF_stLed *pstOwner, uint16_t usPeriodMs);

void pifLed_BlinkOn(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_BlinkOff(PIF_stLed *pstOwner, uint8_t ucIndex);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
