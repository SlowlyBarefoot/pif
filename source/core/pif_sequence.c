#include "core/pif_sequence.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_dlist.h"
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif

// Step-based sequence runner with task-driven execution.

#ifdef PIF_COLLECT_SIGNAL
	static PifDList s_cs_list;
#endif


static uint32_t _doTask(PifTask* p_task)
{
	PifSequence *p_owner = (PifSequence *)p_task->_p_client;

	if (p_owner->__process) {
		(*p_owner->__process)(p_owner);
		p_owner->__process = NULL;
		if (p_owner->__next_process) {
			p_owner->__process = p_owner->__next_process;
			p_owner->__next_process = NULL;
		}
	}
	return 0;
}

static void _evtTimerTimeoutFinish(PifIssuerP p_issuer)
{
    PifSequence* p_owner = (PifSequence*)p_issuer;

	pif_error = E_TIMEOUT;
	if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
	p_owner->__process = NULL;
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

BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifTimerManager* p_timer_manager, void* p_param)
{
    if (!p_owner || !p_timer_manager) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifSequence));

    p_owner->__p_timer_manager = p_timer_manager;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->p_param = p_param;

    p_owner->__p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, p_owner, TRUE);
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

BOOL pifSequence_IsRunning(PifSequence *p_owner)
{
	return p_owner->__process != NULL;
}

BOOL pifSequence_Start(PifSequence *p_owner, PifSequenceProcess process)
{
	if (p_owner->__process) return FALSE; 
	if (!pifTask_SetTrigger(p_owner->__p_task, 0)) return FALSE;
	p_owner->__process = process;
	return TRUE;
}

BOOL pifSequence_NextDelay(PifSequence *p_owner, PifSequenceProcess process, uint16_t delay1ms)
{
	if (!pifTask_SetTrigger(p_owner->__p_task, delay1ms * 1000)) return FALSE;
	p_owner->__next_process = process;
	return TRUE;
}

BOOL pifSequence_NextEvent(PifSequence *p_owner, PifSequenceProcess process, uint16_t timeout1ms)
{
	if (timeout1ms) {
		if (!p_owner->__p_timer_timeout) {
			p_owner->__p_timer_timeout = pifTimerManager_Add(p_owner->__p_timer_manager, TT_ONCE);
			if (!p_owner->__p_timer_timeout) return FALSE;
			pifTimer_AttachEvtFinish(p_owner->__p_timer_timeout, _evtTimerTimeoutFinish, p_owner);
		}
		pifTimer_Start(p_owner->__p_timer_timeout, timeout1ms);
	}
	p_owner->__next_process = process;
	return TRUE;
}

void pifSequence_TriggerEvent(PifSequence *p_owner)
{
	if (p_owner->__p_timer_timeout)	{
		pifTimer_Stop(p_owner->__p_timer_timeout);
		p_owner->__p_timer_timeout = NULL;
	}
	pifTask_SetTrigger(p_owner->__p_task, 0);
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
