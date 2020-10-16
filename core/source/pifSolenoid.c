#include "pifLog.h"
#include "pifSolenoid.h"


static PIF_stSolenoid *s_pstSolenoidArray = NULL;
static uint8_t s_ucSolenoidArraySize;
static uint8_t s_ucSolenoidArrayPos;

static PIF_stPulse *s_pstSolenoidTimer;


static void _ActionOn(PIF_stSolenoid *pstOwner, uint16_t usDelay, PIF_enSolenoidDir enDir)
{
	if (!usDelay) {
		if (pstOwner->enType != ST_en2Point || enDir != pstOwner->__enCurrentDir) {
			pstOwner->__enCurrentDir = enDir;
			(*pstOwner->__actControl)(ON, enDir);
			pstOwner->__bState = TRUE;
			if (pstOwner->usOnTime) {
				if (!pifPulse_StartItem(pstOwner->__pstTimerOn, pstOwner->usOnTime)) {
					pstOwner->__enCurrentDir = SD_enInvalid;
					(*pstOwner->__actControl)(OFF, SD_enInvalid);
					pstOwner->__bState = FALSE;
					if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
				}
			}
		}
	}
	else {
		pstOwner->__enDir = enDir;
		if (!pifPulse_StartItem(pstOwner->__pstTimerDelay, usDelay)) {
			if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
		}
	}
}

static void _TimerDelayFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoid *pstOwner = (PIF_stSolenoid *)pvIssuer;

	if (pstOwner->enType != ST_en2Point || pstOwner->__enDir != pstOwner->__enCurrentDir) {
		pstOwner->__enCurrentDir = pstOwner->__enDir;
		(*pstOwner->__actControl)(ON, pstOwner->__enDir);
		pstOwner->__bState = TRUE;
		if (pstOwner->usOnTime) {
			if (!pifPulse_StartItem(pstOwner->__pstTimerOn, pstOwner->usOnTime)) {
				pstOwner->__enCurrentDir = SD_enInvalid;
				(*pstOwner->__actControl)(OFF, SD_enInvalid);
				pstOwner->__bState = FALSE;
				if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
			}
		}
	}

	if (pstOwner->__pstBuffer) {
		PIF_stSolenoidContent *pstContent = pifRingData_Remove(pstOwner->__pstBuffer);
		if (pstContent) {
			_ActionOn(pstOwner, pstContent->usDelay, pstContent->enDir);
		}
	}
}

static void _TimerOnFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoid *pstOwner = (PIF_stSolenoid *)pvIssuer;

    if (pstOwner->__bState) {
        (*pstOwner->__actControl)(OFF, SD_enInvalid);
        if (pstOwner->evtOff) (*pstOwner->evtOff)(pstOwner);
        pstOwner->__bState = FALSE;
    }
}

static int32_t _CalcurateTime(PIF_stSolenoid *pstOwner)
{
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	time = pifPulse_RemainItem(pstOwner->__pstTimerDelay);
	pstContent = pifRingData_GetFirstData(pstOwner->__pstBuffer);
	while (pstContent) {
		time += pstContent->usDelay;
		pstContent = pifRingData_GetNextData(pstOwner->__pstBuffer);
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

    s_pstSolenoidArray = calloc(sizeof(PIF_stSolenoid), ucSize);
    if (!s_pstSolenoidArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSolenoidArraySize = ucSize;
    s_ucSolenoidArrayPos = 0;

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
    if (s_pstSolenoidArray) {
    	free(s_pstSolenoidArray);
        s_pstSolenoidArray = NULL;
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
    if (s_ucSolenoidArrayPos >= s_ucSolenoidArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!actControl) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stSolenoid *pstOwner = &s_pstSolenoidArray[s_ucSolenoidArrayPos];

    pstOwner->__pstTimerOn = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstOwner->__pstTimerOn) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerOn, _TimerOnFinish, pstOwner);

    pstOwner->__pstTimerDelay = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _TimerDelayFinish, pstOwner);

    pstOwner->unDeviceCode = unDeviceCode;
    pstOwner->enType = enType;
    pstOwner->usOnTime = usOnTime;
    pstOwner->__actControl = actControl;
    pstOwner->__bState = FALSE;

    s_ucSolenoidArrayPos = s_ucSolenoidArrayPos + 1;
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
	pstOwner->__pstBuffer = pifRingData_Init(sizeof(PIF_stSolenoidContent), usSize);
	if (!pstOwner->__pstBuffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief
 * @param pstOwner
 */
void pifSolenoid_SetInvalidDirection(PIF_stSolenoid *pstOwner)
{
	pstOwner->__enCurrentDir = SD_enInvalid;
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
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	if (pstOwner->__pstBuffer) {
		if (pifPulse_GetStep(pstOwner->__pstTimerDelay) == PS_enRunning) {
			time = _CalcurateTime(pstOwner);
			pstContent = pifRingData_Add(pstOwner->__pstBuffer);
			pstContent->usDelay = usDelay > time ? usDelay - time : 0;
			return;
		}
	}

	_ActionOn(pstOwner, usDelay, SD_enInvalid);
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
	PIF_stSolenoidContent *pstContent;
	int32_t time;

	if (pstOwner->__pstBuffer) {
		if (pifPulse_GetStep(pstOwner->__pstTimerDelay) == PS_enRunning) {
			time = _CalcurateTime(pstOwner);
			pstContent = pifRingData_Add(pstOwner->__pstBuffer);
			pstContent->usDelay = usDelay >= time ? usDelay - time : 0;
			pstContent->enDir = enDir;
			return;
		}
	}

    _ActionOn(pstOwner, usDelay, enDir);
}

/**
 * @fn pifSolenoid_ActionOff
 * @brief
 * @param pstOwner
 */
void pifSolenoid_ActionOff(PIF_stSolenoid *pstOwner)
{
    if (pstOwner->__bState) {
		pifPulse_StopItem(pstOwner->__pstTimerOn);
		(*pstOwner->__actControl)(OFF, SD_enInvalid);
		pstOwner->__bState = FALSE;
    }
}
