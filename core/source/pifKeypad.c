#include "pifKeypad.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stKeypad s_stKeypad;


static void _CheckKeyState(int idx, BOOL button)
{
	uint32_t unTime;
	PIF_stKey *pstKey = &s_stKeypad.__pstKey[idx];

	switch (pstKey->enState) {
	case KS_enIdle:
		if (button == ON) {
			pstKey->bPressed = FALSE;
			pstKey->bLongReleased = FALSE;
			pstKey->unPressedTime = pif_unTimer1sec * 1000 + pif_usTimer1ms;
			if (s_stKeypad.evtDoublePressed) {
				if (!pstKey->unFirstTime) {
					pstKey->unFirstTime = pstKey->unPressedTime;
					pstKey->bDoublePressed = FALSE;
				}
				else {
					if (pstKey->unPressedTime - pstKey->unFirstTime < s_stKeypad._usDoubleTimeMs) {
						(*s_stKeypad.evtDoublePressed)(s_stKeypad.__pcUserKeymap[idx]);
						pstKey->bDoublePressed = TRUE;
					}
				}
			}
			pstKey->enState = KS_enPressed;
		}
		else if (pstKey->bShortClicked) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeypad._usDoubleTimeMs) {
				if (s_stKeypad.evtPressed)  {
					(*s_stKeypad.evtPressed)(s_stKeypad.__pcUserKeymap[idx]);
				}
				if (s_stKeypad.evtReleased)  {
					(*s_stKeypad.evtReleased)(s_stKeypad.__pcUserKeymap[idx], unTime);
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
			if (unTime >= s_stKeypad._usHoldTimeMs) {
				pstKey->enState = KS_enHold;
			}
		}
		break;

	case KS_enHold:
		if (!pstKey->bDoublePressed) {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			if (unTime >= s_stKeypad._usLongTimeMs) {
				if (!pstKey->bLongReleased) {
					if (s_stKeypad.evtLongReleased)  {
						(*s_stKeypad.evtLongReleased)(s_stKeypad.__pcUserKeymap[idx], unTime);
					}
					pstKey->bLongReleased = TRUE;
				}
			}
			else if (unTime >= s_stKeypad._usDoubleTimeMs) {
				if (!pstKey->bPressed) {
					if (s_stKeypad.evtPressed)  {
						(*s_stKeypad.evtPressed)(s_stKeypad.__pcUserKeymap[idx]);
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
		if (!pstKey->bLongReleased && s_stKeypad.evtReleased)  {
			unTime = pif_unTimer1sec * 1000 + pif_usTimer1ms - pstKey->unPressedTime;
			(*s_stKeypad.evtReleased)(s_stKeypad.__pcUserKeymap[idx], unTime);
		}
		pstKey->enState = KS_enIdle;
		break;
	}
}

/**
 * @fn pifKeypad_Init
 * @brief
 * @param usPifId
 * @param ucNumRows
 * @param ucNumCols
 * @param pcUserKeymap
 * @return
 */
PIF_stKeypad *pifKeypad_Init(PIF_usId usPifId, uint8_t ucNumRows, uint8_t ucNumCols, const char *pcUserKeymap)
{
	int i;

	s_stKeypad.__pstKey = calloc(sizeof(PIF_stKey), ucNumRows * ucNumCols);
	if (!s_stKeypad.__pstKey) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	s_stKeypad.__pusState = calloc(sizeof(uint16_t), ucNumRows);
	if (!s_stKeypad.__pusState) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
	s_stKeypad._usPifId = usPifId;
	s_stKeypad.__pcUserKeymap = pcUserKeymap;
	s_stKeypad.__ucNumRows = ucNumRows;
	s_stKeypad.__ucNumCols = ucNumCols;
	s_stKeypad._usHoldTimeMs = PIF_KEYPAD_DEFAULT_HOLD_TIME;
	s_stKeypad._usLongTimeMs = PIF_KEYPAD_DEFAULT_LONG_TIME;
	s_stKeypad._usDoubleTimeMs = PIF_KEYPAD_DEFAULT_DOUBLE_TIME;

	s_stKeypad.__actAcquire = NULL;

	s_stKeypad.evtPressed = NULL;
	s_stKeypad.evtReleased = NULL;
	s_stKeypad.evtLongReleased = NULL;
	s_stKeypad.evtDoublePressed = NULL;

	for (i = 0; i < ucNumRows * ucNumCols; i++) {
		s_stKeypad.__pstKey[i].enState = KS_enIdle;
		s_stKeypad.__pstKey[i].bPressed = FALSE;
		s_stKeypad.__pstKey[i].bShortClicked = FALSE;
		s_stKeypad.__pstKey[i].bDoublePressed = FALSE;
		s_stKeypad.__pstKey[i].bLongReleased = FALSE;
	}
    return &s_stKeypad;

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
	if (s_stKeypad.__pusState) {
		free(s_stKeypad.__pusState);
		s_stKeypad.__pusState = NULL;
	}

	if (s_stKeypad.__pstKey) {
		free(s_stKeypad.__pstKey);
		s_stKeypad.__pstKey = NULL;
	}
}

/**
 * @fn pifKeypad_AttachAction
 * @brief
 * @param actAcquire
 */
void pifKeypad_AttachAction(PIF_actKeypadAcquire actAcquire)
{
	s_stKeypad.__actAcquire = actAcquire;
}

/**
 * @fn pifKeypad_SetHoldTime
 * @brief
 * @param usHoldTimeMs
 * @return
 */
BOOL pifKeypad_SetHoldTime(uint16_t usHoldTimeMs)
{
	if (usHoldTimeMs >= s_stKeypad._usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeypad._usHoldTimeMs = usHoldTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_SetLongTime
 * @brief
 * @param usLongTimeMs
 * @return
 */
BOOL pifKeypad_SetLongTime(uint16_t usLongTimeMs)
{
	if (usLongTimeMs <= s_stKeypad._usDoubleTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeypad._usLongTimeMs = usLongTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief
 * @param usDoubleTimeMs
 * @return
 */
BOOL pifKeypad_SetDoubleTime(uint16_t usDoubleTimeMs)
{
	if (usDoubleTimeMs <= s_stKeypad._usHoldTimeMs || usDoubleTimeMs >= s_stKeypad._usLongTimeMs) {
		pif_enError = E_enInvalidParam;
		return FALSE;
	}

	s_stKeypad._usDoubleTimeMs = usDoubleTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_taskAll
 * @brief
 * @param pstTask
 * @return
 */
uint16_t pifKeypad_taskAll(PIF_stTask *pstTask)
{
	int idx, r, c;

	(void)pstTask;

	if (!s_stKeypad.__pstKey || !s_stKeypad.__actAcquire) return;

	(*s_stKeypad.__actAcquire)(s_stKeypad.__pusState);

	for (idx = 0, r = 0; r < s_stKeypad.__ucNumRows; r++) {
		for (c = 0; c < s_stKeypad.__ucNumCols; c++, idx++) {
			_CheckKeyState(idx, (s_stKeypad.__pusState[r] >> c) & 1);
		}
	}
	return 0;
}
