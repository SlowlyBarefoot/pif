#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSequence.h"


typedef struct _PIF_stSequenceBase
{
	// Public Member Variable
	PIF_stSequence stOwner;

	// Private Member Variable
	const PIF_stSequencePhase *pstPhaseList;
	PIF_stPulseItem *pstTimerTimeout;
	PIF_stPulseItem *pstTimerDelay;

	PIF_enTaskLoop enTaskLoop;
} PIF_stSequenceBase;


static PIF_stSequenceBase *s_pstSequenceBase = NULL;
static uint8_t s_ucSequenceBaseSize;
static uint8_t s_ucSequenceBasePos;

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
	pstOwner->ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
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

static void _LoopCommon(PIF_stSequenceBase *pstBase)
{
	PIF_stSequence *pstOwner = &pstBase->stOwner;
	const PIF_stSequencePhase *pstPhase;
	uint8_t ucPhaseNoNext;
	
	if (pstOwner->ucPhaseNo == PIF_SEQUENCE_PHASE_NO_IDLE ||
			pstOwner->ucStep == PIF_SEQUENCE_STEP_DELAY) return;

	pstPhase = &pstBase->pstPhaseList[pstOwner->ucPhaseNo];

	if (!pstPhase->fnProcess) {
		pif_enError = E_enWrongData;
		goto fail;
	}

	switch ((*pstPhase->fnProcess)(pstOwner)) {
	case SR_enContinue:
		break;

	case SR_enNext:
		if (pstBase->pstTimerTimeout) pifPulse_StopItem(pstBase->pstTimerTimeout);

		ucPhaseNoNext = pstOwner->ucPhaseNoNext;
		if (ucPhaseNoNext == PIF_SEQUENCE_PHASE_NO_IDLE) {
			ucPhaseNoNext = pstPhase->ucPhaseNoNext;
		}

		if (ucPhaseNoNext != PIF_SEQUENCE_PHASE_NO_IDLE) {
			if (pstOwner->usDelay) {
				if (!pstBase->pstTimerDelay) {
					pstBase->pstTimerDelay = pifPulse_AddItem(s_pstSequenceTimer, PT_enOnce);
					if (!pstBase->pstTimerDelay) goto fail;
					pifPulse_AttachEvtFinish(pstBase->pstTimerDelay, _evtTimerDelayFinish, pstOwner);
				}
				pifPulse_StartItem(pstBase->pstTimerDelay, pstOwner->usDelay);
				pstOwner->ucStep = PIF_SEQUENCE_STEP_DELAY;
			}
			else {
				pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
				pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
				pstOwner->usDelay = 0;
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enInfo, "Sequence:Next(%u->%u)", pstOwner->ucPhaseNo, ucPhaseNoNext);
#endif		
		pstOwner->ucPhaseNo = ucPhaseNoNext;
		break;

	case SR_enFinish:
		pstOwner->ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
		break;

	default:
		pif_enError = E_enWrongData;
		goto fail;
	}
	return;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Sequence:Error(%d) EC:%d", pstOwner->ucPhaseNo, pif_enError);
#endif
	if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
	pstOwner->ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
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

    s_pstSequenceBase = calloc(sizeof(PIF_stSequenceBase), ucSize);
    if (!s_pstSequenceBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucSequenceBaseSize = ucSize;
    s_ucSequenceBasePos = 0;

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
	PIF_stSequenceBase *pstOwner;

	if (s_pstSequenceBase) {
		for (int i = 0; i < s_ucSequenceBasePos; i++) {
			pstOwner = &s_pstSequenceBase[i];
			if (pstOwner->pstTimerDelay) {
				pifPulse_RemoveItem(s_pstSequenceTimer, pstOwner->pstTimerDelay);
			}
		}		
        free(s_pstSequenceBase);
        s_pstSequenceBase = NULL;
    }
}

/**
 * @fn pifSequence_Add
 * @brief Sequence를 추가한다.
 * @param ucId
 * @param pstPhaseList
 * @param pvParam
 * @return Sequence 구조체 포인터를 반환한다.
 */
PIF_stSequence *pifSequence_Add(uint8_t ucId, const PIF_stSequencePhase *pstPhaseList, void *pvParam)
{
    if (s_ucSequenceBasePos >= s_ucSequenceBaseSize) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

    PIF_stSequenceBase *pstBase = &s_pstSequenceBase[s_ucSequenceBasePos];
    pstBase->pstPhaseList = pstPhaseList;

    PIF_stSequence *pstOwner = &pstBase->stOwner;
    pstOwner->ucId = ucId;
    pstOwner->ucPhaseNo = PIF_SEQUENCE_PHASE_NO_IDLE;
    pstOwner->pvParam = pvParam;

    s_ucSequenceBasePos = s_ucSequenceBasePos + 1;
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
	pstOwner->ucPhaseNo = 0;
	pstOwner->ucStep = PIF_SEQUENCE_STEP_INIT;
	pstOwner->ucPhaseNoNext = PIF_SEQUENCE_PHASE_NO_IDLE;
	pstOwner->usDelay = 0;
}	

BOOL pifSequence_SetTimeout(PIF_stSequence *pstOwner, uint16_t usTimeout)
{
	PIF_stSequenceBase *pstBase = (PIF_stSequenceBase *)pstOwner;

	if (!pstBase->pstTimerTimeout) {
		pstBase->pstTimerTimeout = pifPulse_AddItem(s_pstSequenceTimer, PT_enOnce);
		if (!pstBase->pstTimerTimeout) return FALSE;
		pifPulse_AttachEvtFinish(pstBase->pstTimerTimeout, _evtTimerTimeoutFinish, pstOwner);
	}
	pifPulse_StartItem(pstBase->pstTimerTimeout, usTimeout);
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

    for (int i = 0; i < s_ucSequenceBasePos; i++) {
        PIF_stSequenceBase *pstBase = &s_pstSequenceBase[i];
    	if (!pstBase->enTaskLoop) _LoopCommon(pstBase);
    }
}

/**
 * @fn pifSequence_taskEach
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifSequence_taskEach(PIF_stTask *pstTask)
{
	PIF_stSequenceBase *pstBase = pstTask->pvLoopEach;

	if (pstBase->enTaskLoop != TL_enEach) {
		pstBase->enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstBase);
	}
}
