// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_led.h
 * @brief Up to 32 LEDs switched as one bit field, with blinking.
 *
 * The platform writes all LEDs at once through act_state. Blinking is done by a task, at one period or at
 * several multiples of a base period.
 */
#ifndef PIF_LED_H
#define PIF_LED_H


#include "core/pif_task_manager.h"


/**
 * @brief Writes the state of all LEDs of the instance id. Bit i is LED i.
 */
typedef void (*PifActLedState)(PifId id, uint32_t state);

/**
 * @class StPifLedBlink
 * @brief Blink profile entry used by single or multi-blink LED control.
 */
typedef struct StPifLedBlink
{
	SWITCH state;	///< ON in the current half of the blink
	uint16_t multiple;	///< Toggles after this many periods
	uint16_t count;	///< Periods left before the next toggle
	uint32_t flag;	///< LEDs that blink with this profile, one bit each
} PifLedBlink;

/**
 * @class StPifLed
 * @brief Multi-channel LED controller with optional task-based blink features.
 */
typedef struct StPifLed
{
	// Public Member Variable
    uint8_t count;	///< Number of LEDs, 1 to 32

	// Read-only Member Variable
    PifId _id;						///< Instance ID

	// Private Member Variable
	uint32_t __state;
	uint8_t __blink_count;
	PifLedBlink* __p_blinks;
	PifTask* __p_task;				///< Blink task, paused while no LED blinks

	// Private Action Function
	PifActLedState __act_state;
} PifLed;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the LED controller and registers state output callback.
 * @param p_owner Pointer to the LED controller instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param count Number of LED channels managed by this instance. Bits past it are ignored by
 *              every function that takes a bit mask or a state.
 * @param act_state Callback that applies aggregated LED state to hardware.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifLed_Init(PifLed* p_owner, PifId id, uint8_t count, PifActLedState act_state);

/**
 * @brief Releases resources allocated by the LED controller.
 * @param p_owner Pointer to the LED controller instance to clear.
 */
void pifLed_Clear(PifLed* p_owner);

/**
 * @brief Turns on selected LEDs using a bit mask.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartOn(PifLed* p_owner, uint32_t bits);

/**
 * @brief Turns off selected LEDs using a bit mask.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartOff(PifLed* p_owner, uint32_t bits);

/**
 * @brief Sets selected LEDs to the specified ON/OFF state.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 * @param state Target state (`ON` or `OFF`) for selected channels.
 */
void pifLed_PartChange(PifLed* p_owner, uint32_t bits, SWITCH state);

/**
 * @brief Toggles selected LEDs.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting target LED channels.
 */
void pifLed_PartToggle(PifLed* p_owner, uint32_t bits);

/**
 * @brief Turns on all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllOn(PifLed* p_owner);

/**
 * @brief Turns off all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllOff(PifLed* p_owner);

/**
 * @brief Sets all LED channels from a packed state bitfield.
 * @param p_owner Pointer to an initialized LED controller.
 * @param state Packed ON/OFF bits applied to all channels.
 */
void pifLed_AllChange(PifLed* p_owner, uint32_t state);

/**
 * @brief Toggles all configured LED channels.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_AllToggle(PifLed* p_owner);

/**
 * @brief Enables single-pattern blink mode for selected channels. Same as
 *        pifLed_AttachMBlink(p_owner, period1ms, 1, 1).
 * @param p_owner Pointer to an initialized LED controller.
 * @param period1ms Blink period in milliseconds, the time each LED stays on or off.
 * @return `TRUE` if the blink task is ready, otherwise `FALSE`.
 */
BOOL pifLed_AttachSBlink(PifLed* p_owner, uint16_t period1ms);

/**
 * @brief Enables multi-pattern blink mode with variable blink profile entries.
 *        It adds a task that runs only while an LED blinks.
 * @param p_owner Pointer to an initialized LED controller.
 * @param period1ms Blink period in milliseconds, the unit of the multiples below.
 * @param count Number of blink profile entries that follow in variadic arguments.
 * @param ... `count` values of type `int`, each a non-zero multiple of the period at which
 *            that profile toggles. Calling it again replaces the profiles and clears which
 *            LEDs blink.
 * @return `TRUE` if multi-blink resources are configured, otherwise `FALSE`. On failure the
 *         previous blink setting is kept.
 */
BOOL pifLed_AttachMBlink(PifLed* p_owner, uint16_t period1ms, uint8_t count, ...);

/**
 * @brief Disables blink mode and removes the blink task. The LEDs keep their last state.
 * @param p_owner Pointer to an initialized LED controller.
 */
void pifLed_DetachBlink(PifLed* p_owner);

/**
 * @brief Changes the blink period.
 * @param p_owner Pointer to an initialized LED controller with blink enabled.
 * @param period1ms New blink period in milliseconds.
 * @return `TRUE` if period update succeeds, otherwise `FALSE`.
 */
BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms);

/**
 * @brief Enables single-pattern blinking on selected channels. Same as
 *        pifLed_MBlinkOn(p_owner, bits, 0).
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels that should blink.
 */
void pifLed_SBlinkOn(PifLed* p_owner, uint32_t bits);

/**
 * @brief Enables a specific multi-blink profile on selected channels, and takes them out of
 *        any other profile. The LEDs take the current phase of the profile at once.
 *        When no LED was blinking, every profile starts over: the LEDs turn on at once and
 *        toggle after a full multiple of the period.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels that should blink.
 * @param index Blink profile index to apply.
 */
void pifLed_MBlinkOn(PifLed* p_owner, uint32_t bits, uint8_t index);

/**
 * @brief Disables single-pattern blinking on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels to stop blinking.
 * @param state Final LED state to apply after blink is disabled.
 */
void pifLed_SBlinkOff(PifLed* p_owner, uint32_t bits, SWITCH state);

/**
 * @brief Disables a multi-blink profile on selected channels.
 * @param p_owner Pointer to an initialized LED controller.
 * @param bits Bit mask selecting channels to stop blinking.
 * @param index Blink profile index to disable.
 * @param state Final LED state to apply after blink is disabled. A channel that blinks in
 *              another profile keeps blinking there and is left alone.
 */
void pifLed_MBlinkOff(PifLed* p_owner, uint32_t bits, uint8_t index, SWITCH state);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LED_H
