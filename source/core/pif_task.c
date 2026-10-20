// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_task.h"

// Task lifecycle and scheduling state transitions.

extern PifTask *g_task_cutin;

#ifdef PIF_USE_TASK_REALTIME
extern PifTask *g_realtime_task;
#endif


// Judged against the reading the scheduler took at the start of the pass rather than a new one.
// A pass visits every task but dispatches at most one, so the visits that answer no would each
// cost a timer read otherwise. The same reading becomes __pretime on a dispatch, which keeps the
// release grid of every task measured on one clock.
static PifTask* _processingPeriod(PifTask* p_owner)
{
	p_owner->_delta_time = pif_timer1us - p_owner->__pretime;
	if (p_owner->_delta_time >= p_owner->__period) {
		p_owner->__current_time = pif_timer1us;
		return p_owner;
	}
	return NULL;
}

// Brings the trigger of the task forward to the delay, and leaves a nearer one alone. A cut in is
// nearer than any delay. A trigger time later than the reading means an interrupt set it after
// the reading was taken, so none of its delay has elapsed.
static void _triggerBefore(PifTask *p_task, uint32_t now, uint32_t delay)
{
	uint32_t elapsed;

	if (g_task_cutin == p_task) return;
	if (p_task->__trigger) {
		elapsed = now - p_task->__trigger_time;
		if ((int32_t)elapsed < 0) elapsed = 0;
		if (elapsed >= p_task->__trigger_delay || p_task->__trigger_delay - elapsed <= delay) return;
	}
	pifTask_SetTrigger(p_task, delay);
}


void pifTask_Init(PifTask* p_owner, PifId id)
{
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
}

BOOL pifTask_CheckParam(PifTaskMode* p_mode, uint32_t period)
{
	switch (*p_mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
		// Only one task can hold the realtime slot.
		if (g_realtime_task) {
			pif_error = E_CANNOT_USE;
			return FALSE;
		}
		// A period of zero is accepted here, unlike TM_PERIOD, and leaves the task released by
		// pifTask_SetTrigger() alone.
    	break;
#endif

    case TM_PERIOD:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	break;
    	
	case TM_EXTERNAL:
		break;

    default:
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }
	return TRUE;
}

BOOL pifTask_SetParam(PifTask* p_owner, PifTaskMode mode, uint32_t period)
{
    switch (mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	// With no period there is no release to poll for, only triggers. _processingPeriod would
    	// report every pass as due.
    	p_owner->__processing = period ? _processingPeriod : NULL;
		g_realtime_task = p_owner;
		break;
#endif

    case TM_PERIOD:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriod;
    	break;

    case TM_EXTERNAL:
    	period = 0;
    	p_owner->__processing = NULL;
    	break;

    default:
    	break;
    }

    p_owner->_mode = mode;
    p_owner->_default_period = period;
    p_owner->__period = period;
	return TRUE;
}

BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint32_t period)
{
	if (mode == p_owner->_mode) return TRUE;

	if (!pifTask_CheckParam(&mode, period)) return FALSE;

	// The realtime slot is released only after the change is certain. Otherwise a failed change
	// leaves a task whose mode is TM_REALTIME while another task can take the slot.
#ifdef PIF_USE_TASK_REALTIME
	if (p_owner->_mode == TM_REALTIME) g_realtime_task = NULL;
#endif

	if (!pifTask_SetParam(p_owner, mode, period)) return FALSE;

    return TRUE;
}

BOOL pifTask_ChangePeriod(PifTask* p_owner, uint32_t period)
{
	switch (p_owner->_mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
		// Dropping the period leaves the task released by trigger alone, so the periodic check
		// has to go with it.
		p_owner->__processing = period ? _processingPeriod : NULL;
		break;
#endif

	case TM_PERIOD:
		if (!period) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}
		break;

	default:
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	p_owner->_default_period = period;
	p_owner->__period = period;
	return TRUE;
}

BOOL pifTask_SetTrigger(PifTask* p_owner, uint32_t delay)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__trigger_time = (*pif_act_timer1us)();
	p_owner->__trigger = TRUE;
	p_owner->__trigger_delay = delay;
	return TRUE;
}

BOOL pifTask_SetCutinTrigger(PifTask *p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// The time is set in both paths so that the trigger statistics measure the cut in latency.
	p_owner->__trigger_time = (*pif_act_timer1us)();
	if (g_task_cutin) {
		p_owner->__trigger = TRUE;
		p_owner->__trigger_delay = 0;
	}
	else {
		g_task_cutin = p_owner;
	}
	return TRUE;
}

void pifTask_CancelTrigger(PifTask *p_owner)
{
	p_owner->__trigger = FALSE;
	p_owner->__trigger_delay = 0;
	if (g_task_cutin == p_owner) g_task_cutin = NULL;
}

void pifTask_Pause(PifTask *p_owner)
{
	p_owner->_pause = TRUE;
}

void pifTask_Resume(PifTask *p_owner)
{
	p_owner->_pause = FALSE;
}

