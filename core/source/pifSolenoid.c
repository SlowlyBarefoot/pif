#include "pifLog.h"
#include "pifSolenoid.h"


typedef struct _PIF_stSolenoidBase
{
	// Public Member Variable
	PIF_stSolenoid stOwner;

	// Private Member Variable
    BOOL bState;
    PIF_enSolenoidDir enCurrentDir;
    PIF_stPulseItem *pstTimerOn;
	PIF_stPulseItem *pstTimerDelay;
    PIF_enSolenoidDir enDir;
	PIF_stRingData *pstBuffer;

    // Private Action Function
    PIF_actSolenoidControl actControl;
} PIF_stSolenoidBase;


static PIF_stSolenoidBase *s_pstSolenoidBase = NULL;
static uint8_t s_ucSolenoidBaseSize;
static uint8_t s_ucSolenoidBasePos;

static PIF_stPulse *s_pstSolenoidTimer;


static void _ActionOn(PIF_stSolenoidBase *pstBase, uint16_t usDelay, PIF_enSolenoidDir enDir)
{
	if (!usDelay) {
		if (pstBase->stOwner.enType != ST_en2Point || enDir != pstBase->enCurrentDir) {
			pstBase->enCurrentDir = enDir;
			(*pstBase->actControl)(ON, enDir);
			pstBase->bState = TRUE;
			if (pstBase->stOwner.usOnTime) {
				if (!pifPulse_StartItem(pstBase->pstTimerOn, pstBase->stOwner.usOnTime)) {
					pstBase->enCurrentDir = SD_enInvalid;
					(*pstBase->actControl)(OFF, SD_enInvalid);
					pstBase->bState = FALSE;
					if (pstBase->stOwner.evtError) (*pstBase->stOwner.evtError)(&pstBase->stOwner);
				}
			}
		}
	}
	else {
		pstBase->enDir = enDir;
		if (!pifPulse_StartItem(pstBase->pstTimerDelay, usDelay)) {
			if (pstBase->stOwner.evtError) (*pstBase->stOwner.evtError)(&pstBase->stOwner);
		}
	}
}

static void _TimerDelayFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pvIssuer;

	if (pstBase->stOwner.enType != ST_en2Point || pstBase->enDir != pstBase->enCurrentDir) {
		pstBase->enCurrentDir = pstBase->enDir;
		(*pstBase->actControl)(ON, pstBase->enDir);
		pstBase->bState = TRUE;
		if (pstBase->stOwner.usOnTime) {
			if (!pifPulse_StartItem(pstBase->pstTimerOn, pstBase->stOwner.usOnTime)) {
				pstBase->enCurrentDir = SD_enInvalid;
				(*pstBase->actControl)(OFF, SD_enInvalid);
				pstBase->bState = FALSE;
				if (pstBase->stOwner.evtError) (*pstBase->stOwner.evtError)(&pstBase->stOwner);
			}
		}
	}

	if (pstBase->pstBuffer) {
		PIF_stSolenoidContent *pstContent = pifRingData_Remove(pstBase->pstBuffer);
		if (pstContent) {
			_ActionOn(pstBase, pstContent->usDelay, pstContent->enDir);
		}
	}
}

static void _TimerOnFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pvIssuer;

    if (pstBase->bState) {
        (*pstBase->actControl)(OFF, SD_enInvalid);
        if (pstBase->stOwner.evtOff) (*pstBase->stOwner.evtOff)(&pstBase->stOwner);
        pstBase->bState = FALSE;
    }
}

static int32_t _CalcurateTime(PIF_stSolenoidBase *pstBase)
{
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	time = pifPulse_RemainItem(pstBase->pstTimerDelay);
	pstContent = pifRingData_GetFirstData(pstBase->pstBuffer);
	while (pstContent) {
		time += pstContent->usDelay;
		pstContent = pifRingData_GetNextData(pstBase->pstBuffer);
	}
	return time;
}

/**
 * @fn pifSolenoid_Init
 * @brief
 * @param pstTimer
 * @param ucSize
 * @return
 */
