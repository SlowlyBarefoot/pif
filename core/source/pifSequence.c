#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSequence.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


static void _setPhaseNo(PifSequence* p_owner, uint8_t phase_no)
{
	p_owner->_phase_no = phase_no;
#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & SQ_CSF_PHASE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SQ_CSF_PHASE_IDX], p_owner->_phase_no);
	}
#endif
}

static void _evtTimerTimeoutFinish(void* p_issuer)
{
    PifSequence* p_owner = (PifSequence*)p_issuer;

	pif_error = E_TIMEOUT;
	if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
	_setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char* prefix[SQ_CSF_COUNT] = { "SQ" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		PifSequence* p_owner = p_colsig->p_owner;
		for (int f = 0; f < SQ_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_enReg, 8, prefix[f], 0xFF);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SQ_CS:Add(DC:%u F:%u)", p_owner->_id, p_colsig->flag);
#endif

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
 * @param id
 * @param p_timer
 * @param p_phase_list
 * @param p_param
 * @return Sequence 구조체 포인터를 반환한다.
 */
PifSequence* pifSequence_Create(PifId id, PifPulse* p_timer, const PifSequencePhase* p_phase_list, void* p_param)
{
    PifSequence *p_owner = NULL;

    if (!pif_act_timer1us) {
        pif_error = E_CANNOT_USE;
	    return NULL;
    }

    p_owner = calloc(sizeof(PifSequence), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    p_owner->__p_timer = p_timer;
    p_owner->__p_phase_list = p_phase_list;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    _setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
    p_owner->p_param = p_param;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSequence, _addDeviceInCollectSignal);
	PIF_SequenceColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_SequenceColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return p_owner;
}

/**
 * @fn pifSequence_Destroy
 * @brief Sequence용 메모리를 반환한다.
 * @param pp_owner
 */
void pifSequence_Destroy(PifSequence** pp_owner)
{
	if (*pp_owner) {
		PifSequence *p_owner = *pp_owner;
		if (p_owner->__p_timer_timeout) {
			pifPulse_RemoveItem(p_owner->__p_timer, p_owner->__p_timer_timeout);
		}
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSequence_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSequence_SetCsFlagAll(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSequence_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSequence_ResetCsFlagAll(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSequence_SetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_SetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

/**
 * @fn pifSequence_ResetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_ResetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

#endif

/**
 * @fn pifSequence_Start
 * @brief 
 * @param p_owner
 */
void pifSequence_Start(PifSequence* p_owner)
{
	_setPhaseNo(p_owner, 0);
	p_owner->step = PIF_SEQUENCE_STEP_INIT;
	p_owner->phase_no_next = PIF_SEQUENCE_PHASE_NO_IDLE;
	p_owner->delay1us = 0;
}	

/**
 * @fn pifSequence_SetTimeout
 * @brief
 * @param p_owner
 * @param timeout
 * @return
 */
BOOL pifSequence_SetTimeout(PifSequence* p_owner, uint16_t timeout)
{
	if (!p_owner->__p_timer_timeout) {
		p_owner->__p_timer_timeout = pifPulse_AddItem(p_owner->__p_timer, PT_ONCE);
		if (!p_owner->__p_timer_timeout) return FALSE;
		pifPulse_AttachEvtFinish(p_owner->__p_timer_timeout, _evtTimerTimeoutFinish, p_owner);
	}
	pifPulse_StartItem(p_owner->__p_timer_timeout, timeout);
	return TRUE;
}

static uint16_t _doTask(PifTask* p_task)
{
	PifSequence *p_owner = p_task->_p_client;
	const PifSequencePhase *pstPhase;
	uint8_t ucPhaseNoNext;
	
	if (p_owner->_phase_no == PIF_SEQUENCE_PHASE_NO_IDLE) return 0;

	if (p_owner->delay1us) {
		if ((*pif_act_timer1us)() >= p_owner->__target_delay) {
			p_owner->delay1us = 0;
		}
		else return 0;
	}

	pstPhase = &p_owner->__p_phase_list[p_owner->_phase_no];

	if (!pstPhase->process) {
		pif_error = E_WRONG_DATA;
		goto fail;
	}

	switch ((*pstPhase->process)(p_owner)) {
	case SR_CONTINUE:
		if (p_owner->delay1us) {
			p_owner->__target_delay = (*pif_act_timer1us)() + p_owner->delay1us;
		}
		break;

	case SR_NEXT:
		if (p_owner->__p_timer_timeout) pifPulse_StopItem(p_owner->__p_timer_timeout);

		ucPhaseNoNext = p_owner->phase_no_next;
		if (ucPhaseNoNext == PIF_SEQUENCE_PHASE_NO_IDLE) {
			ucPhaseNoNext = pstPhase->phase_no_next;
		}

		if (ucPhaseNoNext != PIF_SEQUENCE_PHASE_NO_IDLE) {
			if (p_owner->delay1us) {
				p_owner->__target_delay = (*pif_act_timer1us)() + p_owner->delay1us;
			}
			p_owner->step = PIF_SEQUENCE_STEP_INIT;
			p_owner->phase_no_next = PIF_SEQUENCE_PHASE_NO_IDLE;
		}
		_setPhaseNo(p_owner, ucPhaseNoNext);
		break;

	case SR_FINISH:
		_setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
		break;

	default:
		pif_error = E_WRONG_DATA;
		goto fail;
	}
	return 0;

fail:
	if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
	_setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
	return 0;
}

/**
 * @fn pifSequence_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifSequence_AttachTask(PifSequence* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_owner, start);
}
