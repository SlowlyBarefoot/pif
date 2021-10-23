#ifndef PIF_LED_H
#define PIF_LED_H


#include "pifPulse.h"


typedef void (*PifActLedState)(PifId id, uint32_t state);

/**
 * @class StPifLed
 * @brief
 */
typedef struct StPifLed
{
	// Public Member Variable
    uint8_t count;

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifPulse*__p_timer;
	uint32_t __state;
	SWITCH __blink;
	uint32_t __blink_flag;
	PifPulseItem* __p_timer_blink;

	// Private Action Function
	PifActLedState __act_state;
} PifLed;


#ifdef __cplusplus
extern "C" {
#endif

PifLed* pifLed_Create(PifId id, PifPulse* p_timer, uint8_t count, PifActLedState act_state);
void pifLed_Destroy(PifLed** pp_owner);

BOOL pifLed_Init(PifLed* p_owner, PifId id, PifPulse* p_timer, uint8_t count, PifActLedState act_state);
void pifLed_Clear(PifLed* p_owner);

void pifLed_EachOn(PifLed* p_owner, uint8_t index);
void pifLed_EachOff(PifLed* p_owner, uint8_t index);
void pifLed_EachChange(PifLed* p_owner, uint8_t index, SWITCH state);
void pifLed_EachToggle(PifLed* p_owner, uint8_t index);

void pifLed_AllOn(PifLed* p_owner);
void pifLed_AllOff(PifLed* p_owner);
void pifLed_AllChange(PifLed* p_owner, uint32_t state);
void pifLed_AllToggle(PifLed* p_owner);

BOOL pifLed_AttachBlink(PifLed* p_owner, uint16_t period1ms);
void pifLed_DetachBlink(PifLed* p_owner);
BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms);

void pifLed_BlinkOn(PifLed* p_owner, uint8_t index);
void pifLed_BlinkOff(PifLed* p_owner, uint8_t index);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