void pifTask_Restart(PifTask *p_owner)
{
	switch (p_owner->_mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
#endif
	case TM_PERIOD:
		p_owner->__pretime = (*pif_act_timer1us)();
		p_owner->__period = p_owner->_default_period;
		break;

	default:
		break;
	}
	pifTask_Resume(p_owner);
}

BOOL pifTask_IsSubPeriodElapsed(uint32_t *p_time_us, uint32_t period_us)
{
	uint32_t now = (*pif_act_timer1us)();

	if (now - *p_time_us < period_us) return FALSE;
	*p_time_us += period_us;
	if (now - *p_time_us >= period_us) *p_time_us = now;
	return TRUE;
}

#ifdef PIF_USE_TASK_STATISTICS

void pifTask_ResetStatistics(PifTask* p_owner)
{
    p_owner->_total_execution_time = 0UL;
    p_owner->_max_execution_time = 0L;
	p_owner->_max_trigger_delay = 0UL;
	p_owner->_max_delay = 0UL;
	// PIF_USE_TASK_STATISTICS implies PIF_USE_BLOCK_TIME, and the block time of a task is
	// owned by the function below.
	pifTask_ResetMaxBlockTime(p_owner);

	p_owner->__total_delta_time[0] = 0UL;
	p_owner->__total_delta_time[1] = 0UL;
    p_owner->__sum_execution_time[0] = 0UL;
    p_owner->__sum_execution_time[1] = 0UL;
	p_owner->__total_trigger_delay[0] = 0UL;
	p_owner->__total_trigger_delay[1] = 0UL;
	p_owner->__execution_count = 0;
	p_owner->__trigger_count = 0;
	p_owner->__execute_index = 0;
	p_owner->__trigger_index = 0;
}

void pifTask_ResetMaxExecutionTime(PifTask* p_owner)
{
    p_owner->_max_execution_time = 0L;
}

#endif

#ifdef PIF_USE_BLOCK_TIME

void pifTask_ResetBlockTime(PifBlockTime *p_owner)
{
	p_owner->_max = 0UL;
	p_owner->__bucket[0] = 0UL;
	p_owner->__bucket[1] = 0UL;
	p_owner->__count = 0;
	p_owner->__index = 0;
}

// One bucket collects while the other still holds the previous 100 runs, and the incoming bucket
// is cleared as they swap. The reported maximum is therefore the largest of the last 100 to 200
// runs and an outlier leaves it once the bucket holding it is cleared.
void pifTask_UpdateBlockTime(PifBlockTime *p_owner, uint32_t block_time)
{
	if (block_time > p_owner->__bucket[p_owner->__index]) {
		p_owner->__bucket[p_owner->__index] = block_time;
	}
	p_owner->__count++;
	if (p_owner->__count == 200) {
		p_owner->__count -= 100;
		p_owner->__index ^= 1;
		p_owner->__bucket[p_owner->__index] = 0UL;
	}
	else if (p_owner->__count == 100) {
		p_owner->__index ^= 1;
	}
	p_owner->_max = (p_owner->__bucket[0] > p_owner->__bucket[1]) ?
			p_owner->__bucket[0] : p_owner->__bucket[1];
}

void pifTask_ResetMaxBlockTime(PifTask *p_owner)
{
	pifTask_ResetBlockTime(&p_owner->_block_time);
}

void pifTask_SetNextBlockTime(PifTask *p_owner, uint32_t block_time)
{
	p_owner->__next_block_time = block_time;
}

void pifTask_IgnoreBlockTime(PifTask *p_owner)
{
	p_owner->__ignore_block = TRUE;
}

#endif


void pifTaskTimeout_Init(PifTaskTimeout *p_owner, PifTask *p_task)
{
	p_owner->__p_task = p_task;
	p_owner->__start = 0;
	p_owner->__time = 0;
}

void pifTaskTimeout_Start(PifTaskTimeout *p_owner, uint32_t time_us)
{
	p_owner->__start = (*pif_act_timer1us)();
	p_owner->__time = time_us ? time_us : 1;
	if (p_owner->__p_task) _triggerBefore(p_owner->__p_task, p_owner->__start, p_owner->__time);
}

void pifTaskTimeout_Stop(PifTaskTimeout *p_owner)
{
	p_owner->__time = 0;
}

BOOL pifTaskTimeout_IsRunning(PifTaskTimeout *p_owner)
{
	return p_owner->__time != 0;
}

BOOL pifTaskTimeout_IsExpired(PifTaskTimeout *p_owner)
{
	uint32_t now, elapsed;

	if (!p_owner->__time) return FALSE;
	now = (*pif_act_timer1us)();
	elapsed = now - p_owner->__start;
	if (elapsed < p_owner->__time) {
		if (p_owner->__p_task) _triggerBefore(p_owner->__p_task, now, p_owner->__time - elapsed);
		return FALSE;
	}
	p_owner->__time = 0;
	return TRUE;
}
