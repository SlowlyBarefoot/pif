#include "pifFnd.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define PIF_FND_CONTROL_PERIOD_DEFAULT		25


const uint8_t c_aucFndNumber[] = {
		0x3F, /*  0  */	0x06, /*  1  */	0x5B, /*  2  */ 0x4F, /*  3  */ 	// 0x30
		0x66, /*  4  */ 0x6D, /*  5  */ 0x7D, /*  6  */ 0x07, /*  7  */		// 0x34
		0x7F, /*  8  */ 0x6F  /*  9  */ 									// 0x38
};

const uint8_t *c_pucFndUserChar;
static uint8_t s_ucFndUserCharCount = 0;


static void _evtTimerBlinkFinish(void *pvIssuer)
{
    PIF_stFnd *pstOwner = (PIF_stFnd *)pvIssuer;
    pstOwner->__bt.Blink ^= 1;
}

/**
 * @fn pifFnd_SetUserChar
 * @brief
 * @param pucUserChar
 * @param ucCount
 */
void pifFnd_SetUserChar(const uint8_t *pucUserChar, uint8_t ucCount)
{
	c_pucFndUserChar = pucUserChar;
	s_ucFndUserCharCount = ucCount;
}

/**
 * @fn pifFnd_Create
 * @brief
 * @param usPifId
 * @param pstTimer
 * @param ucDigitSize
 * @param actDisplay
 * @return
 */
PIF_stFnd *pifFnd_Create(PifId usPifId, PifPulse *pstTimer, uint8_t ucDigitSize, PIF_actFndDisplay actDisplay)
{
    PIF_stFnd *pstOwner = NULL;

    if (!pstTimer || !ucDigitSize || !actDisplay) {
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    pstOwner = calloc(sizeof(PIF_stFnd), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    pstOwner->__pcString = calloc(sizeof(uint8_t), ucDigitSize);
    if (!pstOwner->__pcString) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}
    for (int i = 0; i < ucDigitSize; i++) pstOwner->__pcString[i] = 0x20;

    pstOwner->__pstTimer = pstTimer;
    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    pstOwner->__usControlPeriodMs = PIF_FND_CONTROL_PERIOD_DEFAULT / ucDigitSize;
    pstOwner->_ucDigitSize = ucDigitSize;
    pstOwner->__actDisplay = actDisplay;
    pstOwner->__pstTimerBlink = NULL;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
    return NULL;
}

/**
 * @fn pifFnd_Destroy
 * @brief
 * @param pp_owner
 */
void pifFnd_Destroy(PIF_stFnd** pp_owner)
{
	if (*pp_owner) {
		PIF_stFnd* pstOwner = *pp_owner;
		if (pstOwner->__pcString) {
			free(pstOwner->__pcString);
			pstOwner->__pcString = NULL;
		}
		if (pstOwner->__pstTimerBlink) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__pstTimerBlink);
		}
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifFnd_SetControlPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifFnd_SetControlPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs)
{
    if (!usPeriodMs || usPeriodMs < pstOwner->_ucDigitSize) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    pstOwner->__usControlPeriodMs = usPeriodMs / pstOwner->_ucDigitSize;
    return TRUE;
}

/**
 * @fn pifFnd_Start
 * @brief
 * @param pstOwner
 */
void pifFnd_Start(PIF_stFnd *pstOwner)
{
	pstOwner->__bt.Run = TRUE;
}

/**
 * @fn pifFnd_Stop
 * @brief
 * @param pstOwner
 */
void pifFnd_Stop(PIF_stFnd *pstOwner)
{
	int i;

	for (i = 0; i < pstOwner->_ucDigitSize; i++) {
		(*pstOwner->__actDisplay)(0, 1 << i);
	}
	pstOwner->__bt.Run = FALSE;
    if (pstOwner->__bt.Blink) {
		pifPulse_StopItem(pstOwner->__pstTimerBlink);
		pstOwner->__bt.Blink = FALSE;
    }
}

/**
 * @fn pifFnd_BlinkOn
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifFnd_BlinkOn(PIF_stFnd *pstOwner, uint16_t usPeriodMs)
{
	if (!usPeriodMs) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	if (!pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink = pifPulse_AddItem(pstOwner->__pstTimer, PT_REPEAT);
        if (!pstOwner->__pstTimerBlink) return FALSE;
        pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _evtTimerBlinkFinish, pstOwner);
    }
    if (!pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs * 1000L / pstOwner->__pstTimer->_period1us)) return FALSE;
    return TRUE;
}

/**
 * @fn pifFnd_BlinkOff
 * @brief
 * @param pstOwner
 */
void pifFnd_BlinkOff(PIF_stFnd *pstOwner)
{
	pstOwner->__bt.Blink = FALSE;
	if (pstOwner->__pstTimerBlink) {
		pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__pstTimerBlink);
		pstOwner->__pstTimerBlink = NULL;
	}
}

/**
 * @fn pifFnd_ChangeBlinkPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 * @return
 */
BOOL pifFnd_ChangeBlinkPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs)
{
	if (!usPeriodMs) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!pstOwner->__pstTimerBlink || pstOwner->__pstTimerBlink->_step == PS_STOP) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	pstOwner->__pstTimerBlink->target = usPeriodMs * 1000 / pstOwner->__pstTimer->_period1us;
	return TRUE;
}

/**
 * @fn pifFnd_SetFillZero
 * @brief
 * @param pstOwner
 * @param bFillZero
 */
void pifFnd_SetFillZero(PIF_stFnd *pstOwner, BOOL bFillZero)
{
    pstOwner->__bt.FillZero = bFillZero;
}

/**
 * @fn pifFnd_SetFloat
 * @brief
 * @param pstOwner
 * @param dValue
 */
