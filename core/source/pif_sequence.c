#ifdef __PIF_COLLECT_SIGNAL__
#include "pif_collect_signal.h"
#endif
#include "pif_list.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif
#include "pif_sequence.h"


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
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_REG, 8, prefix[f], 0xFF);
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

PifSequence* pifSequence_Create(PifId id, PifPulse* p_timer, uint16_t control_period1ms,
		const PifSequencePhase* p_phase_list, void* p_param)
{
    PifSequence *p_owner = malloc(sizeof(PifSequence));
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

	if (!pifSequence_Init(p_owner, id, p_timer, control_period1ms, p_phase_list, p_param)) {
		pifSequence_Destroy(&p_owner);
	    return NULL;
	}
    return p_owner;
}

void pifSequence_Destroy(PifSequence** pp_owner)
{
	if (*pp_owner) {
		pifSequence_Clear(*pp_owner);
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifPulse* p_timer, uint16_t control_period1ms,
		const PifSequencePhase* p_phase_list, void* p_param)
{
    if (!p_owner || !p_timer || !p_phase_list) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    if (!pif_act_timer1us) {
        pif_error = E_CANNOT_USE;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifSequence));

    p_owner->__p_timer = p_timer;
    p_owner->__p_phase_list = p_phase_list;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    _setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
    p_owner->p_param = p_param;

	if (!pifTaskManager_Add(TM_PERIOD_MS, control_period1ms, _doTask, p_owner, FALSE)) goto fail;

#ifdef __PIF_COLLECT_SIGNAL__
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_SEQUENCE, _addDeviceInCollectSignal);
	}
	PIF_SequenceColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_SequenceColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return TRUE;

fail:
	pifSequence_Clear(p_owner);
	return FALSE;
}

void pifSequence_Clear(PifSequence* p_owner)
{
#ifdef __PIF_COLLECT_SIGNAL__
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		if (p_colsig == p_owner->__p_colsig) {
			pifDList_Remove(&s_cs_list, it);
			break;
		}
		it = pifDList_Next(it);
	}
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_SEQUENCE);
	}
	p_owner->__p_colsig = NULL;
#endif
	if (p_owner->__p_timer_timeout) {
		pifPulse_RemoveItem(p_owner->__p_timer_timeout);
		p_owner->__p_timer_timeout = NULL;
	}
}

#ifdef __PIF_COLLECT_SIGNAL__

void pifSequence_SetCsFlagAll(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifSequence_ResetCsFlagAll(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_SequenceColSig* p_colsig = (PIF_SequenceColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

void pifSequence_SetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifSequence_ResetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

#endif

void pifSequence_Start(PifSequence* p_owner)
{
	_setPhaseNo(p_owner, 0);
	p_owner->step = PIF_SEQUENCE_STEP_INIT;
	p_owner->phase_no_next = PIF_SEQUENCE_PHASE_NO_IDLE;
	p_owner->delay1us = 0;
}	

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