BOOL pifSolenoid_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pstTimer || ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSolenoidBase = calloc(sizeof(PIF_stSolenoid), ucSize);
    if (!s_pstSolenoidBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSolenoidBaseSize = ucSize;
    s_ucSolenoidBasePos = 0;

    s_pstSolenoidTimer = pstTimer;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Solenoid:Init(S:%u) EC:%d", ucSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifSolenoid_Exit
 * @brief
 */
void pifSolenoid_Exit()
{
    if (s_pstSolenoidBase) {
    	free(s_pstSolenoidBase);
        s_pstSolenoidBase = NULL;
    }
}

/**
 * @fn pifSolenoid_Add
 * @brief 
 * @param unDeviceCode
 * @param enType
 * @param usOnTime
 * @param actControl
 * @return 
 */
PIF_stSolenoid *pifSolenoid_Add(PIF_unDeviceCode unDeviceCode, PIF_enSolenoidType enType, uint16_t usOnTime,
		PIF_actSolenoidControl actControl)
{
    if (s_ucSolenoidBasePos >= s_ucSolenoidBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!actControl) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stSolenoidBase *pstBase = &s_pstSolenoidBase[s_ucSolenoidBasePos];

    pstBase->pstTimerOn = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstBase->pstTimerOn) return FALSE;
    pifPulse_AttachEvtFinish(pstBase->pstTimerOn, _TimerOnFinish, pstBase);

    pstBase->pstTimerDelay = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstBase->pstTimerDelay) return FALSE;
    pifPulse_AttachEvtFinish(pstBase->pstTimerDelay, _TimerDelayFinish, pstBase);

    pstBase->actControl = actControl;
    pstBase->bState = FALSE;

    PIF_stSolenoid *pstOwner = &pstBase->stOwner;

    pstOwner->unDeviceCode = unDeviceCode;
    pstOwner->enType = enType;
    pstOwner->usOnTime = usOnTime;

    s_ucSolenoidBasePos = s_ucSolenoidBasePos + 1;
    return pstOwner;

fail:
	pifLog_Printf(LT_enError, "Solenoid:Add(D:%u O:%u) EC:%d", unDeviceCode, usOnTime, pif_enError);
    return NULL;
}

/**
 * @fn pifSolenoid_SetBuffer
 * @brief
 * @param pstOwner
 * @param usSize
 * @return
 */
BOOL pifSolenoid_SetBuffer(PIF_stSolenoid *pstOwner, uint16_t usSize)
{
	PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pstOwner;

	pstBase->pstBuffer = pifRingData_Init(sizeof(PIF_stSolenoidContent), usSize);
	if (!pstBase->pstBuffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief
 * @param pstOwner
 */
void pifSolenoid_SetInvalidDirection(PIF_stSolenoid *pstOwner)
{
	((PIF_stSolenoidBase *)pstOwner)->enCurrentDir = SD_enInvalid;
}

/**
 * @fn pifSolenoid_SetOnTime
 * @brief
 * @param pstOwner
 * @param usOnTime
 * @return
 */
BOOL pifSolenoid_SetOnTime(PIF_stSolenoid *pstOwner, uint16_t usOnTime)
{
    if (!usOnTime) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    pstOwner->usOnTime = usOnTime;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Solenoid:SetOnTime(O:%u) EC:%d", usOnTime, pif_enError);
    return FALSE;
}

/**
 * @fn pifSolenoid_ActionOn
 * @brief
 * @param pstOwner
 * @param usDelay
 */
void pifSolenoid_ActionOn(PIF_stSolenoid *pstOwner, uint16_t usDelay)
{
	PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pstOwner;
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	if (pstBase->pstBuffer) {
		if (pifPulse_GetStep(pstBase->pstTimerDelay) == PS_enRunning) {
			time = _CalcurateTime(pstBase);
			pstContent = pifRingData_Add(pstBase->pstBuffer);
			pstContent->usDelay = usDelay > time ? usDelay - time : 0;
			return;
		}
	}

	_ActionOn(pstBase, usDelay, SD_enInvalid);
}

/**
 * @fn pifSolenoid_ActionOnDir
 * @brief 
 * @param pstOwner
 * @param usDelay
 * @param enDir
 */
void pifSolenoid_ActionOnDir(PIF_stSolenoid *pstOwner, uint16_t usDelay, PIF_enSolenoidDir enDir)
{
	PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pstOwner;
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	if (pstBase->pstBuffer) {
		if (pifPulse_GetStep(pstBase->pstTimerDelay) == PS_enRunning) {
			time = _CalcurateTime(pstBase);
			pstContent = pifRingData_Add(pstBase->pstBuffer);
			pstContent->usDelay = usDelay >= time ? usDelay - time : 0;
			pstContent->enDir = enDir;
			return;
		}
	}

    _ActionOn(pstBase, usDelay, enDir);
}

/**
 * @fn pifSolenoid_ActionOff
 * @brief
 * @param pstOwner
 */
void pifSolenoid_ActionOff(PIF_stSolenoid *pstOwner)
{
	PIF_stSolenoidBase *pstBase = (PIF_stSolenoidBase *)pstOwner;

	if (pstBase->bState) {
		pifPulse_StopItem(pstBase->pstTimerOn);
		(*pstBase->actControl)(OFF, SD_enInvalid);
		pstBase->bState = FALSE;
    }
}