void pifFnd_SetFloat(PIF_stFnd *pstOwner, double dValue)
{
    BOOL minus = FALSE;

    if (dValue < 0.0) {
    	minus = TRUE;
    	dValue *= -1.0;
    }
    uint32_t nValue = (uint32_t)dValue;
    int sp = pstOwner->_ucDigitSize;
    if (pstOwner->ucSubNumericDigits) {
    	dValue -= nValue;
    	for (int p = sp - pstOwner->ucSubNumericDigits; p < sp; p++) {
    		dValue *= 10;
    		uint32_t sd = (uint32_t)dValue;
    		pstOwner->__pcString[p] = '0' + sd;
    		dValue -= sd;
    	}
    	sp -= pstOwner->ucSubNumericDigits;
    }
    sp--;
	BOOL first = TRUE;
	for (int p = sp; p >= 0; p--) {
		if (!first && !nValue) {
			if (minus) {
				pstOwner->__pcString[p] = '-';
				minus = FALSE;
			}
			else {
				pstOwner->__pcString[p] = 0x20;
			}
		}
		else {
			uint8_t digit = nValue % 10;
			pstOwner->__pcString[p] = '0' + digit;
			if (digit) first = FALSE;
		}
		nValue = nValue / 10;
	}
	pstOwner->__pcString[pstOwner->_ucDigitSize - 1 - pstOwner->ucSubNumericDigits] |= 0x80;
	if (nValue || minus) {
		pstOwner->__pcString[pstOwner->_ucDigitSize - 1] = '_';
	}
}

/**
 * @fn pifFnd_SetInterger
 * @brief
 * @param pstOwner
 * @param nValue
 */
void pifFnd_SetInterger(PIF_stFnd *pstOwner, int32_t nValue)
{
    BOOL minus = FALSE;

    if (nValue < 0) {
    	minus = TRUE;
    	nValue *= -1;
    }
    int sp = pstOwner->_ucDigitSize - 1;
    if (pstOwner->ucSubNumericDigits) {
    	for (int p = sp; p >= sp - pstOwner->ucSubNumericDigits; p--) {
    		pstOwner->__pcString[p] = '0';
    	}
    	sp -= pstOwner->ucSubNumericDigits;
    }
    if (pstOwner->__bt.FillZero) {
        for (int p = sp; p >= minus; p--) {
			uint8_t digit = nValue % 10;
			pstOwner->__pcString[p] = '0' + digit;
        	nValue = nValue / 10;
        }
        if (minus) {
        	pstOwner->__pcString[0] = '-';
        }
        if (nValue) {
        	pstOwner->__pcString[pstOwner->_ucDigitSize - 1] = '_';
        }
    }
    else {
        BOOL first = TRUE;
        for (int p = sp; p >= 0; p--) {
        	if (!first && !nValue) {
        		if (minus) {
        			pstOwner->__pcString[p] = '-';
            		minus = FALSE;
        		}
        		else {
        			pstOwner->__pcString[p] = 0x20;
        		}
        	}
        	else {
            	uint8_t digit = nValue % 10;
            	pstOwner->__pcString[p] = '0' + digit;
            	if (digit) first = FALSE;
        	}
        	nValue = nValue / 10;
        }
        if (nValue || minus) {
        	pstOwner->__pcString[pstOwner->_ucDigitSize - 1] = '_';
        }
    }
}

/**
 * @fn pifFnd_SetString
 * @brief
 * @param pstOwner
 * @param pcString
 */
void pifFnd_SetString(PIF_stFnd *pstOwner, char *pcString)
{
    int src = 0;
    BOOL last = FALSE;

    for (int p = 0; p < pstOwner->_ucDigitSize; p++) {
    	if (!last && pcString[src]) {
    		pstOwner->__pcString[p] = pcString[src];
    	}
    	else {
    		pstOwner->__pcString[p] = 0x20;
    		last = TRUE;
    	}
    	src++;
    }
}

static uint16_t _DoTask(PifTask *pstTask)
{
	PIF_stFnd *pstOwner = pstTask->_p_client;
	uint8_t ch, seg = 0;
	BOOL bPoint = FALSE;

	if (!pstOwner->__bt.Run) return 0;

	if (pif_timer1ms > pstOwner->__usPretimeMs) {
		if (pif_timer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return 0;
	}
	else if (pif_timer1ms < pstOwner->__usPretimeMs) {
		if (1000 + pif_timer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return 0;
	}
	else return 0;

	if (!pstOwner->__bt.Blink) {
		ch = pstOwner->__pcString[pstOwner->__ucDigitIndex];
		if (ch & 0x80) {
			bPoint = TRUE;
			ch &= 0x7F;
		}
		if (ch >= '0' && ch <= '9') {
			seg = c_aucFndNumber[ch - '0'];
		}
		else if (ch == '-') {
			seg = 0x40;
		}
		else if (s_ucFndUserCharCount && ch >= 'A' && ch < 'A' + s_ucFndUserCharCount) {
			seg = c_pucFndUserChar[ch - 'A'];
		}
		if (bPoint) seg |= 0x80;
		(*pstOwner->__actDisplay)(seg, pstOwner->__ucDigitIndex);
	}
	else {
		(*pstOwner->__actDisplay)(0, pstOwner->__ucDigitIndex);
	}
	pstOwner->__ucDigitIndex++;
	if (pstOwner->__ucDigitIndex >= pstOwner->_ucDigitSize) pstOwner->__ucDigitIndex = 0;

	pstOwner->__usPretimeMs = pif_timer1ms;
	return 0;
}

/**
 * @fn pifFnd_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask *pifFnd_AttachTask(PIF_stFnd *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}
