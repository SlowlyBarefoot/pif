// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_keypad.h
 * @brief Keypad of up to 16 keys per row word, with press, release, long press and double press events.
 *
 * The platform reads the state of all keys through act_acquire, and a task debounces them and raises the events
 * with the character of each key.
 */
#ifndef PIF_KEYPAD_H
#define PIF_KEYPAD_H


#include "core/pif_task_manager.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_KEYPAD)
	#define COLLECT_SIGNAL_KEYPAD	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_NO_KEYPAD is not
	#include "core/pif_collect_signal.h"
#endif


#ifndef PIF_KEYPAD_DEFAULT_HOLD_TIME
#define PIF_KEYPAD_DEFAULT_HOLD_TIME	100	///< Default time in ms a press has to last to count
#endif

#ifndef PIF_KEYPAD_DEFAULT_LONG_TIME
#define PIF_KEYPAD_DEFAULT_LONG_TIME	1000	///< Default time in ms of holding that makes a long press
#endif

#ifndef PIF_KEYPAD_DEFAULT_DOUBLE_TIME
#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300	///< Default time in ms within which a second press makes a double press
#endif

#define PIF_KEYPAD_CONTROL_PERIOD		20	///< Period in ms of the scanning task


/**
 * @brief State of one key.
 */
typedef enum EnPifKeyState
{
	KS_IDLE,	///< Not pressed
	KS_PRESSED,	///< Pressed, not yet for the hold time
	KS_HOLD,	///< Held past the hold time
	KS_RELEASED	///< Just released
} PifKeyState;

#ifdef COLLECT_SIGNAL_KEYPAD

/**
 * @brief Signals of a keypad for pifKeypad_SetCsFlag() and pifKeypad_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifKeypadCsFlag
{
	KP_CSF_OFF			= 0,

	KP_CSF_STATE_IDX	= 0,	///< PifKeyState of each key, named "KP_<key index>"

	KP_CSF_STATE_BIT	= 1,
	KP_CSF_ALL_BIT		= 1,

	KP_CSF_COUNT		= 1
} PifKeypadCsFlag;

#endif	// COLLECT_SIGNAL_KEYPAD


/**
 * @brief Reads the state of all keys.
 *
 * Bit c of p_state[r] is the key in row r and column c, 1 while it is pressed.
 */
typedef void (*PifActKeypadAcquire)(uint16_t* p_state);

/**
 * @brief Reports a press of the key ch.
 */
typedef void (*PifEvtKeypadPressed)(char ch);
/**
 * @brief Reports a double press of the key ch.
 */
typedef void (*PifEvtKeypadDoublePressed)(char ch);
/**
 * @brief Reports the release of the key ch held for on_time ms.
 */
typedef void (*PifEvtKeypadReleased)(char ch, uint32_t on_time);
/**
 * @brief Reports a long press of the key ch held for on_time ms so far.
 */
typedef void (*PifEvtKeypadLongPressed)(char ch, uint32_t on_time);


/**
 * @class StPifKey
 * @brief Runtime state for one key in the keypad matrix.
 */
typedef struct StPifKey
{
	PifKeyState state;	///< State of the key
	BOOL pressed;	///< Pressed for longer than the hold time
	BOOL short_clicked;	///< A short click waits for the double press window to close
	BOOL double_pressed;	///< The press is the second of a double press
	BOOL long_pressed;	///< evt_long_pressed was raised for this press
	uint32_t pressed_time;	///< Time of the press, from pif_cumulative_timer1ms
	uint32_t first_time;	///< Time of the first press of a possible double press
#ifdef COLLECT_SIGNAL_KEYPAD
	PifCollectSignalChannel __cs[KP_CSF_COUNT];
#endif
} PifKey;

/**
 * @class StPifKeypad
 * @brief Keypad controller context used to scan keys and generate key events.
 */
