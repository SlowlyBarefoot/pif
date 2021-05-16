#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSequence.h"


static PIF_stSequence *s_pstSequence = NULL;
static uint8_t s_ucSequenceSize;
static uint8_t s_ucSequencePos;

static PIF_stPulse *s_pstSequenceTimer;


static void _SetPhaseNo(PIF_stSequence *pstOwner, uint8_t ucPhaseNo)
{
	pstOwner->_ucPhaseNo = ucPhaseNo;
#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__ucCsFlag & SqCsF_enPhaseBit) {
		pifCollectSignal_AddSignal(pstOwner->__cCsIndex[SqCsF_enPhaseIdx], pstOwner->_ucPhaseNo);
	}
#endif
}

static void _evtTimerTimeoutFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSequence *pstOwner = (PIF_stSequence *)pvIssuer;

	pif_enError = E_enTimeout;
	if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
	_SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
}

static void _taskCommon(PIF_stSequence *pstOwner)
{
	const PIF_stSequencePhase *pstPhase;
	uint8_t ucPhaseNoNext;
	
	if (pstOwner->_ucPhaseNo == PIF_SEQUENCE_PHASE_NO_IDLE) return;

	if (pstOwner->usDelay1us) {
		if ((*pif_actTimer1us)() >= pstOwner->__unTargetDelay) {
			pstOwner->usDelay1us = 0;
		}
		else return;
	}

	pstPhase = &pstOwner->__pstPhaseList[pstOwner->_ucPhaseNo];

	if (!pstPhase->fnProcess) {
		pif_enError = E_enWrongData;
		goto fail;
	}

	switch ((*pstPhase->fnProcess)(pstOwner)) {
	case SR_enContinue:
		if (pstOwner->usDelay1us) {
			pstOwner->__unTargetDelay = (*pif_actTimer1us)() + pstOwner->usDelay1us;
		}
		break;

	case SR_enNext:
		if (pstOwner->__pstTimerTimeout) pifPulse_StopItem(pstOwner->__pstTimerTimeout);

		ucPhaseNoNext = pstOwner->ucPhaseNoNext;
		if (ucPhaseNoNext == PIF_SEQUENCE_PHASE_NO_IDLE) {
			ucPhaseNoNext = pstPhase->ucPhaseNoNext;
		}

		if (ucPhaseNoNext != PIF_SEQUENCE_PHASE_NO_IDLE) {
			if (pstOwner->usDelay1us) {
				pstOwner->__unTargetDelay = (*pif_actTimer1us)() + pstOwner->usDelay1us;
			}
			pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
			pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
		}
		_SetPhaseNo(pstOwner, ucPhaseNoNext);
		break;

	case SR_enFinish:
		_SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
		break;

	default:
		pif_enError = E_enWrongData;
		goto fail;
	}
	return;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sequence:Error(%d) EC:%d", pstOwner->_ucPhaseNo, pif_enError);
#endif
	if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
	_SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[SqCsF_enCount] = { "SQ" };

	for (int i = 0; i < s_ucSequencePos; i++) {
		PIF_stSequence *pstOwner = &s_pstSequence[i];
		if (pstOwner->__ucCsFlag) {
			for (int f = 0; f < SqCsF_enCount; f++) {
				pstOwner->__cCsIndex[f] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enWire, 8, prefix[f], 0xFF);
			}
		}
	}
}

#endif

/**
 * @fn pifSequence_Init
 * @brief 입력된 크기만큼 Sequence 구조체를 할당하고 초기화한다.
 * @param pstTimer
 * @param ucSize Sequence 크기
 * @return 성공 여부
 */
BOOL pifSequence_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
    if (!pif_actTimer1us) {
        pif_enError = E_enCanNotUse;
        goto fail;
    }

    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstSequence = calloc(sizeof(PIF_stSequence), ucSize);
    if (!s_pstSequence) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSequenceSize = ucSize;
    s_ucSequencePos = 0;

    s_pstSequenceTimer = pstTimer;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSequence, _AddDeviceInCollectSignal);
