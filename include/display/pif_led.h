#ifndef PIF_LED_H
#define PIF_LED_H


#include "core/pif_timer_manager.h"


typedef void (*PifActLedState)(PifId id, uint32_t state);

/**
 * @class StPifLedBlink
 * @brief Blink profile entry used by single or multi-blink LED control.
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
 * @brief Multi-channel LED controller with optional timer-based blink features.
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
 * @brief Initializes the LED controller and registers state output callback.
 * @param p_owner Pointer to the LED controller instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param p_timer_manager Timer manager used for blink scheduling.
 * @param count Number of LED channels managed by this instance.
 * @param act_state Callback that applies aggregated LED state to hardware.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifLed_Init(PifLed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t count, PifActLedState act_state);

/**
 * @fn pifLed_Clear
 * @brief Releases resources allocated by the LED controller.
 * @param p_owner Pointer to the LED controller instance to clear.
 */
void pifLed_Clear(PifLed* p_owner);

/**
 * @fn pifLed_PartOn
 * @brief Turns on selected LEDs using a bit mask.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartOn(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_PartOff
 * @brief Turns off selected LEDs using a bit mask.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartOff(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_PartChange
 * @brief Sets selected LEDs to the specified ON/OFF state.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 * @param state Target state (`ON` or `OFF`) for selected channels.
 */
void pifLed_PartChange(PifLed* p_owner, uint8_t bits, SWITCH state);

/**
 * @fn pifLed_PartToggle
 * @brief Toggles selected LEDs.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartToggle(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_AllOn
 * @brief Turns on all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllOn(PifLed* p_owner);

/**
 * @fn pifLed_AllOff
 * @brief Turns off all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllOff(PifLed* p_owner);

/**
 * @fn pifLed_AllChange
 * @brief Sets all LED channels from a packed state bitfield.
 * @param p_owner Pointer to an initialized LED controller.
 * @param state Packed ON/OFF bits applied to all channels.
 */
void pifLed_AllChange(PifLed* p_owner, uint32_t state);

/**
 * @fn pifLed_AllToggle
 * @brief Toggles all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllToggle(PifLed* p_owner);

/**
 * @fn pifLed_AttachSBlink
 * @brief Enables single-pattern blink mode for selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param period1ms Blink timer period in milliseconds.
 * @return `TRUE` if blink timer is created and started, otherwise `FALSE`.
 */
BOOL pifLed_AttachSBlink(PifLed* p_owner, uint16_t period1ms);

/**
 * @fn pifLed_AttachMBlink
 * @brief Enables multi-pattern blink mode with variable blink profile entries.
 * @param p_owner Pointer to an initialized LED controller.
 * @param period1ms Blink timer period in milliseconds.
 * @param count Number of blink profile entries that follow in variadic arguments.
 * @param ... Variadic blink profile parameters consumed by implementation.
 * @return `TRUE` if multi-blink resources are configured, otherwise `FALSE`.
 */
BOOL pifLed_AttachMBlink(PifLed* p_owner, uint16_t period1ms, uint8_t count, ...);

/**
 * @fn pifLed_DetachBlink
 * @brief Disables blink mode and releases associated timer resources.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_DetachBlink(PifLed* p_owner);

/**
 * @fn pifLed_ChangeBlinkPeriod
 * @brief Changes the active blink timer period.
 * @param p_owner Pointer to an initialized LED controller with blink enabled.
 * @param period1ms New blink period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms);

/**
 * @fn pifLed_SBlinkOn
 * @brief Enables single-pattern blinking on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels that should blink.
 */
void pifLed_SBlinkOn(PifLed* p_owner, uint8_t bits);

/**
 * @fn pifLed_MBlinkOn
 * @brief Enables a specific multi-blink profile on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels that should blink.
 * @param index Blink profile index to apply.
 */
void pifLed_MBlinkOn(PifLed* p_owner, uint8_t bits, uint8_t index);

/**
 * @fn pifLed_SBlinkOff
 * @brief Disables single-pattern blinking on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels to stop blinking.
 * @param state Final LED state to apply after blink is disabled.
 */
void pifLed_SBlinkOff(PifLed* p_owner, uint8_t bits, SWITCH state);

/**
 * @fn pifLed_MBlinkOff
 * @brief Disables a multi-blink profile on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels to stop blinking.
 * @param index Blink profile index to disable.
 * @param state Final LED state to apply after blink is disabled.
 */
void pifLed_MBlinkOff(PifLed* p_owner, uint8_t bits, uint8_t index, SWITCH state);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
