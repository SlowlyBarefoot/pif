#include "pifFnd.h"
#include "pifLog.h"


static PIF_stFnd *s_pstFndArray = NULL;
static uint8_t s_ucFndArraySize;
static uint8_t s_ucFndArrayPos;

static PIF_stPulse *s_pstFndTimer;

const uint8_t c_aucFndAscii[] = {
		0x00, /*     */	0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 	// 0x20
		0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 0x00, /*     */		// 0x24
		0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 	// 0x28
		0x00, /*     */ 0x40, /*  -  */ 0x80, /*  .  */ 0x00, /*     */		// 0x2C
		0x3F, /*  0  */	0x06, /*  1  */	0x5B, /*  2  */ 0x4F, /*  3  */ 	// 0x30
		0x66, /*  4  */ 0x6D, /*  5  */ 0x7D, /*  6  */ 0x07, /*  7  */		// 0x34
		0x7F, /*  8  */ 0x6F, /*  9  */ 0x00, /*     */ 0x00, /*     */ 	// 0x38
		0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 0x00, /*     */		// 0x3C
		0x00, /*     */ 0x77, /*  A  */	0x7C, /*  b  */ 0x39, /*  C  */ 	// 0x40
		0x5E, /*  d  */ 0x79, /*  E  */ 0x71, /*  F  */ 0x3D, /*  G  */ 	// 0x44
		0x76, /*  H  */ 0x30, /*  I  */ 0x1E, /*  J  */ 0x7A, /*  K  */ 	// 0x48
		0x38, /*  L  */ 0x55, /*  m  */ 0x54, /*  n  */ 0x5C, /*  o  */		// 0x4C
		0x73, /*  P  */ 0x67, /*  q  */ 0x50, /*  r  */ 0x6D, /*  S  */		// 0x50
		0x78, /*  t  */ 0x3E, /*  U  */ 0x7E, /*  V  */ 0x6A, /*  W  */ 	// 0x54
		0x36, /*  X  */ 0x6E, /*  y  */ 0x49, /*  Z  */ 0x00, /*     */ 	// 0x58
		0x00, /*     */ 0x00, /*     */ 0x00, /*     */ 0x08  /*  _  */		// 0x5C
};


static void _TimerDisplayFinish(PIF_stFnd *pstOwner)
{
	uint8_t ch, seg;

	if (!pstOwner->btBlink) {
		ch = pstOwner->__pcString[pstOwner->__ucDigitIndex];
		if (ch & 0x80) {
			seg = c_aucFndAscii[ch - 0xA0] | 0x80;
		}
		else {
			seg = c_aucFndAscii[ch - 0x20];
		}
		(*pstOwner->__actDisplay)(seg, pstOwner->__ucDigitIndex, pstOwner->btColor);
	}
	else {
		(*pstOwner->__actDisplay)(0, pstOwner->__ucDigitIndex, 0);
	}
	pstOwner->__ucDigitIndex++;
	if (pstOwner->__ucDigitIndex >= pstOwner->ucDigitSize) pstOwner->__ucDigitIndex = 0;
}

static void _TimerBlinkFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stFnd *pstOwner = (PIF_stFnd *)pvIssuer;
    pstOwner->btBlink ^= 1;
}

