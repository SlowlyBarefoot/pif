#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSolenoid.h"


static PIF_stSolenoid *s_pstSolenoid = NULL;
static uint8_t s_ucSolenoidSize;
static uint8_t s_ucSolenoidPos;

static PIF_stPulse *s_pstSolenoidTimer;

static void _Action(PIF_stSolenoid *pstOwner, BOOL bState, PIF_enSolenoidDir enDir)
{
	(*pstOwner->__actControl)(bState, enDir);
#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__ucCsFlag & SnCsF_enActionBit) {
		pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SnCsF_enActionIdx], bState);
	}
	if (pstOwner->__ucCsFlag & SnCsF_enDirBit) {
		pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SnCsF_enDirIdx], enDir);
	}
#endif
}

static void _ActionOn(PIF_stSolenoid *pstOwner, uint16_t usDelay, PIF_enSolenoidDir enDir)
{
	if (!usDelay) {
		if (pstOwner->_enType != ST_en2Point || enDir != pstOwner->__enCurrentDir) {
			pstOwner->__enCurrentDir = enDir;
			_Action(pstOwner, ON, enDir);
			pstOwner->__bState = TRUE;
			if (pstOwner->usOnTime) {
				if (!pifPulse_StartItem(pstOwner->__pstTimerOn, pstOwner->usOnTime)) {
					pstOwner->__enCurrentDir = SD_enInvalid;
					_Action(pstOwner, OFF, SD_enInvalid);
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

static void _evtTimerDelayFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoid *pstOwner = (PIF_stSolenoid *)pvIssuer;

	if (pstOwner->_enType != ST_en2Point || pstOwner->__enDir != pstOwner->__enCurrentDir) {
		pstOwner->__enCurrentDir = pstOwner->__enDir;
		_Action(pstOwner, ON, pstOwner->__enDir);
		pstOwner->__bState = TRUE;
		if (pstOwner->usOnTime) {
			if (!pifPulse_StartItem(pstOwner->__pstTimerOn, pstOwner->usOnTime)) {
				pstOwner->__enCurrentDir = SD_enInvalid;
				_Action(pstOwner, OFF, SD_enInvalid);
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

static void _evtTimerOnFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSolenoid *pstOwner = (PIF_stSolenoid *)pvIssuer;

    if (pstOwner->__bState) {
        _Action(pstOwner, OFF, SD_enInvalid);
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

#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[SnCsF_enCount] = { "SNA", "SND" };

	for (int i = 0; i < s_ucSolenoidPos; i++) {
		PIF_stSolenoid *pstOwner = &s_pstSolenoid[i];
		if (pstOwner->__ucCsFlag) {
			pstOwner->__cCsIndex[0] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enWire, 1, prefix[0], 0);
			pstOwner->__cCsIndex[1] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enWire, 2, prefix[1], 0);
		}
	}
}

#endif

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

    s_pstSolenoid = calloc(sizeof(PIF_stSolenoid), ucSize);
    if (!s_pstSolenoid) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSolenoidSize = ucSize;
    s_ucSolenoidPos = 0;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSolenoid, _AddDeviceInCollectSignal);
#endif

    s_pstSolenoidTimer = pstTimer;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Solenoid:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSolenoid_Exit
 * @brief
 */
void pifSolenoid_Exit()
{
    if (s_pstSolenoid) {
        for (int i = 0; i < s_ucSolenoidPos; i++) {
        	PIF_stSolenoid *pstOwner = (PIF_stSolenoid *)&s_pstSolenoid[i];
        	if (pstOwner->__pstTimerOn) {
        		pifPulse_RemoveItem(s_pstSolenoidTimer, pstOwner->__pstTimerOn);
        	}
        	if (pstOwner->__pstTimerDelay) {
        		pifPulse_RemoveItem(s_pstSolenoidTimer, pstOwner->__pstTimerDelay);
        	}
        }
    	free(s_pstSolenoid);
        s_pstSolenoid = NULL;
    }
}

/**
 * @fn pifSolenoid_Add
 * @brief 
 * @param usPifId
 * @param enType
 * @param usOnTime
 * @param actControl
 * @return 
 */
PIF_stSolenoid *pifSolenoid_Add(PIF_usId usPifId, PIF_enSolenoidType enType, uint16_t usOnTime,
		PIF_actSolenoidControl actControl)
{
    if (s_ucSolenoidPos >= s_ucSolenoidSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!actControl) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    PIF_stSolenoid *pstOwner = &s_pstSolenoid[s_ucSolenoidPos];

    pstOwner->__pstTimerOn = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstOwner->__pstTimerOn) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerOn, _evtTimerOnFinish, pstOwner);

    pstOwner->__pstTimerDelay = pifPulse_AddItem(s_pstSolenoidTimer, PT_enOnce);
    if (!pstOwner->__pstTimerDelay) return FALSE;
    pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);

    pstOwner->__ucIndex = s_ucSolenoidPos;
    pstOwner->__actControl = actControl;
    pstOwner->__bState = FALSE;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->_enType = enType;
    pstOwner->usOnTime = usOnTime;

    s_ucSolenoidPos = s_ucSolenoidPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Solenoid:Add(D:%u O:%u) EC:%d", usPifId, usOnTime, pif_enError);
#endif
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
	pstOwner->__pstBuffer = pifRingData_Init(PIF_ID_AUTO, sizeof(PIF_stSolenoidContent), usSize);
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
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Solenoid:SetOnTime(O:%u) EC:%d", usOnTime, pif_enError);
#endif
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
		if (pstOwner->__pstTimerDelay->_enStep == PS_enRunning) {
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
		if (pstOwner->__pstTimerDelay->_enStep == PS_enRunning) {
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
		_Action(pstOwner, OFF, SD_enInvalid);
		pstOwner->__bState = FALSE;
    }
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSolenoid_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSolenoid_SetCsFlagAll(PIF_enSolenoidCsFlag enFlag)
{
    for (int i = 0; i < s_ucSolenoidPos; i++) {
    	s_pstSolenoid[i].__ucCsFlag |= enFlag;
    }
}

/**
 * @fn pifSolenoid_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSolenoid_ResetCsFlagAll(PIF_enSolenoidCsFlag enFlag)
{
    for (int i = 0; i < s_ucSolenoidPos; i++) {
    	s_pstSolenoid[i].__ucCsFlag &= ~enFlag;
    }
}

/**
 * @fn pifSolenoid_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSolenoid_SetCsFlagEach(PIF_stSolenoid *pstOwner, PIF_enSolenoidCsFlag enFlag)
{
	((PIF_stSolenoid *)pstOwner)->__ucCsFlag |= enFlag;
}

/**
 * @fn pifSolenoid_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param enFlag
 */
void pifSolenoid_ResetCsFlagEach(PIF_stSolenoid *pstOwner, PIF_enSolenoidCsFlag enFlag)
{
	((PIF_stSolenoid *)pstOwner)->__ucCsFlag &= ~enFlag;
}

#endif
