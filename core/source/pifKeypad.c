#include "pifKeypad.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static void _CheckKeyState(PIF_stKeypad* p_owner, int idx, BOOL button)
{
	uint32_t unTime;
	PIF_stKey *pstKey = &p_owner->__pstKey[idx];

	switch (pstKey->enState) {
	case KS_enIdle:
		if (button == ON) {
			pstKey->bPressed = FALSE;
			pstKey->bLongReleased = FALSE;
			pstKey->unPressedTime = pif_timer1sec * 1000 + pif_timer1ms;
			if (p_owner->evtDoublePressed) {
				if (!pstKey->unFirstTime) {
					pstKey->unFirstTime = pstKey->unPressedTime;
					pstKey->bDoublePressed = FALSE;
				}
				else {
					if (pstKey->unPressedTime - pstKey->unFirstTime < p_owner->_usDoubleTimeMs) {
						(*p_owner->evtDoublePressed)(p_owner->__pcUserKeymap[idx]);
						pstKey->bDoublePressed = TRUE;
					}
				}
			}
			pstKey->enState = KS_enPressed;
		}
		else if (pstKey->bShortClicked) {
			unTime = pif_timer1sec * 1000 + pif_timer1ms - pstKey->unPressedTime;
			if (unTime >= p_owner->_usDoubleTimeMs) {
				if (p_owner->evtPressed)  {
					(*p_owner->evtPressed)(p_owner->__pcUserKeymap[idx]);
				}
				if (p_owner->evtReleased)  {
					(*p_owner->evtReleased)(p_owner->__pcUserKeymap[idx], unTime);
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
			unTime = pif_timer1sec * 1000 + pif_timer1ms - pstKey->unPressedTime;
			if (unTime >= p_owner->_usHoldTimeMs) {
				pstKey->enState = KS_enHold;
			}
		}
		break;

	case KS_enHold:
		if (!pstKey->bDoublePressed) {
			unTime = pif_timer1sec * 1000 + pif_timer1ms - pstKey->unPressedTime;
			if (unTime >= p_owner->_usLongTimeMs) {
				if (!pstKey->bLongReleased) {
					if (p_owner->evtLongReleased)  {
						(*p_owner->evtLongReleased)(p_owner->__pcUserKeymap[idx], unTime);
					}
					pstKey->bLongReleased = TRUE;
				}
			}
			else if (unTime >= p_owner->_usDoubleTimeMs) {
				if (!pstKey->bPressed) {
					if (p_owner->evtPressed)  {
						(*p_owner->evtPressed)(p_owner->__pcUserKeymap[idx]);
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
		if (!pstKey->bLongReleased && p_owner->evtReleased)  {
			unTime = pif_timer1sec * 1000 + pif_timer1ms - pstKey->unPressedTime;
			(*p_owner->evtReleased)(p_owner->__pcUserKeymap[idx], unTime);
		}
		pstKey->enState = KS_enIdle;
		break;
	}
}

/**
 * @fn pifKeypad_Create
 * @brief
 * @param usPifId
 * @param ucNumRows
 * @param ucNumCols
 * @param pcUserKeymap
 * @return
 */
PIF_stKeypad *pifKeypad_Create(PifId usPifId, uint8_t ucNumRows, uint8_t ucNumCols, const char *pcUserKeymap)
{
	PIF_stKeypad* p_owner;

	p_owner = calloc(sizeof(PIF_stKeypad), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	p_owner->__pstKey = calloc(sizeof(PIF_stKey), ucNumRows * ucNumCols);
	if (!p_owner->__pstKey) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	p_owner->__pusState = calloc(sizeof(uint16_t), ucNumRows);
	if (!p_owner->__pusState) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    p_owner->_usPifId = usPifId;
    p_owner->__pcUserKeymap = pcUserKeymap;
    p_owner->__ucNumRows = ucNumRows;
    p_owner->__ucNumCols = ucNumCols;
    p_owner->_usHoldTimeMs = PIF_KEYPAD_DEFAULT_HOLD_TIME;
    p_owner->_usLongTimeMs = PIF_KEYPAD_DEFAULT_LONG_TIME;
    p_owner->_usDoubleTimeMs = PIF_KEYPAD_DEFAULT_DOUBLE_TIME;
    return p_owner;
}

/**
 * @fn pifKeypad_Destroy
 * @brief
 * @param pp_owner
 */
void pifKeypad_Destroy(PIF_stKeypad** pp_owner)
{
	if (*pp_owner) {
		PIF_stKeypad* p_owner = *pp_owner;
		if (p_owner->__pusState) free(p_owner->__pusState);
		if (p_owner->__pstKey) free(p_owner->__pstKey);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifKeypad_AttachAction
 * @brief
 * @param p_owner
 * @param actAcquire
 */
void pifKeypad_AttachAction(PIF_stKeypad* p_owner, PIF_actKeypadAcquire actAcquire)
{
	p_owner->__actAcquire = actAcquire;
}

/**
 * @fn pifKeypad_SetHoldTime
 * @brief
 * @param p_owner
 * @param usHoldTimeMs
 * @return
 */
BOOL pifKeypad_SetHoldTime(PIF_stKeypad* p_owner, uint16_t usHoldTimeMs)
{
	if (usHoldTimeMs >= p_owner->_usDoubleTimeMs) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_usHoldTimeMs = usHoldTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_SetLongTime
 * @brief
 * @param p_owner
 * @param usLongTimeMs
 * @return
 */
BOOL pifKeypad_SetLongTime(PIF_stKeypad* p_owner, uint16_t usLongTimeMs)
{
	if (usLongTimeMs <= p_owner->_usDoubleTimeMs) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_usLongTimeMs = usLongTimeMs;
	return TRUE;
}

/**
 * @fn pifKeypad_SetDoubleTime
 * @brief
 * @param p_owner
 * @param usDoubleTimeMs
 * @return
 */
BOOL pifKeypad_SetDoubleTime(PIF_stKeypad* p_owner, uint16_t usDoubleTimeMs)
{
	if (usDoubleTimeMs <= p_owner->_usHoldTimeMs || usDoubleTimeMs >= p_owner->_usLongTimeMs) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_usDoubleTimeMs = usDoubleTimeMs;
	return TRUE;
}

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	int idx, r, c;

	PIF_stKeypad* p_owner = pstTask->_pvClient;

	if (!p_owner->__pstKey || !p_owner->__actAcquire) return 0;

	(*p_owner->__actAcquire)(p_owner->__pusState);

	for (idx = 0, r = 0; r < p_owner->__ucNumRows; r++) {
		for (c = 0; c < p_owner->__ucNumCols; c++, idx++) {
			_CheckKeyState(p_owner, idx, (p_owner->__pusState[r] >> c) & 1);
		}
	}
	return 0;
}

/**
 * @fn pifKeypad_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifKeypad_AttachTask(PIF_stKeypad* p_owner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, p_owner, bStart);
}