static void _TaskCommon(PIF_stFnd *pstOwner)
{
	if (!pstOwner->btRun) return;

	if (pif_usTimer1ms > pstOwner->__usPretimeMs) {
		if (pif_usTimer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return;
	}
	else if (pif_usTimer1ms < pstOwner->__usPretimeMs) {
		if (1000 + pif_usTimer1ms - pstOwner->__usPretimeMs < pstOwner->__usControlPeriodMs) return;
	}
	else return;

	_TimerDisplayFinish(pstOwner);

	pstOwner->__usPretimeMs = pif_usTimer1ms;
}

/**
 * @fn pifFnd_Init
 * @brief
 * @param pstTimer 1ms Timer를 사용하여야 한다.
 * @param ucSize
 * @return
 */
BOOL pifFnd_Init(PIF_stPulse *pstTimer1ms, uint8_t ucSize)
{
    if (!pstTimer1ms || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstFndArray = calloc(sizeof(PIF_stFnd), ucSize);
    if (!s_pstFndArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucFndArraySize = ucSize;
    s_ucFndArrayPos = 0;

    s_pstFndTimer = pstTimer1ms;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Fnd:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifFnd_Exit
 * @brief
 */
void pifFnd_Exit()
{
	PIF_stFnd *pstOwner;

    if (s_pstFndArray) {
    	for (int i = 0; i < s_ucFndArrayPos; i++) {
    		pstOwner = &s_pstFndArray[i];
    		if (pstOwner->__pcString) {
    			free(pstOwner->__pcString);
    			pstOwner->__pcString = NULL;
    		}
    	}
    	free(s_pstFndArray);
        s_pstFndArray = NULL;
    }
}

/**
 * @fn pifFnd_Add
 * @brief
 * @param unDeviceCode
 * @param ucDigitSize
 * @param ucStringSize
 * @param actDisplay
 * @return
 */
PIF_stFnd *pifFnd_Add(PIF_unDeviceCode unDeviceCode, uint8_t ucDigitSize, uint8_t ucStringSize,	PIF_actFndDisplay actDisplay)
{
    if (s_ucFndArrayPos >= s_ucFndArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucDigitSize || !actDisplay) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stFnd *pstOwner = &s_pstFndArray[s_ucFndArrayPos];

	pstOwner->__pcString = calloc(sizeof(uint8_t), ucStringSize + ucDigitSize);
    if (!pstOwner->__pcString) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}
    for (int i = 0; i < ucStringSize; i++) pstOwner->__pcString[i] = 0x20;
    pstOwner->__ucStringSize = ucStringSize;

    pstOwner->unDeviceCode = unDeviceCode;
    pstOwner->__usControlPeriodMs = IDPF_FND_CONTROL_PERIOD_DEFAULT / ucDigitSize;
	pstOwner->ucDigitSize = ucDigitSize;
	pstOwner->__actDisplay = actDisplay;
    pstOwner->btColor = 1;
    pstOwner->__pstTimerBlink = NULL;

    s_ucFndArrayPos = s_ucFndArrayPos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "Fnd:AddSingle(D:%u DS:%u S:%u) EC:%d", unDeviceCode, ucDigitSize, ucStringSize, pif_enError);
    return NULL;
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
    if (!usPeriodMs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

   	pstOwner->__usControlPeriodMs = usPeriodMs;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Fnd:SetControlPeriod(P:%u) EC:%d", usPeriodMs, pif_enError);
    return FALSE;
}

/**
 * @fn pifFnd_Start
 * @brief
 * @param pstOwner
 */
void pifFnd_Start(PIF_stFnd *pstOwner)
{
	pstOwner->btRun = TRUE;
}

/**
 * @fn pifFnd_Stop
 * @brief
 * @param pstOwner
 */
void pifFnd_Stop(PIF_stFnd *pstOwner)
{
	int i;

	for (i = 0; i < pstOwner->ucDigitSize; i++) {
		(*pstOwner->__actDisplay)(0, 1 << i, 0);
	}
	pstOwner->btRun = FALSE;
    if (pstOwner->btBlink) {
		pifPulse_StopItem(pstOwner->__pstTimerBlink);
		pstOwner->btBlink = FALSE;
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
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstOwner->__pstTimerBlink) {
		pstOwner->__pstTimerBlink = pifPulse_AddItem(s_pstFndTimer, PT_enRepeat);
        if (!pstOwner->__pstTimerBlink) return FALSE;
        pifPulse_AttachEvtFinish(pstOwner->__pstTimerBlink, _TimerBlinkFinish, pstOwner);
    }
    if (!pifPulse_StartItem(pstOwner->__pstTimerBlink, usPeriodMs)) return FALSE;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Fnd:BlinkOn(P:%u) EC:%d", usPeriodMs, pif_enError);
    return FALSE;
}

/**
 * @fn pifFnd_BlinkOff
 * @brief
 * @param pstOwner
 */
void pifFnd_BlinkOff(PIF_stFnd *pstOwner)
{
	pstOwner->btBlink = FALSE;
	if (pstOwner->__pstTimerBlink) {
		pifPulse_RemoveItem(s_pstFndTimer, pstOwner->__pstTimerBlink);
		pstOwner->__pstTimerBlink = NULL;
	}
}

/**
 * @fn pifFnd_ChangeBlinkPeriod
 * @brief
 * @param pstOwner
 * @param usPeriodMs
 */
void pifFnd_ChangeBlinkPeriod(PIF_stFnd *pstOwner, uint16_t usPeriodMs)
{
	if (pstOwner->__pstTimerBlink) {
		pifPulse_ResetItem(pstOwner->__pstTimerBlink, usPeriodMs * 1000);
	}
}

/**
 * @fn pifFnd_SetColor
 * @brief
 * @param pstOwner
 * @param ucColor
 */
void pifFnd_SetColor(PIF_stFnd *pstOwner, uint8_t ucColor)
{
    pstOwner->btColor = ucColor;
}

/**
 * @fn pifFnd_SetFillZero
 * @brief
 * @param pstOwner
 * @param bFillZero
 */
void pifFnd_SetFillZero(PIF_stFnd *pstOwner, BOOL bFillZero)
{
    pstOwner->btFillZero = bFillZero;
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
    int sp = pstOwner->ucDigitSize;
    if (pstOwner->btSubNumericDigits) {
    	dValue -= nValue;
    	for (int p = sp - pstOwner->btSubNumericDigits; p < sp; p++) {
    		dValue *= 10;
    		uint32_t sd = (uint32_t)dValue;
    		pstOwner->__pcString[p] = '0' + sd;
    		dValue -= sd;
    	}
    	sp -= pstOwner->btSubNumericDigits;
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
	pstOwner->__pcString[pstOwner->ucDigitSize - 1 - pstOwner->btSubNumericDigits] |= 0x80;
	if (nValue || minus) {
		pstOwner->__pcString[pstOwner->ucDigitSize - 1] = '_';
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
    int sp = pstOwner->ucDigitSize - 1;
    if (pstOwner->btSubNumericDigits) {
    	for (int p = sp; p >= sp - pstOwner->btSubNumericDigits; p--) {
    		pstOwner->__pcString[p] = '0';
    	}
    	sp -= pstOwner->btSubNumericDigits;
    }
    if (pstOwner->btFillZero) {
        for (int p = sp; p >= minus; p--) {
			uint8_t digit = nValue % 10;
			pstOwner->__pcString[p] = '0' + digit;
        	nValue = nValue / 10;
        }
        if (minus) {
        	pstOwner->__pcString[0] = '-';
        }
        if (nValue) {
        	pstOwner->__pcString[pstOwner->ucDigitSize - 1] = '_';
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
        	pstOwner->__pcString[pstOwner->ucDigitSize - 1] = '_';
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

    for (int p = 0; p < pstOwner->ucDigitSize; p++) {
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

/**
 * @fn pifFnd_SetSubNumericDigits
 * @brief
 * @param pstOwner
 * @param ucSubNumericDigits
 */
void pifFnd_SetSubNumericDigits(PIF_stFnd *pstOwner, uint8_t ucSubNumericDigits)
{
	pstOwner->btSubNumericDigits = ucSubNumericDigits;
}

/**
 * @fn pifFnd_taskAll
 * @brief
 * @param pstTask
 */
void pifFnd_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucFndArrayPos; i++) {
		PIF_stFnd *pstOwner = &s_pstFndArray[i];
		if (!pstOwner->__enTaskLoop) _TaskCommon(pstOwner);
	}
}

/**
 * @fn pifFnd_taskEach
 * @brief
 * @param pstTask
 */
void pifFnd_taskEach(PIF_stTask *pstTask)
{
	PIF_stFnd *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}
