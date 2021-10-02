#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSequence.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PIF_DList s_cs_list;
#endif


static void _SetPhaseNo(PIF_stSequence *pstOwner, uint8_t ucPhaseNo)
{
	pstOwner->_ucPhaseNo = ucPhaseNo;
#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__p_colsig->flag & SqCsF_enPhaseBit) {
		pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[SqCsF_enPhaseIdx], pstOwner->_ucPhaseNo);
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

#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char *prefix[SqCsF_enCount] = { "SQ" };

	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		PIF_stSequence* pstOwner = p_colsig->p_owner;
		for (int f = 0; f < SqCsF_enCount; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enReg, 8, prefix[f], 0xFF);
			}
		}
		pifLog_Printf(LT_enInfo, "SQ_CS:Add(DC:%u F:%u)", pstOwner->_usPifId, p_colsig->flag);

		it = pifDList_Next(it);
	}
}

void pifSequence_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifSequence_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifSequence_Create
 * @brief Sequence를 추가한다.
 * @param usPifId
 * @param pstTimer
 * @param pstPhaseList
 * @param pvParam
 * @return Sequence 구조체 포인터를 반환한다.
 */
PIF_stSequence *pifSequence_Create(PIF_usId usPifId, PIF_stPulse *pstTimer, const PIF_stSequencePhase *pstPhaseList, void *pvParam)
{
    PIF_stSequence *pstOwner = NULL;

    if (!pif_actTimer1us) {
        pif_enError = E_enCanNotUse;
        goto fail;
    }

    pstOwner = calloc(sizeof(PIF_stSequence), 1);
    if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    pstOwner->__pstTimer = pstTimer;
    pstOwner->__pstPhaseList = pstPhaseList;

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    _SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
    pstOwner->pvParam = pvParam;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSequence, _AddDeviceInCollectSignal);
	PIF_SequenceColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_SequenceColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = pstOwner;
	pstOwner->__p_colsig = p_colsig;
#endif
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SQ:Add() EC:%d", pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifSequence_Destroy
 * @brief Sequence용 메모리를 반환한다.
 * @param pp_owner
 */
void pifSequence_Destroy(PIF_stSequence** pp_owner)
{
	if (*pp_owner) {
		PIF_stSequence *pstOwner = *pp_owner;
		if (pstOwner->__pstTimerTimeout) {
			pifPulse_RemoveItem(pstOwner->__pstTimer, pstOwner->__pstTimerTimeout);
		}
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSequence_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSequence_SetCsFlagAll(PIF_enSequenceCsFlag enFlag)
{
	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag |= enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSequence_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifSequence_ResetCsFlagAll(PIF_enSequenceCsFlag enFlag)
{
	PIF_DListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag &= ~enFlag;
		it = pifDList_Next(it);
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
	pstOwner->__p_colsig->flag |= enFlag;
}

/**
 * @fn pifSequence_ResetCsFlagEach
 * @brief
 * @param pstOwner
 * @param enFlag
 */
void pifSequence_ResetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag)
{
	pstOwner->__p_colsig->flag &= ~enFlag;
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
	pstOwner->unDelay1us = 0;
}	

BOOL pifSequence_SetTimeout(PIF_stSequence *pstOwner, uint16_t usTimeout)
{
	if (!pstOwner->__pstTimerTimeout) {
		pstOwner->__pstTimerTimeout = pifPulse_AddItem(pstOwner->__pstTimer, PT_enOnce);
		if (!pstOwner->__pstTimerTimeout) return FALSE;
		pifPulse_AttachEvtFinish(pstOwner->__pstTimerTimeout, _evtTimerTimeoutFinish, pstOwner);
	}
	pifPulse_StartItem(pstOwner->__pstTimerTimeout, usTimeout);
	return TRUE;
}

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	PIF_stSequence *pstOwner = pstTask->_pvClient;
	const PIF_stSequencePhase *pstPhase;
	uint8_t ucPhaseNoNext;
	
	if (pstOwner->_ucPhaseNo == PIF_SEQUENCE_PHASE_NO_IDLE) return 0;

	if (pstOwner->unDelay1us) {
		if ((*pif_actTimer1us)() >= pstOwner->__unTargetDelay) {
			pstOwner->unDelay1us = 0;
		}
		else return 0;
	}

	pstPhase = &pstOwner->__pstPhaseList[pstOwner->_ucPhaseNo];

	if (!pstPhase->fnProcess) {
		pif_enError = E_enWrongData;
		goto fail;
	}

	switch ((*pstPhase->fnProcess)(pstOwner)) {
	case SR_enContinue:
		if (pstOwner->unDelay1us) {
			pstOwner->__unTargetDelay = (*pif_actTimer1us)() + pstOwner->unDelay1us;
		}
		break;

	case SR_enNext:
		if (pstOwner->__pstTimerTimeout) pifPulse_StopItem(pstOwner->__pstTimerTimeout);

		ucPhaseNoNext = pstOwner->ucPhaseNoNext;
		if (ucPhaseNoNext == PIF_SEQUENCE_PHASE_NO_IDLE) {
			ucPhaseNoNext = pstPhase->ucPhaseNoNext;
		}

		if (ucPhaseNoNext != PIF_SEQUENCE_PHASE_NO_IDLE) {
			if (pstOwner->unDelay1us) {
				pstOwner->__unTargetDelay = (*pif_actTimer1us)() + pstOwner->unDelay1us;
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
	return 0;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "SQ:Error(%d) EC:%d", pstOwner->_ucPhaseNo, pif_enError);
#endif
	if (pstOwner->evtError) (*pstOwner->evtError)(pstOwner);
	_SetPhaseNo(pstOwner, PIF_SEQUENCE_PHASE_NO_IDLE);
	return 0;
}

/**
 * @fn pifSequence_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifSequence_AttachTask(PIF_stSequence *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}
