#ifndef PIF_KEYPAD_H
#define PIF_KEYPAD_H


#include "pifTask.h"


typedef void (*PIF_actKeypadAcquire)(uint16_t *pusState);
typedef void (*PIF_evtKeypadPressed)(char cChar);
typedef void (*PIF_evtKeypadDoublePressed)(char cChar);
typedef void (*PIF_evtKeypadReleased)(char cChar, uint32_t unOnTime);
typedef void (*PIF_evtKeypadLongReleased)(char cChar, uint32_t unOnTime);


/**
 * @class _PIF_stKeypad
 * @brief Keypad 관리용 구조체
 */
typedef struct _PIF_stKeypad
{
	// Public Action Function
	PIF_actKeypadAcquire actAcquire;				// Default: NULL

	// Public Event Function
	PIF_evtKeypadPressed evtPressed;				// Default: NULL
	PIF_evtKeypadReleased evtReleased;				// Default: NULL
	PIF_evtKeypadLongReleased evtLongReleased;		// Default: NULL
	PIF_evtKeypadDoublePressed evtDoublePressed;	// Default: NULL
} PIF_stKeypad;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stKeypad *pifKeypad_Init(const char *pcUserKeymap, uint8_t ucNumRows, uint8_t ucNumCols);
void pifKeypad_Exit();

uint16_t pifKeypad_GetHoldTime();
BOOL pifKeypad_SetHoldTime(uint16_t usHoldTimeMs);

uint16_t pifKeypad_GetLongTime();
BOOL pifKeypad_SetLongTime(uint16_t usLongTimeMs);

uint16_t pifKeypad_GetDoubleTime();
BOOL pifKeypad_SetDoubleTime(uint16_t usDoubleTimeMs);

// Task Function
void pifKeypad_taskAll(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// PIF_KEYPAD_H
