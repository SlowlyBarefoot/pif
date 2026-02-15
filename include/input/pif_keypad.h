#ifndef PIF_KEYPAD_H
#define PIF_KEYPAD_H


#include "core/pif_task_manager.h"


#ifndef PIF_KEYPAD_DEFAULT_HOLD_TIME
#define PIF_KEYPAD_DEFAULT_HOLD_TIME	100
#endif

#ifndef PIF_KEYPAD_DEFAULT_LONG_TIME
#define PIF_KEYPAD_DEFAULT_LONG_TIME	1000
#endif

#ifndef PIF_KEYPAD_DEFAULT_DOUBLE_TIME
#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300
#endif

#define PIF_KEYPAD_CONTROL_PERIOD		20


typedef enum EnPifKeyState
{
	KS_IDLE,
	KS_PRESSED,
	KS_HOLD,
	KS_RELEASED
} PifKeyState;


typedef void (*PifActKeypadAcquire)(uint16_t* p_state);

typedef void (*PifEvtKeypadPressed)(char ch);
typedef void (*PifEvtKeypadDoublePressed)(char ch);
typedef void (*PifEvtKeypadReleased)(char ch, uint32_t on_time);
typedef void (*PifEvtKeypadLongReleased)(char ch, uint32_t on_time);


/**
 * @class StPifKey
 * @brief Runtime state for one key in the keypad matrix.
 */
typedef struct StPifKey
{
	PifKeyState state;
	BOOL pressed;
	BOOL short_clicked;
	BOOL double_pressed;
	BOOL long_released;
	uint32_t pressed_time;
	uint32_t first_time;
} PifKey;

/**
 * @class StPifKeypad
 * @brief Keypad controller context used to scan keys and generate key events.
 */
typedef struct StPifKeypad
{
	// Public Member Variable

	// Public Event Function
	PifEvtKeypadPressed evt_pressed;
	PifEvtKeypadReleased evt_released;
	PifEvtKeypadLongReleased evt_long_released;
	PifEvtKeypadDoublePressed evt_double_pressed;

	// Read-only Member Variable
	PifId _id;
	uint16_t _hold_time1ms;							// Default: PIF_KEYPAD_DEFAULT_HOLD_TIME
	uint16_t _long_time1ms;							// Default: PIF_KEYPAD_DEFAULT_LONG_TIME
	uint16_t _double_time1ms;						// Default: PIF_KEYPAD_DEFAULT_DOUBLE_TIME

	// Private Member Variable
    PifTask* __p_task;
	const char* __p_user_keymap;
	uint8_t __num_block;
	uint8_t __num_cell;
	uint16_t* __p_state;
	PifKey* __p_key;
    uint16_t __control_period_1ms;					// PIF_KEYPAD_CONTROL_PERIOD

	// Private Action Function
	PifActKeypadAcquire __act_acquire;
} PifKeypad;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifKeypad_Init
 * @brief Initializes a keypad instance.
 * @param p_owner Pointer to the keypad instance to initialize.
 * @param id Instance ID. Use `PIF_ID_AUTO` to assign an ID automatically.
 * @param act_acquire Callback that reads the keypad bit-state array.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifKeypad_Init(PifKeypad* p_owner, PifId id, PifActKeypadAcquire act_acquire);

/**
 * @fn pifKeypad_Clear
 * @brief Releases runtime resources held by the keypad instance.
 * @param p_owner Pointer to the keypad instance.
 */
void pifKeypad_Clear(PifKeypad* p_owner);

/**
 * @fn pifKeypad_GetControlPeriod
 * @brief Gets the current keypad scan period.
 * @param p_owner Pointer to the keypad instance.
 * @return Scan period in milliseconds.
 */
uint16_t pifKeypad_GetControlPeriod(PifKeypad* p_owner);

/**
 * @fn pifKeypad_SetControlPeriod
 * @brief Updates the keypad scan period.
 * @param p_owner Pointer to the keypad instance.
 * @param period1ms New scan period in milliseconds. Must be greater than zero.
 * @return `TRUE` if the period is valid and applied, otherwise `FALSE`.
 */
BOOL pifKeypad_SetControlPeriod(PifKeypad* p_owner, uint16_t period1ms);

/**
 * @fn pifKeypad_SetKeymap
 * @brief Configures the key count and index-to-character mapping.
 * @param p_owner Pointer to the keypad instance.
 * @param num Number of keys to manage.
 * @param p_user_keymap Character mapping table indexed by key position.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifKeypad_SetKeymap(PifKeypad* p_owner, uint8_t num, const char* p_user_keymap);

/**
 * @fn pifKeypad_SetHoldTime
 * @brief Sets the threshold for entering hold state.
 * @param p_owner Pointer to the keypad instance.
 * @param hold_time1ms Hold threshold in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetHoldTime(PifKeypad* p_owner, uint16_t hold_time1ms);

/**
 * @fn pifKeypad_SetLongTime
 * @brief Sets the threshold used for long-release detection.
 * @param p_owner Pointer to the keypad instance.
 * @param long_time1ms Long-release threshold in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetLongTime(PifKeypad* p_owner, uint16_t long_time1ms);

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief Sets the interval used for double-press detection.
 * @param p_owner Pointer to the keypad instance.
 * @param double_time1ms Double-press interval in milliseconds.
 * @return `TRUE` if the value is valid, otherwise `FALSE`.
 */
BOOL pifKeypad_SetDoubleTime(PifKeypad* p_owner, uint16_t double_time1ms);

/**
 * @fn pifKeypad_Start
 * @brief Starts or resumes periodic keypad scanning.
 * @param p_owner Pointer to the keypad instance.
 * @param p_name Optional task name. If `NULL`, `"Keypad"` is used.
 * @return `TRUE` if scanning starts successfully, otherwise `FALSE`.
 */
BOOL pifKeypad_Start(PifKeypad* p_owner, const char* p_name);

/**
 * @fn pifKeypad_Stop
 * @brief Pauses periodic keypad scanning.
 * @param p_owner Pointer to the keypad instance.
 */
void pifKeypad_Stop(PifKeypad* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_KEYPAD_H
