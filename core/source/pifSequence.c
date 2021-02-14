#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSequence.h"


static PIF_stSequence *s_pstSequence = NULL;
static uint8_t s_ucSequenceSize;
static uint8_t s_ucSequencePos;

static PIF_stPulse *s_pstSequenceTimer;


static void _evtTimerTimeoutFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSequence *pstOwner = (PIF_stSequence *)pvIssuer;

	pif_enError = E_enTimeout;
	if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
	pstOwner->_ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
}

static void _evtTimerDelayFinish(void *pvIssuer)
{
    if (!pvIssuer) {
        pif_enError = E_enInvalidParam;
        return;
    }

    PIF_stSequence *pstOwner = (PIF_stSequence *)pvIssuer;

	pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
	pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
	pstOwner->usDelay = 0;
}

static void _taskCommon(PIF_stSequence *pstOwner)
{
	const PIF_stSequencePhase *pstPhase;
	uint8_t ucPhaseNoNext;
	
	if (pstOwner->_ucPhaseNo == PIF_SEQUENCE_PHASE_NO_IDLE ||
			pstOwner->ucStep == PIF_SEQUENCE_STEP_DELAY) return;

	pstPhase = &pstOwner->__pstPhaseList[pstOwner->_ucPhaseNo];

	if (!pstPhase->fnProcess) {
		pif_enError = E_enWrongData;
		goto fail;
	}

	switch ((*pstPhase->fnProcess)(pstOwner)) {
	case SR_enContinue:
		break;

	case SR_enNext:
		if (pstOwner->__pstTimerTimeout) pifPulse_StopItem(pstOwner->__pstTimerTimeout);

		ucPhaseNoNext = pstOwner->ucPhaseNoNext;
		if (ucPhaseNoNext == PIF_SEQUENCE_PHASE_NO_IDLE) {
			ucPhaseNoNext = pstPhase->ucPhaseNoNext;
		}

		if (ucPhaseNoNext != PIF_SEQUENCE_PHASE_NO_IDLE) {
			if (pstOwner->usDelay) {
				if (!pstOwner->__pstTimerDelay) {
					pstOwner->__pstTimerDelay = pifPulse_AddItem(s_pstSequenceTimer, PT_enOnce);
					if (!pstOwner->__pstTimerDelay) goto fail;
					pifPulse_AttachEvtFinish(pstOwner->__pstTimerDelay, _evtTimerDelayFinish, pstOwner);
				}
				pifPulse_StartItem(pstOwner->__pstTimerDelay, pstOwner->usDelay);
				pstOwner->ucStep = PIF_SEQUENCE_STEP_DELAY;
			}
			else {
				pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
				pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
				pstOwner->usDelay = 0;
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enInfo, "Sequence:Next(%u->%u)", pstOwner->_ucPhaseNo, ucPhaseNoNext);
#endif		
		pstOwner->_ucPhaseNo = ucPhaseNoNext;
		break;

	case SR_enFinish:
		pstOwner->_ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
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
	pstOwner->_ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
}

/**
 * @fn pifSequence_Init
 * @brief 입력된 크기만큼 Sequence 구조체를 할당하고 초기화한다.
 * @param pstTimer
 * @param ucSize Sequence 크기
 * @return 성공 여부
 */
BOOL pifSequence_Init(PIF_stPulse *pstTimer, uint8_t ucSize)
{
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
			if (pstOwner->__pstTimerDelay) {
				pifPulse_RemoveItem(s_pstSequenceTimer, pstOwner->__pstTimerDelay);
			}
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
    pstOwner->__pstPhaseList = pstPhaseList;

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->_ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
    pstOwner->pvParam = pvParam;

    s_ucSequencePos = s_ucSequencePos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sequence:Add() EC:%d", pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifSequence_Start
 * @brief 
 * @param pstOwner
 */
void pifSequence_Start(PIF_stSequence *pstOwner)
{
	pstOwner->_ucPhaseNo = 0;
	pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
	pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
	pstOwner->usDelay = 0;
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
