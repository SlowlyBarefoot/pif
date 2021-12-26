#ifndef PIF_LED_H
#define PIF_LED_H


#include "pif_timer.h"


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
	PifTimerManager* __p_timer_manager;
	uint32_t __state;
	SWITCH __blink;
	uint32_t __blink_flag;
	PifTimer* __p_timer_blink;

	// Private Action Function
	PifActLedState __act_state;
} PifLed;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifLed_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param count
 * @param act_state
 * @return
 */
BOOL pifLed_Init(PifLed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t count, PifActLedState act_state);

/**
 * @fn pifLed_Clear
 * @brief
 * @param p_owner
 */
void pifLed_Clear(PifLed* p_owner);

/**
 * @fn pifLed_EachOn
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachOn(PifLed* p_owner, uint8_t index);

/**
 * @fn pifLed_EachOff
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachOff(PifLed* p_owner, uint8_t index);

/**
 * @fn pifLed_EachChange
 * @brief
 * @param p_owner
 * @param index
 * @param state
 */
void pifLed_EachChange(PifLed* p_owner, uint8_t index, SWITCH state);

/**
 * @fn pifLed_EachToggle
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachToggle(PifLed* p_owner, uint8_t index);

/**
 * @fn pifLed_AllOn
 * @brief
 * @param p_owner
 */
void pifLed_AllOn(PifLed* p_owner);

/**
 * @fn pifLed_AllOff
 * @brief
 * @param p_owner
 */
void pifLed_AllOff(PifLed* p_owner);

/**
 * @fn pifLed_AllChange
 * @brief
 * @param p_owner
 * @param state
 */
void pifLed_AllChange(PifLed* p_owner, uint32_t state);

/**
 * @fn pifLed_AllToggle
 * @brief
 * @param p_owner
 */
void pifLed_AllToggle(PifLed* p_owner);

/**
 * @fn pifLed_AttachBlink
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifLed_AttachBlink(PifLed* p_owner, uint16_t period1ms);

/**
 * @fn pifLed_DetachBlink
 * @brief
 * @param p_owner
 */
void pifLed_DetachBlink(PifLed* p_owner);

/**
 * @fn pifLed_ChangeBlinkPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms);

/**
 * @fn pifLed_BlinkOn
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_BlinkOn(PifLed* p_owner, uint8_t index);

/**
 * @fn pifLed_BlinkOff
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_BlinkOff(PifLed* p_owner, uint8_t index);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
