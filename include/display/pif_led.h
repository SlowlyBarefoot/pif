#ifndef PIF_LED_H
#define PIF_LED_H


#include "core/pif_timer.h"


typedef void (*PifActLedState)(PifId id, uint32_t state);

/**
 * @class StPifLedBlink
 * @brief
 */
typedef struct StPifLedBlink
{
	SWITCH state;
	uint16_t multiple;
	uint16_t count;
	uint32_t flag;
} PifLedBlink;

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
	uint8_t __blink_count;
	PifLedBlink* __p_blinks;
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
 * @fn pifLed_PartOn
 * @brief
 * @param p_owner
 * @param bits
 */
void pifLed_PartOn(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_PartOff
 * @brief
 * @param p_owner
 * @param bits
 */
void pifLed_PartOff(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_PartChange
 * @brief
 * @param p_owner
 * @param bits
 * @param state
 */
void pifLed_PartChange(PifLed* p_owner, uint8_t bits, SWITCH state);

/**
 * @fn pifLed_PartToggle
 * @brief
 * @param p_owner
 * @param bits
 */
void pifLed_PartToggle(PifLed* p_owner, uint8_t bits);

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
 * @fn pifLed_AttachSBlink
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifLed_AttachSBlink(PifLed* p_owner, uint16_t period1ms);

/**
 * @fn pifLed_AttachMBlink
 * @brief
 * @param p_owner
 * @param period1ms
 * @param count
 * @param ...
 * @return
 */
BOOL pifLed_AttachMBlink(PifLed* p_owner, uint16_t period1ms, uint8_t count, ...);

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
 * @fn pifLed_SBlinkOn
 * @brief
 * @param p_owner
 * @param bits
 */
void pifLed_SBlinkOn(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_MBlinkOn
 * @brief
 * @param p_owner
 * @param bits
 * @param index
 */
void pifLed_MBlinkOn(PifLed* p_owner, uint8_t bits, uint8_t index);

/**
 * @fn pifLed_SBlinkOff
 * @brief
 * @param p_owner
 * @param bits
 * @param state
 */
void pifLed_SBlinkOff(PifLed* p_owner, uint8_t bits, SWITCH state);

/**
 * @fn pifLed_MBlinkOff
 * @brief
 * @param p_owner
 * @param bits
 * @param index
 * @param state
 */
void pifLed_MBlinkOff(PifLed* p_owner, uint8_t bits, uint8_t index, SWITCH state);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
