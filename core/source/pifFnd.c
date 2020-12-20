#include "pifFnd.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#define PIF_FND_CONTROL_PERIOD_DEFAULT		25


typedef struct _PIF_stFndBase
{
	// Public Member Variable
	PIF_stFnd stOwner;

	// Private Member Variable
	struct {
		uint8_t btRun			: 1;
		uint8_t btBlink			: 1;
		uint8_t btFillZero		: 1;
	};
    uint16_t usControlPeriodMs;
	uint16_t usPretimeMs;
	uint8_t ucDigitIndex;
	uint8_t ucStringSize;
    char *pcString;
	PIF_stPulseItem *pstTimerBlink;

	PIF_enTaskLoop enTaskLoop;

	// Private Member Function
   	PIF_actFndDisplay actDisplay;
} PIF_stFndBase;


static PIF_stFndBase *s_pstFndBase = NULL;
static uint8_t s_ucFndBaseSize;
static uint8_t s_ucFndBasePos;

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


static void _TimerDisplayFinish(PIF_stFndBase *pstBase)
{
	uint8_t ch, seg;

	if (!pstBase->btBlink) {
		ch = pstBase->pcString[pstBase->ucDigitIndex];
		if (ch & 0x80) {
			seg = c_aucFndAscii[ch - 0xA0] | 0x80;
		}
		else {
			seg = c_aucFndAscii[ch - 0x20];
		}
		(*pstBase->actDisplay)(seg, pstBase->ucDigitIndex);
	}
	else {
		(*pstBase->actDisplay)(0, pstBase->ucDigitIndex);
	}
	pstBase->ucDigitIndex++;
	if (pstBase->ucDigitIndex >= pstBase->stOwner.ucDigitSize) pstBase->ucDigitIndex = 0;
}

static void _evtTimerBlinkFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stFndBase *pstBase = (PIF_stFndBase *)pvIssuer;
    pstBase->btBlink ^= 1;
}

