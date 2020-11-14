#include "pifKeypad.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stKeypad s_stKeyPad;


static void _CheckKeyState(int idx, BOOL button)
{
	uint32_t unTime;
	PIF_stKey *pstKey = &s_stKeyPad.__pstKey[idx];

	switch (pstKey->enState) {
	case KS_enIdle:
		if (button == ON) {
			pstKey->bPressed = FALSE;
			pstKey->bLongReleased = FALSE;
			pstKey->unPressedTime = pif_unTimer1sec * 1000 + pif_usTimer1ms;
			if (s_stKeyPad.evtDoublePressed) {
				if (!pstKey->unFirstTime) {
					pstKey->unFirstTime = pstKey->unPressedTime;
					pstKey->bDoublePressed = FALSE;
				}
				else {
					if (pstKey->unPressedTime - pstKey->unFirstTime < s_stKeyPad.__usDoubleTimeMs) {
						(*s_stKeyPad.evtDoublePressed)(s_stKeyPad.pcUserKeymap[idx]);
						pstKey->bDoublePressed = TRUE;
					}
				}
			}
			pstKey->enState = KS_enPressed;
		}
		else if (pstKey->bShortClicked) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeyPad.__usDoubleTimeMs) {
				if (s_stKeyPad.evtPressed)  {
					(*s_stKeyPad.evtPressed)(s_stKeyPad.pcUserKeymap[idx]);
				}
				if (s_stKeyPad.evtReleased)  {
					(*s_stKeyPad.evtReleased)(s_stKeyPad.pcUserKeymap[idx], unTime);
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
			if (unTime >= s_stKeyPad.__usHoldTimeMs) {
				pstKey->enState = KS_enHold;
			}
		}
		break;

	case KS_enHold:
		if (!pstKey->bDoublePressed) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeyPad.__usLongTimeMs) {
				if (!pstKey->bLongReleased) {
					if (s_stKeyPad.evtLongReleased)  {
						(*s_stKeyPad.evtLongReleased)(s_stKeyPad.pcUserKeymap[idx], unTime);
					}
					pstKey->bLongReleased = TRUE;
				}
			}
			else if (unTime >= s_stKeyPad.__usDoubleTimeMs) {
				if (!pstKey->bPressed) {
					if (s_stKeyPad.evtPressed)  {
						(*s_stKeyPad.evtPressed)(s_stKeyPad.pcUserKeymap[idx]);
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
		if (!pstKey->bLongReleased && s_stKeyPad.evtReleased)  {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			(*s_stKeyPad.evtReleased)(s_stKeyPad.pcUserKeymap[idx], unTime);
		}
		pstKey->enState = KS_enIdle;
		break;
	}
}

/**
 * @fn pifKeypad_Init
 * @brief
 * @param pcUserKeymap
 * @param ucNumRows
 * @param ucNumCols
 * @return
 */
PIF_stKeypad *pifKeypad_Init(const char *pcUserKeymap, uint8_t ucNumRows, uint8_t ucNumCols)
{
	int i;

	s_stKeyPad.__pstKey = calloc(sizeof(PIF_stKey), ucNumRows * ucNumCols);
	if (!s_stKeyPad.__pstKey) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	s_stKeyPad.__pusState = calloc(sizeof(uint16_t), ucNumRows);
	if (!s_stKeyPad.__pusState) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	s_stKeyPad.pcUserKeymap = pcUserKeymap;
	s_stKeyPad.ucNumRows = ucNumRows;
	s_stKeyPad.ucNumCols = ucNumCols;
	s_stKeyPad.__usHoldTimeMs = PIF_KEYPAD_DEFAULT_HOLD_TIME;
	s_stKeyPad.__usLongTimeMs = PIF_KEYPAD_DEFAULT_LONG_TIME;
	s_stKeyPad.__usDoubleTimeMs = PIF_KEYPAD_DEFAULT_DOUBLE_TIME;

	s_stKeyPad.actAcquire = NULL;

	s_stKeyPad.evtPressed = NULL;
	s_stKeyPad.evtReleased = NULL;
	s_stKeyPad.evtLongReleased = NULL;
	s_stKeyPad.evtDoublePressed = NULL;

	for (i = 0; i < ucNumRows * ucNumCols; i++) {
		s_stKeyPad.__pstKey[i].enState = KS_enIdle;
		s_stKeyPad.__pstKey[i].bPressed = FALSE;
		s_stKeyPad.__pstKey[i].bShortClicked = FALSE;
		s_stKeyPad.__pstKey[i].bDoublePressed = FALSE;
		s_stKeyPad.__pstKey[i].bLongReleased = FALSE;
	}
    return &s_stKeyPad;

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
	if (s_stKeyPad.__pusState) {
		free(s_stKeyPad.__pusState);
		s_stKeyPad.__pusState = NULL;
	}

	if (s_stKeyPad.__pstKey) {
		free(s_stKeyPad.__pstKey);
		s_stKeyPad.__pstKey = NULL;
	}
}

/**
 * @fn pifKeypad_GetHoldTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetHoldTime()
{
	return s_stKeyPad.__usHoldTimeMs;
}

/**
 * @fn pifKeypad_SetHoldTime
 * @brief
 * @param usHoldTimeMs
 * @return
 */
BOOL pifKeypad_SetHoldTime(uint16_t usHoldTimeMs)
{
	if (usHoldTimeMs >= s_stKeyPad.__usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPad.__usHoldTimeMs = usHoldTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_GetLongTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetLongTime()
{
	return s_stKeyPad.__usLongTimeMs;
}

/**
 * @fn pifKeypad_SetLongTime
 * @brief
 * @param usLongTimeMs
 * @return
 */
BOOL pifKeypad_SetLongTime(uint16_t usLongTimeMs)
{
	if (usLongTimeMs <= s_stKeyPad.__usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPad.__usLongTimeMs = usLongTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_GetDoubleTime
 * @brief
 * @return
 */
uint16_t pifKeypad_GetDoubleTime()
{
	return s_stKeyPad.__usDoubleTimeMs;
}

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief
 * @param usDoubleTimeMs
 * @return
 */
BOOL pifKeypad_SetDoubleTime(uint16_t usDoubleTimeMs)
{
	if (usDoubleTimeMs <= s_stKeyPad.__usHoldTimeMs || usDoubleTimeMs >= s_stKeyPad.__usLongTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeyPad.__usDoubleTimeMs = usDoubleTimeMs;
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

	if (!s_stKeyPad.__pstKey || !s_stKeyPad.actAcquire) return;

	(*s_stKeyPad.actAcquire)(s_stKeyPad.__pusState);

	for (idx = 0, r = 0; r < s_stKeyPad.ucNumRows; r++) {
		for (c = 0; c < s_stKeyPad.ucNumCols; c++, idx++) {
			_CheckKeyState(idx, (s_stKeyPad.__pusState[r] >> c) & 1);
		}
	}
}
