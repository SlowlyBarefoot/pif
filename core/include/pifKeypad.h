#ifndef PIF_KEYPAD_H
#define PIF_KEYPAD_H


#include "pifTask.h"


typedef enum _PIF_enKeyState
{
	KS_enIdle,
	KS_enPressed,
	KS_enHold,
	KS_enReleased
} PIF_enKeyState;


typedef void (*PIF_actKeypadAcquire)(uint16_t *pusState);
typedef void (*PIF_evtKeypadPressed)(char cChar);
typedef void (*PIF_evtKeypadDoublePressed)(char cChar);
typedef void (*PIF_evtKeypadReleased)(char cChar, uint32_t unOnTime);
typedef void (*PIF_evtKeypadLongReleased)(char cChar, uint32_t unOnTime);


/**
 * @class _PIF_stKey
 * @brief
 */
typedef struct _PIF_stKey
{
	PIF_enKeyState enState;
	BOOL bPressed;
	BOOL bShortClicked;
	BOOL bDoublePressed;
	BOOL bLongReleased;
	uint32_t unPressedTime;
	uint32_t unFirstTime;
} PIF_stKey;

/**
 * @class _PIF_stKeypad
 * @brief Keypad 관리용 구조체
 */
typedef struct _PIF_stKeypad
{
	// Public Member Variable

	// Public Event Function
	PIF_evtKeypadPressed evtPressed;				// Default: NULL
	PIF_evtKeypadReleased evtReleased;				// Default: NULL
	PIF_evtKeypadLongReleased evtLongReleased;		// Default: NULL
	PIF_evtKeypadDoublePressed evtDoublePressed;	// Default: NULL

	// Read-only Member Variable
	PIF_usId _usPifId;
	uint16_t _usHoldTimeMs;							// Default: PIF_KEYPAD_DEFAULT_HOLD_TIME
	uint16_t _usLongTimeMs;							// Default: PIF_KEYPAD_DEFAULT_LONG_TIME
	uint16_t _usDoubleTimeMs;						// Default: PIF_KEYPAD_DEFAULT_DOUBLE_TIME

	// Private Member Variable
	const char *__pcUserKeymap;
	uint8_t __ucNumRows;
	uint8_t __ucNumCols;
	uint16_t *__pusState;
	PIF_stKey *__pstKey;

	// Private Action Function
	PIF_actKeypadAcquire __actAcquire;				// Default: NULL
} PIF_stKeypad;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stKeypad *pifKeypad_Init(PIF_usId usPifId, uint8_t ucNumRows, uint8_t ucNumCols, const char *pcUserKeymap);
void pifKeypad_Exit();

void pifKeypad_AttachAction(PIF_actKeypadAcquire actAcquire);

BOOL pifKeypad_SetHoldTime(uint16_t usHoldTimeMs);
BOOL pifKeypad_SetLongTime(uint16_t usLongTimeMs);
BOOL pifKeypad_SetDoubleTime(uint16_t usDoubleTimeMs);

// Task Function
uint16_t pifKeypad_Task(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// PIF_KEYPAD_H