#endif
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sequence:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifSequence_Exit
 * @brief Sequence용 메모리를 반환한다.
 */
void pifSequence_Exit()
{
	PIF_stSequence *pstOwner;

	if (s_pstSequence) {
		for (int i = 0; i < s_ucSequencePos; i++) {
			pstOwner = &s_pstSequence[i];
			if (pstOwner->__pstTimerTimeout) {
				pifPulse_RemoveItem(s_pstSequenceTimer, pstOwner->__pstTimerTimeout);
			}
		}		
        free(s_pstSequence);
        s_pstSequence = NULL;
    }
}

/**
 * @fn pifSequence_Add
 * @brief Sequence를 추가한다.
 * @param usPifId
 * @param pstPhaseList
 * @param pvParam
 * @return Sequence 구조체 포인터를 반환한다.
 */
PIF_stSequence *pifSequence_Add(PIF_usId usPifId, const PIF_stSequencePhase *pstPhaseList, void *pvParam)
{
    if (s_ucSequencePos >= s_ucSequenceSize) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

    PIF_stSequence *pstOwner = &s_pstSequence[s_ucSequencePos];
    pstOwner->__ucIndex = s_ucSequencePos;
    pstOwner->__pstPhaseList = pstPhaseList;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    _SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
    pstOwner->pvParam = pvParam;

    s_ucSequencePos = s_ucSequencePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sequence:Add() EC:%d", pif_enError);
#endif
    return NULL;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSequence_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSequence_SetCsFlagAll(PIF_enSequenceCsFlag enFlag)
{
    for (int i = 0; i < s_ucSequencePos; i++) {
        s_pstSequence[i].__ucCsFlag |= enFlag;
    }
}

/**
 * @fn pifSequence_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSequence_ResetCsFlagAll(PIF_enSequenceCsFlag enFlag)
{
    for (int i = 0; i < s_ucSequencePos; i++) {
        s_pstSequence[i].__ucCsFlag &= ~enFlag;
    }
}

/**
 * @fn pifSequence_SetCsFlagEach
 * @brief
 * @param pstOwner
 * @param enFlag
 */
void pifSequence_SetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag)
{
	pstOwner->__ucCsFlag |= enFlag;
}

/**
 * @fn pifSequence_ResetCsFlagEach
 * @brief
 * @param pstOwner
 * @param enFlag
 */
void pifSequence_ResetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag)
{
	pstOwner->__ucCsFlag &= ~enFlag;
}

#endif

/**
 * @fn pifSequence_Start
 * @brief 
 * @param pstOwner
 */
void pifSequence_Start(PIF_stSequence *pstOwner)
{
	_SetPhaseNo(pstOwner, 0);
	pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
	pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
	pstOwner->usDelay1us = 0;
}	

BOOL pifSequence_SetTimeout(PIF_stSequence *pstOwner, uint16_t usTimeout)
{
	if (!pstOwner->__pstTimerTimeout) {
		pstOwner->__pstTimerTimeout = pifPulse_AddItem(s_pstSequenceTimer, PT_enOnce);
		if (!pstOwner->__pstTimerTimeout) return FALSE;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerTimeout, _evtTimerTimeoutFinish, pstOwner);
	}
	pifPulse_StartItem(pstOwner->__pstTimerTimeout, usTimeout);
	return TRUE;
}

/**
 * @fn pifSequence_taskAll
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifSequence_taskAll(PIF_stTask *pstTask)
{
	(void) pstTask;

    for (int i = 0; i < s_ucSequencePos; i++) {
        PIF_stSequence *pstOwner = &s_pstSequence[i];
    	if (!pstOwner->__enTaskLoop) _taskCommon(pstOwner);
    }
}

/**
 * @fn pifSequence_taskEach
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifSequence_taskEach(PIF_stTask *pstTask)
{
	PIF_stSequence *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_taskCommon(pstOwner);
	}
}
