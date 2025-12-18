#include "core/pif_task.h"


#ifdef PIF_DEBUG

PifActTaskSignal pif_act_task_signal = NULL;

#endif

extern PifTask* s_task_cutin;


static PifTask* _processingAlways(PifTask* p_owner)
{
	if (p_owner->__delay_us) {
		p_owner->_delta_time = (*pif_act_timer1us)() - p_owner->__pretime;
		if (p_owner->_delta_time >= p_owner->__delay_us) {
			p_owner->__delay_us = 0;
		}
		return NULL;
	}
	return p_owner;
}

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
    case TM_PERIOD:
    case TM_IDLE:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	break;
    	
	case TM_TIMER:
	case TM_ALWAYS:
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
    case TM_ALWAYS:
    	p_owner->__processing = _processingAlways;
    	break;

    case TM_PERIOD:
    case TM_IDLE:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriod;
    	break;

	case TM_TIMER:
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

	if (!pifTask_SetParam(p_owner, mode, period)) return FALSE;

    return TRUE;
}

BOOL pifTask_ChangePeriod(PifTask* p_owner, uint32_t period)
{
	switch (p_owner->_mode) {
	case TM_PERIOD:
	case TM_IDLE:
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

	if (s_task_cutin) {
		p_owner->__trigger_time = (*pif_act_timer1us)();
		p_owner->__trigger = TRUE;
		p_owner->__trigger_delay = 0;
	}
	else {
		s_task_cutin = p_owner;
	}
	return TRUE;
}

BOOL pifTask_SetTriggerForTimer(PifTask *p_owner)
{
	if (!p_owner) return FALSE;

	p_owner->__timer_trigger++;
	return TRUE;
}

void pifTask_DelayMs(PifTask* p_owner, uint16_t delay)
{
	switch (p_owner->_mode) {
	case TM_ALWAYS:
		p_owner->__delay_us = delay * 1000;
    	p_owner->__pretime = (*pif_act_timer1us)();
		break;

	default:
		break;
	}
}

#ifdef PIF_USE_TASK_STATISTICS

PIF_INLINE uint32_t pifTask_GetAverageDeltaTime(PifTask* p_owner)
{
	if (p_owner->__execution_count < 20) return 0;
	return (p_owner->__total_delta_time[0] + p_owner->__total_delta_time[1]) / p_owner->__execution_count;
}

PIF_INLINE uint32_t pifTask_GetAverageExecuteTime(PifTask* p_owner)
{
	if (p_owner->__execution_count < 20) return 0;
	return (p_owner->__sum_execution_time[0] + p_owner->__sum_execution_time[1]) / p_owner->__execution_count;
}

PIF_INLINE uint32_t pifTask_GetAverageTriggerTime(PifTask* p_owner)
{
	if (p_owner->__trigger_count < 20) return 0;
	return (p_owner->__total_trigger_delay[0] + p_owner->__total_trigger_delay[1]) / p_owner->__trigger_count;
}

#endif
