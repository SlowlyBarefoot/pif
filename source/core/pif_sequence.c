#include "core/pif_sequence.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_list.h"
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif


#ifdef PIF_COLLECT_SIGNAL
	static PifDList s_cs_list;
#endif


static void _setPhaseNo(PifSequence* p_owner, uint8_t phase_no)
{
	p_owner->_phase_no = phase_no;
#ifdef PIF_COLLECT_SIGNAL
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
		if (p_owner->__p_timer_timeout) pifTimer_Stop(p_owner->__p_timer_timeout);

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

static void _evtTimerTimeoutFinish(PifIssuerP p_issuer)
{
    PifSequence* p_owner = (PifSequence*)p_issuer;

	pif_error = E_TIMEOUT;
	if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
	_setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
}

#ifdef PIF_COLLECT_SIGNAL

static void _addDeviceInCollectSignal()
{
	const char* prefix[SQ_CSF_COUNT] = { "SQ" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSequenceColSig* p_colsig = (PifSequenceColSig*)it->data;
		PifSequence* p_owner = p_colsig->p_owner;
		for (int f = 0; f < SQ_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_REG, 8, prefix[f], 0xFF);
			}
		}
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "SQ_CS:Add(DC:%u F:%u)", p_owner->_id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL

BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t control_period1ms,
		const PifSequencePhase* p_phase_list, void* p_param)
{
    if (!p_owner || !p_timer_manager || !p_phase_list) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifSequence));

    p_owner->__p_timer_manager = p_timer_manager;
    p_owner->__p_phase_list = p_phase_list;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    _setPhaseNo(p_owner, PIF_SEQUENCE_PHASE_NO_IDLE);
    p_owner->p_param = p_param;

    p_owner->__p_task = pifTaskManager_Add(TM_PERIOD_MS, control_period1ms, _doTask, p_owner, TRUE);
	if (!p_owner->__p_task) goto fail;
	p_owner->__p_task->name = "Sequence";

#ifdef PIF_COLLECT_SIGNAL
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_SEQUENCE, _addDeviceInCollectSignal);
	}
	PifSequenceColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSequenceColSig));
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
#ifdef PIF_COLLECT_SIGNAL
	pifDList_Remove(&s_cs_list, p_owner->__p_colsig);
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_SEQUENCE);
	}
	p_owner->__p_colsig = NULL;
#endif
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	if (p_owner->__p_timer_timeout) {
		pifTimerManager_Remove(p_owner->__p_timer_timeout);
		p_owner->__p_timer_timeout = NULL;
	}
}

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
		p_owner->__p_timer_timeout = pifTimerManager_Add(p_owner->__p_timer_manager, TT_ONCE);
		if (!p_owner->__p_timer_timeout) return FALSE;
		pifTimer_AttachEvtFinish(p_owner->__p_timer_timeout, _evtTimerTimeoutFinish, p_owner);
	}
	pifTimer_Start(p_owner->__p_timer_timeout, timeout);
	return TRUE;
}


#ifdef PIF_COLLECT_SIGNAL

void pifSequence_SetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifSequence_ResetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifSequenceColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifSequenceColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifSequenceColSig_SetFlag(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSequenceColSig* p_colsig = (PifSequenceColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifSequenceColSig_ResetFlag(PifSequenceCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSequenceColSig* p_colsig = (PifSequenceColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL
