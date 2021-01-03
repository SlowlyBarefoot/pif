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
	// Public Member Variable
	PIF_unDeviceCode unDeviceCode;
} PIF_stKeypad;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stKeypad *pifKeypad_Init(PIF_unDeviceCode unDeviceCode, uint8_t ucNumRows, uint8_t ucNumCols, const char *pcUserKeymap);
void pifKeypad_Exit();

void pifKeypad_AttachAction(PIF_actKeypadAcquire actAcquire);
void pifKeypad_AttachEvent(PIF_evtKeypadPressed evtPressed, PIF_evtKeypadReleased evtReleased,
		PIF_evtKeypadLongReleased evtLongReleased, PIF_evtKeypadDoublePressed evtDoublePressed);

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
