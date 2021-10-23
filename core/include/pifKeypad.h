#ifndef PIF_KEYPAD_H
#define PIF_KEYPAD_H


#include "pifTask.h"


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
 * @brief
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
 * @brief Keypad 관리용 구조체
 */
typedef struct StPifKeypad
{
	// Public Member Variable

	// Public Event Function
	PifEvtKeypadPressed evt_pressed;				// Default: NULL
	PifEvtKeypadReleased evt_released;				// Default: NULL
	PifEvtKeypadLongReleased evt_long_released;		// Default: NULL
	PifEvtKeypadDoublePressed evt_double_pressed;	// Default: NULL

	// Read-only Member Variable
	PifId _id;
	uint16_t _hold_time1ms;							// Default: PIF_KEYPAD_DEFAULT_HOLD_TIME
	uint16_t _long_time1ms;							// Default: PIF_KEYPAD_DEFAULT_LONG_TIME
	uint16_t _double_time1ms;						// Default: PIF_KEYPAD_DEFAULT_DOUBLE_TIME

	// Private Member Variable
	const char* __p_user_keymap;
	uint8_t __num_rows;
	uint8_t __num_cols;
	uint16_t* __p_state;
	PifKey* __p_key;

	// Private Action Function
	PifActKeypadAcquire __act_acquire;				// Default: NULL
} PifKeypad;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifKeypad_Create
 * @brief
 * @param id
 * @param num_rows
 * @param num_cols
 * @param p_user_keymap
 * @return
 */
PifKeypad* pifKeypad_Create(PifId id, uint8_t num_rows, uint8_t num_cols, const char* p_user_keymap);

/**
 * @fn pifKeypad_Destroy
 * @brief
 * @param pp_owner
 */
void pifKeypad_Destroy(PifKeypad** pp_owner);

/**
 * @fn pifKeypad_Init
 * @brief
 * @param p_owner
 * @param id
 * @param num_rows
 * @param num_cols
 * @param p_user_keymap
 * @return
 */
BOOL pifKeypad_Init(PifKeypad* p_owner, PifId id, uint8_t num_rows, uint8_t num_cols, const char* p_user_keymap);

/**
 * @fn pifKeypad_Clear
 * @brief
 * @param p_owner
 */
void pifKeypad_Clear(PifKeypad* p_owner);

/**
 * @fn pifKeypad_AttachAction
 * @brief
 * @param p_owner
 * @param act_acquire
 */
void pifKeypad_AttachAction(PifKeypad* p_owner, PifActKeypadAcquire act_acquire);

/**
 * @fn pifKeypad_SetHoldTime
 * @brief
 * @param p_owner
 * @param hold_time1ms
 * @return
 */
BOOL pifKeypad_SetHoldTime(PifKeypad* p_owner, uint16_t hold_time1ms);

/**
 * @fn pifKeypad_SetLongTime
 * @brief
 * @param p_owner
 * @param long_time1ms
 * @return
 */
BOOL pifKeypad_SetLongTime(PifKeypad* p_owner, uint16_t long_time1ms);

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief
 * @param p_owner
 * @param double_time1ms
 * @return
 */
BOOL pifKeypad_SetDoubleTime(PifKeypad* p_owner, uint16_t double_time1ms);

/**
 * @fn pifKeypad_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifKeypad_AttachTask(PifKeypad* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif	// PIF_KEYPAD_H