typedef struct StPifKeypad
{
	// Public Member Variable

	// Public Event Function
	/// A press shorter than _hold_time1ms is noise and raises no event.
	PifEvtKeypadPressed evt_pressed;				///< At _double_time1ms of holding, or with evt_released for a short click
	PifEvtKeypadReleased evt_released;				///< On release, unless evt_long_pressed was raised
	PifEvtKeypadLongPressed evt_long_pressed;		///< Once, while still held, at _long_time1ms
	PifEvtKeypadDoublePressed evt_double_pressed;	///< When a second press within _double_time1ms passes _hold_time1ms.
													///< If set, a short click is reported only after that window closes.

	// Read-only Member Variable
	PifId _id;										///< Instance ID
	uint16_t _hold_time1ms;							///< Default: PIF_KEYPAD_DEFAULT_HOLD_TIME
	uint16_t _long_time1ms;							///< Default: PIF_KEYPAD_DEFAULT_LONG_TIME
	uint16_t _double_time1ms;						///< Default: PIF_KEYPAD_DEFAULT_DOUBLE_TIME

	// Private Member Variable
    PifTask* __p_task;
	const char* __p_user_keymap;
	uint8_t __num_block;
	uint8_t __num_cell;
	uint8_t __num_key;
	uint16_t* __p_state;
	PifKey* __p_key;
    uint16_t __control_period_1ms;					///< PIF_KEYPAD_CONTROL_PERIOD

	// Private Action Function
	PifActKeypadAcquire __act_acquire;
} PifKeypad;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a keypad instance.
 * @param p_owner Pointer to the keypad instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param act_acquire Callback that reads the keypad bit-state array.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifKeypad_Init(PifKeypad* p_owner, PifId id, PifActKeypadAcquire act_acquire);

/**
 * @brief Releases runtime resources held by the keypad instance.
 * @param p_owner Pointer to the keypad instance.
 */
void pifKeypad_Clear(PifKeypad* p_owner);

/**
 * @brief Gets the current keypad scan period.
 * @param p_owner Pointer to the keypad instance.
 * @return Scan period in milliseconds.
 */
uint16_t pifKeypad_GetControlPeriod(PifKeypad* p_owner);

/**
 * @brief Updates the keypad scan period.
 * @param p_owner Pointer to the keypad instance.
 * @param period1ms New scan period in milliseconds. Must be greater than zero.
 * @return `TRUE` if the period is valid and applied, otherwise `FALSE`.
 */
BOOL pifKeypad_SetControlPeriod(PifKeypad* p_owner, uint16_t period1ms);

/**
 * @brief Configures the key count and index-to-character mapping.
 * @param p_owner Pointer to the keypad instance.
 * @param num Number of keys to manage.
 * @param p_user_keymap Character mapping table indexed by key position.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifKeypad_SetKeymap(PifKeypad* p_owner, uint8_t num, const char* p_user_keymap);

/**
 * @brief Sets the threshold for entering hold state.
 * @param p_owner Pointer to the keypad instance.
 * @param hold_time1ms Hold threshold in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetHoldTime(PifKeypad* p_owner, uint16_t hold_time1ms);

/**
 * @brief Sets the threshold used for long-release detection.
 * @param p_owner Pointer to the keypad instance.
 * @param long_time1ms Long-release threshold in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetLongTime(PifKeypad* p_owner, uint16_t long_time1ms);

/**
 * @brief Sets the interval used for double-press detection.
 * @param p_owner Pointer to the keypad instance.
 * @param double_time1ms Double-press interval in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetDoubleTime(PifKeypad* p_owner, uint16_t double_time1ms);

/**
 * @brief Starts or resumes periodic keypad scanning.
 * @param p_owner Pointer to the keypad instance.
 * @param p_name Optional task name. If `NULL`, `"Keypad"` is used.
 * @return `TRUE` if scanning starts successfully, otherwise `FALSE`.
 */
BOOL pifKeypad_Start(PifKeypad* p_owner, const char* p_name);

/**
 * @brief Pauses periodic keypad scanning.
 * @param p_owner Pointer to the keypad instance.
 */
void pifKeypad_Stop(PifKeypad* p_owner);

#ifdef COLLECT_SIGNAL_KEYPAD

/**
 * @brief Adds the selected signals of every key to pifCollectSignal as channels. Call it after
 *        pifKeypad_SetKeymap(), which removes them again when it changes the keys.
 * @param p_owner Pointer to the keypad instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifKeypad_SetCsFlag(PifKeypad* p_owner, PifKeypadCsFlag flag);

/**
 * @brief Removes the selected signals of every key from pifCollectSignal.
 * @param p_owner Pointer to the keypad instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifKeypad_ResetCsFlag(PifKeypad* p_owner, PifKeypadCsFlag flag);

#endif	// COLLECT_SIGNAL_KEYPAD

#ifdef __cplusplus
}
#endif


#endif	// PIF_KEYPAD_H
