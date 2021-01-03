#include "pifKeypad.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define PIF_KEYPAD_DEFAULT_HOLD_TIME	100
#define PIF_KEYPAD_DEFAULT_LONG_TIME	1000
#define PIF_KEYPAD_DEFAULT_DOUBLE_TIME	300


typedef enum _PIF_enKeyState
{
	KS_enIdle,
	KS_enPressed,
	KS_enHold,
	KS_enReleased
} PIF_enKeyState;


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

typedef struct _PIF_stKeypadBase
{
	// Public Member Variable
	PIF_stKeypad stOwner;

	// Private Member Variable
	const char *pcUserKeymap;
	uint8_t ucNumRows;
	uint8_t ucNumCols;
	uint16_t usHoldTimeMs;						// Default: PIF_KEYPAD_DEFAULT_HOLD_TIME
	uint16_t usLongTimeMs;						// Default: PIF_KEYPAD_DEFAULT_LONG_TIME
	uint16_t usDoubleTimeMs;					// Default: PIF_KEYPAD_DEFAULT_DOUBLE_TIME
	uint16_t *pusState;
	PIF_stKey *pstKey;

	// Public Action Function
	PIF_actKeypadAcquire actAcquire;				// Default: NULL

	// Public Event Function
	PIF_evtKeypadPressed evtPressed;				// Default: NULL
	PIF_evtKeypadReleased evtReleased;				// Default: NULL
	PIF_evtKeypadLongReleased evtLongReleased;		// Default: NULL
	PIF_evtKeypadDoublePressed evtDoublePressed;	// Default: NULL
} PIF_stKeypadBase;


static PIF_stKeypadBase s_stKeyPadBase;


static void _CheckKeyState(int idx, BOOL button)
{
	uint32_t unTime;
	PIF_stKey *pstKey = &s_stKeyPadBase.pstKey[idx];

	switch (pstKey->enState) {
	case KS_enIdle:
		if (button == ON) {
			pstKey->bPressed = FALSE;
			pstKey->bLongReleased = FALSE;
			pstKey->unPressedTime = pif_unTimer1sec * 1000 + pif_usTimer1ms;
			if (s_stKeyPadBase.evtDoublePressed) {
				if (!pstKey->unFirstTime) {
					pstKey->unFirstTime = pstKey->unPressedTime;
					pstKey->bDoublePressed = FALSE;
				}
				else {
					if (pstKey->unPressedTime - pstKey->unFirstTime < s_stKeyPadBase.usDoubleTimeMs) {
						(*s_stKeyPadBase.evtDoublePressed)(s_stKeyPadBase.pcUserKeymap[idx]);
						pstKey->bDoublePressed = TRUE;
					}
				}
			}
			pstKey->enState = KS_enPressed;
		}
		else if (pstKey->bShortClicked) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeyPadBase.usDoubleTimeMs) {
				if (s_stKeyPadBase.evtPressed)  {
					(*s_stKeyPadBase.evtPressed)(s_stKeyPadBase.pcUserKeymap[idx]);
				}
				if (s_stKeyPadBase.evtReleased)  {
					(*s_stKeyPadBase.evtReleased)(s_stKeyPadBase.pcUserKeymap[idx], unTime);
				}
				pstKey->bShortClicked = FALSE;
				pstKey->unFirstTime = 0L;
			}
		}
		break;

	case KS_enPressed:
		if (button == OFF) {
			if (pstKey->bDoublePressed)	{
				pstKey->unFirstTime = 0L;
				pstKey->enState = KS_enReleased;
			}
			else pstKey->enState = KS_enIdle;
		}
		else {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeyPadBase.usHoldTimeMs) {
				pstKey->enState = KS_enHold;
			}
		}
		break;

	case KS_enHold:
		if (!pstKey->bDoublePressed) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeyPadBase.usLongTimeMs) {
				if (!pstKey->bLongReleased) {
					if (s_stKeyPadBase.evtLongReleased)  {
						(*s_stKeyPadBase.evtLongReleased)(s_stKeyPadBase.pcUserKeymap[idx], unTime);
					}
					pstKey->bLongReleased = TRUE;
				}
			}
			else if (unTime >= s_stKeyPadBase.usDoubleTimeMs) {
				if (!pstKey->bPressed) {
					if (s_stKeyPadBase.evtPressed)  {
						(*s_stKeyPadBase.evtPressed)(s_stKeyPadBase.pcUserKeymap[idx]);
					}
					pstKey->bPressed = TRUE;
				}
			}
			if (button == OFF) {
				if (!pstKey->bPressed) {
					pstKey->bShortClicked = TRUE;
					pstKey->enState = KS_enIdle;
				}
				else {
					pstKey->enState = KS_enReleased;
				}
			}
		}
		else {
			if (button == OFF) {
				if (pstKey->bDoublePressed) {
					pstKey->bShortClicked = FALSE;
					pstKey->unFirstTime = 0L;
				}
				pstKey->enState = KS_enReleased;
			}
		}
		break;

	case KS_enReleased:
		if (!pstKey->bLongReleased && s_stKeyPadBase.evtReleased)  {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			(*s_stKeyPadBase.evtReleased)(s_stKeyPadBase.pcUserKeymap[idx], unTime);
		}
		pstKey->enState = KS_enIdle;
		break;
	}
}

/**
 * @fn pifKeypad_Init
 * @brief
 * @param unDeviceCode
 * @param ucNumRows
 * @param ucNumCols
 * @param pcUserKeymap
 * @return
 */
