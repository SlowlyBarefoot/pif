#ifndef PIF_LED_H
#define PIF_LED_H


#include "pifPulse.h"


typedef void (*PIF_actLedState)(PifId usPifId, uint32_t unState);

/**
 * @class _PIF_stLed
 * @brief
 */
typedef struct _PIF_stLed
{
	// Public Member Variable
    uint8_t ucLedCount;

	// Read-only Member Variable
    PifId _usPifId;

	// Private Member Variable
	PifPulse *__pstTimer;
	uint32_t __unState;
	SWITCH __swBlink;
	uint32_t __unBlinkFlag;
	PifPulseItem *__pstTimerBlink;

	// Private Action Function
	PIF_actLedState __actState;
} PIF_stLed;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stLed *pifLed_Create(PifId usPifId, PifPulse *pstTimer, uint8_t ucCount, PIF_actLedState actState);
void pifLed_Destroy(PIF_stLed** pp_owner);

void pifLed_EachOn(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_EachOff(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_EachChange(PIF_stLed *pstOwner, uint8_t ucIndex, SWITCH swState);
void pifLed_EachToggle(PIF_stLed *pstOwner, uint8_t ucIndex);

void pifLed_AllOn(PIF_stLed *pstOwner);
void pifLed_AllOff(PIF_stLed *pstOwner);
void pifLed_AllChange(PIF_stLed *pstOwner, uint32_t unState);
void pifLed_AllToggle(PIF_stLed *pstOwner);

BOOL pifLed_AttachBlink(PIF_stLed *pstOwner, uint16_t usPeriodMs);
void pifLed_DetachBlink(PIF_stLed *pstOwner);
BOOL pifLed_ChangeBlinkPeriod(PIF_stLed *pstOwner, uint16_t usPeriodMs);

void pifLed_BlinkOn(PIF_stLed *pstOwner, uint8_t ucIndex);
void pifLed_BlinkOff(PIF_stLed *pstOwner, uint8_t ucIndex);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