static void _TaskCommon(PIF_stFndBase *pstBase)
{
	if (!pstBase->btRun) return;

	if (pif_usTimer1ms > pstBase->usPretimeMs) {
		if (pif_usTimer1ms - pstBase->usPretimeMs < pstBase->usControlPeriodMs) return;
	}
	else if (pif_usTimer1ms < pstBase->usPretimeMs) {
		if (1000 + pif_usTimer1ms - pstBase->usPretimeMs < pstBase->usControlPeriodMs) return;
	}
	else return;

	_TimerDisplayFinish(pstBase);

	pstBase->usPretimeMs = pif_usTimer1ms;
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

    s_pstFndBase = calloc(sizeof(PIF_stFndBase), ucSize);
    if (!s_pstFndBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucFndBaseSize = ucSize;
    s_ucFndBasePos = 0;

    s_pstFndTimer = pstTimer1ms;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Fnd:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifFnd_Exit
 * @brief
 */
void pifFnd_Exit()
{
	PIF_stFndBase *pstBase;

    if (s_pstFndBase) {
    	for (int i = 0; i < s_ucFndBasePos; i++) {
    		pstBase = &s_pstFndBase[i];
    		if (pstBase->pcString) {
    			free(pstBase->pcString);
    			pstBase->pcString = NULL;
    		}
    	}
    	free(s_pstFndBase);
        s_pstFndBase = NULL;
    }
}

/**
 * @fn pifFnd_Add
 * @brief
 * @param unDeviceCode
 * @param ucDigitSize
 * @param actDisplay
 * @return
 */
PIF_stFnd *pifFnd_Add(PIF_unDeviceCode unDeviceCode, uint8_t ucDigitSize, PIF_actFndDisplay actDisplay)
{
    if (s_ucFndBasePos >= s_ucFndBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!ucDigitSize || !actDisplay) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stFndBase *pstBase = &s_pstFndBase[s_ucFndBasePos];

    pstBase->pcString = calloc(sizeof(uint8_t), ucDigitSize);
    if (!pstBase->pcString) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}
    for (int i = 0; i < ucDigitSize; i++) pstBase->pcString[i] = 0x20;

    pstBase->stOwner.unDeviceCode = unDeviceCode;
    pstBase->usControlPeriodMs = PIF_FND_CONTROL_PERIOD_DEFAULT / ucDigitSize;
    pstBase->stOwner.ucDigitSize = ucDigitSize;
    pstBase->actDisplay = actDisplay;
    pstBase->pstTimerBlink = NULL;

    s_ucFndBasePos = s_ucFndBasePos + 1;
    return &pstBase->stOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Fnd:Add(D:%u DS:%u) EC:%d", unDeviceCode, ucDigitSize, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifFnd_GetControlPeriod
 * @brief
 * @param pstOwner
 * @return
 */
uint16_t pifFnd_GetControlPeriod(PIF_stFnd *pstOwner)
{
    return ((PIF_stFndBase *)pstOwner)->usControlPeriodMs;
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
    if (!usPeriodMs || usPeriodMs < pstOwner->ucDigitSize) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    ((PIF_stFndBase *)pstOwner)->usControlPeriodMs = usPeriodMs / pstOwner->ucDigitSize;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Fnd:SetControlPeriod(P:%u) EC:%d", usPeriodMs, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifFnd_Start
 * @brief
 * @param pstOwner
 */
void pifFnd_Start(PIF_stFnd *pstOwner)
{
	((PIF_stFndBase *)pstOwner)->btRun = TRUE;
}

/**
 * @fn pifFnd_Stop
 * @brief
 * @param pstOwner
 */
void pifFnd_Stop(PIF_stFnd *pstOwner)
{
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;
	int i;

	for (i = 0; i < pstOwner->ucDigitSize; i++) {
		(*pstBase->actDisplay)(0, 1 << i);
	}
	pstBase->btRun = FALSE;
    if (pstBase->btBlink) {
		pifPulse_StopItem(pstBase->pstTimerBlink);
		pstBase->btBlink = FALSE;
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
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;

	if (!usPeriodMs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

	if (!pstBase->pstTimerBlink) {
		pstBase->pstTimerBlink = pifPulse_AddItem(s_pstFndTimer, PT_enRepeat);
        if (!pstBase->pstTimerBlink) return FALSE;
        pifPulse_AttachEvtFinish(pstBase->pstTimerBlink, _evtTimerBlinkFinish, pstBase);
    }
    if (!pifPulse_StartItem(pstBase->pstTimerBlink, usPeriodMs)) return FALSE;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Fnd:BlinkOn(P:%u) EC:%d", usPeriodMs, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifFnd_BlinkOff
 * @brief
 * @param pstOwner
 */
void pifFnd_BlinkOff(PIF_stFnd *pstOwner)
{
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;

	pstBase->btBlink = FALSE;
	if (pstBase->pstTimerBlink) {
		pifPulse_RemoveItem(s_pstFndTimer, pstBase->pstTimerBlink);
		pstBase->pstTimerBlink = NULL;
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
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;

	if (pstBase->pstTimerBlink) {
		pifPulse_ResetItem(pstBase->pstTimerBlink, usPeriodMs * 1000);
	}
}

/**
 * @fn pifFnd_SetFillZero
 * @brief
 * @param pstOwner
 * @param bFillZero
 */
void pifFnd_SetFillZero(PIF_stFnd *pstOwner, BOOL bFillZero)
{
    ((PIF_stFndBase *)pstOwner)->btFillZero = bFillZero;
}

/**
 * @fn pifFnd_SetFloat
 * @brief
 * @param pstOwner
 * @param dValue
 */
void pifFnd_SetFloat(PIF_stFnd *pstOwner, double dValue)
{
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;
    BOOL minus = FALSE;

    if (dValue < 0.0) {
    	minus = TRUE;
    	dValue *= -1.0;
    }
    uint32_t nValue = (uint32_t)dValue;
    int sp = pstOwner->ucDigitSize;
    if (pstOwner->ucSubNumericDigits) {
    	dValue -= nValue;
    	for (int p = sp - pstOwner->ucSubNumericDigits; p < sp; p++) {
    		dValue *= 10;
    		uint32_t sd = (uint32_t)dValue;
    		pstBase->pcString[p] = '0' + sd;
    		dValue -= sd;
    	}
    	sp -= pstOwner->ucSubNumericDigits;
    }
    sp--;
	BOOL first = TRUE;
	for (int p = sp; p >= 0; p--) {
		if (!first && !nValue) {
			if (minus) {
				pstBase->pcString[p] = '-';
				minus = FALSE;
			}
			else {
				pstBase->pcString[p] = 0x20;
			}
		}
		else {
			uint8_t digit = nValue % 10;
			pstBase->pcString[p] = '0' + digit;
			if (digit) first = FALSE;
		}
		nValue = nValue / 10;
	}
	pstBase->pcString[pstOwner->ucDigitSize - 1 - pstOwner->ucSubNumericDigits] |= 0x80;
	if (nValue || minus) {
		pstBase->pcString[pstOwner->ucDigitSize - 1] = '_';
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
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;
    BOOL minus = FALSE;

    if (nValue < 0) {
    	minus = TRUE;
    	nValue *= -1;
    }
    int sp = pstOwner->ucDigitSize - 1;
    if (pstOwner->ucSubNumericDigits) {
    	for (int p = sp; p >= sp - pstOwner->ucSubNumericDigits; p--) {
    		pstBase->pcString[p] = '0';
    	}
    	sp -= pstOwner->ucSubNumericDigits;
    }
    if (pstBase->btFillZero) {
        for (int p = sp; p >= minus; p--) {
			uint8_t digit = nValue % 10;
			pstBase->pcString[p] = '0' + digit;
        	nValue = nValue / 10;
        }
        if (minus) {
        	pstBase->pcString[0] = '-';
        }
        if (nValue) {
        	pstBase->pcString[pstOwner->ucDigitSize - 1] = '_';
        }
    }
    else {
        BOOL first = TRUE;
        for (int p = sp; p >= 0; p--) {
        	if (!first && !nValue) {
        		if (minus) {
        			pstBase->pcString[p] = '-';
            		minus = FALSE;
        		}
        		else {
        			pstBase->pcString[p] = 0x20;
        		}
        	}
        	else {
            	uint8_t digit = nValue % 10;
            	pstBase->pcString[p] = '0' + digit;
            	if (digit) first = FALSE;
        	}
        	nValue = nValue / 10;
        }
        if (nValue || minus) {
        	pstBase->pcString[pstOwner->ucDigitSize - 1] = '_';
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
	PIF_stFndBase *pstBase = (PIF_stFndBase *)pstOwner;
    int src = 0;
    BOOL last = FALSE;

    for (int p = 0; p < pstOwner->ucDigitSize; p++) {
    	if (!last && pcString[src]) {
    		pstBase->pcString[p] = pcString[src];
    	}
    	else {
    		pstBase->pcString[p] = 0x20;
    		last = TRUE;
    	}
    	src++;
    }
}

/**
 * @fn pifFnd_taskAll
 * @brief
 * @param pstTask
 */
void pifFnd_taskAll(PIF_stTask *pstTask)
{
	(void)pstTask;

	for (int i = 0; i < s_ucFndBasePos; i++) {
		PIF_stFndBase *pstBase = &s_pstFndBase[i];
		if (!pstBase->enTaskLoop) _TaskCommon(pstBase);
	}
}

/**
 * @fn pifFnd_taskEach
 * @brief
 * @param pstTask
 */
void pifFnd_taskEach(PIF_stTask *pstTask)
{
	PIF_stFndBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstBase);
	}
}