PIF_stKeypad *pifKeypad_Init(PIF_unDeviceCode unDeviceCode, uint8_t ucNumRows, uint8_t ucNumCols, const char *pcUserKeymap)
{
	int i;

	s_stKeyPadBase.pstKey = calloc(sizeof(PIF_stKey), ucNumRows * ucNumCols);
	if (!s_stKeyPadBase.pstKey) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	s_stKeyPadBase.pusState = calloc(sizeof(uint16_t), ucNumRows);
	if (!s_stKeyPadBase.pusState) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	s_stKeyPadBase.stOwner.unDeviceCode = unDeviceCode;
	s_stKeyPadBase.pcUserKeymap = pcUserKeymap;
	s_stKeyPadBase.ucNumRows = ucNumRows;
	s_stKeyPadBase.ucNumCols = ucNumCols;
	s_stKeyPadBase.usHoldTimeMs = PIF_KEYPAD_DEFAULT_HOLD_TIME;
	s_stKeyPadBase.usLongTimeMs = PIF_KEYPAD_DEFAULT_LONG_TIME;
	s_stKeyPadBase.usDoubleTimeMs = PIF_KEYPAD_DEFAULT_DOUBLE_TIME;

	s_stKeyPadBase.actAcquire = NULL;

	s_stKeyPadBase.evtPressed = NULL;
	s_stKeyPadBase.evtReleased = NULL;
	s_stKeyPadBase.evtLongReleased = NULL;
	s_stKeyPadBase.evtDoublePressed = NULL;

	for (i = 0; i < ucNumRows * ucNumCols; i++) {
		s_stKeyPadBase.pstKey[i].enState = KS_enIdle;
		s_stKeyPadBase.pstKey[i].bPressed = FALSE;
		s_stKeyPadBase.pstKey[i].bShortClicked = FALSE;
		s_stKeyPadBase.pstKey[i].bDoublePressed = FALSE;
		s_stKeyPadBase.pstKey[i].bLongReleased = FALSE;
	}
    return &s_stKeyPadBase.stOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Keypad:Init(R:%u C:%d) EC:%d", ucNumRows, ucNumCols, pif_enError);
#endif
	return NULL;
}

/**
 * @fn pifKeypad_Exit
 * @brief
 */
void pifKeypad_Exit()
{
	if (s_stKeyPadBase.pusState) {
		free(s_stKeyPadBase.pusState);
		s_stKeyPadBase.pusState = NULL;
	}

	if (s_stKeyPadBase.pstKey) {
		free(s_stKeyPadBase.pstKey);
		s_stKeyPadBase.pstKey = NULL;
	}
}

/**
 * @fn pifKeypad_AttachAction
 * @brief
 * @param actAcquire
 */
void pifKeypad_AttachAction(PIF_actKeypadAcquire actAcquire)
{
	s_stKeyPadBase.actAcquire = actAcquire;
}

/**
 * @fn pifKeypad_AttachAction
 * @brief
 * @param evtPressed
 * @param evtReleased
 * @param evtLongReleased
 * @param evtDoublePressed
 */
void pifKeypad_AttachEvent(PIF_evtKeypadPressed evtPressed, PIF_evtKeypadReleased evtReleased,
		PIF_evtKeypadLongReleased evtLongReleased, PIF_evtKeypadDoublePressed evtDoublePressed)
{
	s_stKeyPadBase.evtPressed = evtPressed;
	s_stKeyPadBase.evtReleased = evtReleased;
	s_stKeyPadBase.evtLongReleased = evtLongReleased;
	s_stKeyPadBase.evtDoublePressed = evtDoublePressed;
}

/**
 * @fn pifKeypad_GetHoldTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetHoldTime()
{
	return s_stKeyPadBase.usHoldTimeMs;
}

/**
 * @fn pifKeypad_SetHoldTime
 * @brief
 * @param usHoldTimeMs
 * @return
 */
BOOL pifKeypad_SetHoldTime(uint16_t usHoldTimeMs)
{
	if (usHoldTimeMs >= s_stKeyPadBase.usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPadBase.usHoldTimeMs = usHoldTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_GetLongTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetLongTime()
{
	return s_stKeyPadBase.usLongTimeMs;
}

/**
 * @fn pifKeypad_SetLongTime
 * @brief
 * @param usLongTimeMs
 * @return
 */
BOOL pifKeypad_SetLongTime(uint16_t usLongTimeMs)
{
	if (usLongTimeMs <= s_stKeyPadBase.usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPadBase.usLongTimeMs = usLongTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_GetDoubleTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetDoubleTime()
{
	return s_stKeyPadBase.usDoubleTimeMs;
}

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief
 * @param usDoubleTimeMs
 * @return
 */
BOOL pifKeypad_SetDoubleTime(uint16_t usDoubleTimeMs)
{
	if (usDoubleTimeMs <= s_stKeyPadBase.usHoldTimeMs || usDoubleTimeMs >= s_stKeyPadBase.usLongTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPadBase.usDoubleTimeMs = usDoubleTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_taskAll
 * @brief
 * @param pstTask
 */
void pifKeypad_taskAll(PIF_stTask *pstTask)
{
	int idx, r, c;

	(void)pstTask;

	if (!s_stKeyPadBase.pstKey || !s_stKeyPadBase.actAcquire) return;

	(*s_stKeyPadBase.actAcquire)(s_stKeyPadBase.pusState);

	for (idx = 0, r = 0; r < s_stKeyPadBase.ucNumRows; r++) {
		for (c = 0; c < s_stKeyPadBase.ucNumCols; c++, idx++) {
			_CheckKeyState(idx, (s_stKeyPadBase.pusState[r] >> c) & 1);
		}
	}
}
