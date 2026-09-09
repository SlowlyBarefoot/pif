#include "core/pif_task.h"

// Task lifecycle and scheduling state transitions.

extern PifTask *g_task_cutin;

extern PifTask *g_realtime_task;


static PifTask* _processingPeriod(PifTask* p_owner)
{
	uint32_t current;

	current = (*pif_act_timer1us)();
	p_owner->_delta_time = current - p_owner->__pretime;
	if (p_owner->_delta_time >= p_owner->__period) {
		p_owner->__current_time = current;
		return p_owner;
	}
	return NULL;
}

void pifTask_Init(PifTask* p_owner, PifId id)
{
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
}

BOOL pifTask_CheckParam(PifTaskMode* p_mode, uint32_t period)
{
	switch (*p_mode) {
	case TM_REALTIME:
		// Only one task can hold the realtime slot.
		if (g_realtime_task) {
			pif_error = E_CANNOT_USE;
			return FALSE;
		}
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	break;

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
	case TM_REALTIME:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriod;
		g_realtime_task = p_owner;
		break;

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
	if (p_owner->_mode == TM_REALTIME) g_realtime_task = NULL;

	if (!pifTask_SetParam(p_owner, mode, period)) return FALSE;

    return TRUE;
}

BOOL pifTask_ChangePeriod(PifTask* p_owner, uint32_t period)
{
	switch (p_owner->_mode) {
	case TM_REALTIME:
	case TM_PERIOD:
		p_owner->_default_period = period;
		p_owner->__period = period;
		break;

	default:
		pif_error = E_CANNOT_USE;
		return FALSE;
	}
	return TRUE;
}

BOOL pifTask_SetTrigger(PifTask* p_owner, uint32_t delay)
{
	if (!p_owner) return FALSE;

	p_owner->__trigger_time = (*pif_act_timer1us)();
	p_owner->__trigger = TRUE;
	p_owner->__trigger_delay = delay;
	return TRUE;
}

BOOL pifTask_SetCutinTrigger(PifTask *p_owner)
{
	if (!p_owner) return FALSE;

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

#ifdef PIF_USE_TASK_STATISTICS

void pifTask_ResetStatistics(PifTask* p_owner)
{
    p_owner->_total_execution_time = 0UL;
    p_owner->_max_execution_time = 0L;
	p_owner->_max_trigger_delay = 0UL;
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

PIF_INLINE uint32_t pifTask_GetAverageDeltaTime(PifTask* p_owner)
{
	if (p_owner->__execution_count < PIF_TASK_AVERAGE_MIN_COUNT) return PIF_TASK_AVERAGE_NONE;
	return (p_owner->__total_delta_time[0] + p_owner->__total_delta_time[1]) / p_owner->__execution_count;
}

PIF_INLINE uint32_t pifTask_GetAverageExecuteTime(PifTask* p_owner)
{
	if (p_owner->__execution_count < PIF_TASK_AVERAGE_MIN_COUNT) return PIF_TASK_AVERAGE_NONE;
	return (p_owner->__sum_execution_time[0] + p_owner->__sum_execution_time[1]) / p_owner->__execution_count;
}

PIF_INLINE uint32_t pifTask_GetAverageTriggerTime(PifTask* p_owner)
{
	if (p_owner->__trigger_count < PIF_TASK_AVERAGE_MIN_COUNT) return PIF_TASK_AVERAGE_NONE;
	return (p_owner->__total_trigger_delay[0] + p_owner->__total_trigger_delay[1]) / p_owner->__trigger_count;
}

#endif

#ifdef PIF_USE_BLOCK_TIME

void pifTask_ResetMaxBlockTime(PifTask *p_owner)
{
    p_owner->_max_block_time = 0UL;
}

#endif
