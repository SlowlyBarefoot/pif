#include "core/pif_log.h"
#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"

#include <string.h>

// Central task scheduler and runtime execution loop management.

#ifndef PIF_TASK_STACK_SIZE
#define PIF_TASK_STACK_SIZE		5
#endif


static PifObjArray s_tasks;
static PifObjArrayIterator s_it_current;
static PifTask *s_task_stack[PIF_TASK_STACK_SIZE];
static int s_task_stack_ptr = 0;
static PifTask *s_current_task = NULL;
static uint32_t s_loop_count = 0UL, s_pass_count = 0UL;
static uint32_t s_task_load_start_time = 0UL;

static PifObjArray s_timers;

PifTask *s_task_cutin = NULL;

PifEvtTaskIdle pif_evt_task_idle = NULL;


static void _processingTrigger(PifTask *p_owner)
{
	uint32_t current;

	switch (p_owner->_mode) {
	case TM_PERIOD:
	case TM_EXTERNAL:
		current = (*pif_act_timer1us)();
		p_owner->_delta_time = current - p_owner->__pretime;
		p_owner->__current_time = current;
		break;

	default:
		break;
	}
}

static void _processingTask(PifTask *p_owner, BOOL trigger)
{
	uint32_t period;
	uint32_t start_time;
	uint32_t execute_time;
	BOOL outermost_task;
#ifdef PIF_USE_TASK_STATISTICS
	uint16_t trigger_delay;
#else
	(void)trigger;
#endif

	if (s_task_stack_ptr >= PIF_TASK_STACK_SIZE) return;

	p_owner->__pretime = p_owner->__current_time;

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(TRUE);
#endif

#ifdef PIF_USE_TASK_STATISTICS
	if (trigger) {
		trigger_delay = (*pif_act_timer1us)() - p_owner->__trigger_time;
		if (trigger_delay > p_owner->_max_trigger_delay) p_owner->_max_trigger_delay = trigger_delay;
		p_owner->__total_trigger_delay[p_owner->__trigger_index] += trigger_delay;
		p_owner->__trigger_count++;
		if (p_owner->__trigger_count == 200) {
			p_owner->__trigger_count -= 100;
			p_owner->__trigger_index ^= 1;
			p_owner->__total_trigger_delay[p_owner->__trigger_index] = 0;
		}
		else if (p_owner->__trigger_count == 100) {
			p_owner->__trigger_index ^= 1;
		}
	}
#endif

	s_current_task = p_owner;
	outermost_task = s_task_stack_ptr == 0;
    s_task_stack[s_task_stack_ptr] = p_owner;
	s_task_stack_ptr++;
	p_owner->_running = TRUE;
	p_owner->_last_execute_time = (*pif_act_timer1us)();
	start_time = p_owner->_last_execute_time;
	period = (*p_owner->__evt_loop)(p_owner);
	execute_time = (*pif_act_timer1us)() - start_time;
	if (outermost_task) pif_performance._task_time1us += execute_time;
#ifdef PIF_USE_TASK_STATISTICS
	p_owner->_total_execution_time += execute_time;
	if (execute_time > p_owner->_max_execution_time) p_owner->_max_execution_time = execute_time;
	p_owner->__total_delta_time[p_owner->__execute_index] += p_owner->_delta_time;
	p_owner->__sum_execution_time[p_owner->__execute_index] += execute_time;
	p_owner->__execution_count++;
	if (p_owner->__execution_count == 200) {
		p_owner->__execution_count -= 100;
		p_owner->__execute_index ^= 1;
		p_owner->__total_delta_time[p_owner->__execute_index] = 0;
		p_owner->__sum_execution_time[p_owner->__execute_index] = 0;
	}
	else if (p_owner->__execution_count == 100) {
		p_owner->__execute_index ^= 1;
	}
#endif
	p_owner->_running = FALSE;
	s_task_stack_ptr--;
	s_task_stack[s_task_stack_ptr] = NULL;
    s_current_task = (s_task_stack_ptr > 0) ? s_task_stack[s_task_stack_ptr - 1] : NULL;

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(FALSE);
#endif

	switch (p_owner->_mode) {
	case TM_PERIOD:
		if (period > 0) {
			p_owner->__period = period;
		}
		else {
			p_owner->__period = p_owner->_default_period;
		}
		break;

	case TM_EXTERNAL:
		if (period > 0) {
			p_owner->__trigger_time = (*pif_act_timer1us)();
			p_owner->__trigger = TRUE;
			p_owner->__trigger_delay = period;
		}
		break;

	default:
		break;
	}
}

static void _processingIdle()
{
	if (!pif_evt_task_idle) return;

	(*pif_evt_task_idle)();
}

static void _updateCpuLoad()
{
	uint32_t current_time = (*pif_act_timer1us)();
	uint32_t elapsed_time = current_time - s_task_load_start_time;
	uint32_t task_time = pif_performance._task_time1us;

	if (!elapsed_time) return;
	if (task_time > elapsed_time) task_time = elapsed_time;
	pif_performance._task_load = (uint8_t)(((uint64_t)task_time * 100) / elapsed_time);
	// Keep the accumulation window short to avoid counter overflow during long operation.
	if (elapsed_time >= 1000000UL) {
		s_task_load_start_time = current_time;
		pif_performance._task_time1us = 0UL;
	}
}

static void _checkLoopTime()
{
	static uint8_t timer_10ms = 0;
#if defined(PIF_DEBUG) || !defined(PIF_NO_LOG)
	uint32_t value;
#endif
#ifdef PIF_DEBUG
	static uint32_t pretime = 0UL;
	static uint32_t max_loop = 0UL;

	value = (*pif_act_timer1us)() - pretime;
	if (value > pif_performance.__max_loop_time1us) {
		pif_performance.__max_loop_time1us = value;
	}
	pretime = (*pif_act_timer1us)();
#endif

	pif_performance._count++;

	if (pif_performance.__state & 1) {		// 1ms
		if (timer_10ms) timer_10ms--;
		else {
			timer_10ms = 9;

			pif_performance._use_rate = 100 - 100 * s_pass_count / s_loop_count;
			s_loop_count = 0UL;
			s_pass_count = 0UL;
		}
	}

	if (pif_performance.__state & 2) {		// 1sec
#ifdef PIF_DEBUG
		if (pif_performance.__max_loop_time1us > max_loop) max_loop = pif_performance.__max_loop_time1us;
	#ifndef PIF_NO_LOG
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%uns(%lur/s) MLT=%luus", value, pif_performance._count, pif_performance.__max_loop_time1us);
		}
	#endif
		pif_performance.__max_loop_time1us = 0UL;
#else
	#ifndef PIF_NO_LOG
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%uns(%lur/s)", value, pif_performance._count);
        }
	#endif
#endif
    	pif_performance._count = 0;
	}

#ifdef PIF_DEBUG
	if (pif_performance.__state & 4) {		// 1min
	#ifndef PIF_NO_LOG
    	pifLog_Printf(LT_INFO, "MLT=%luus", max_loop);
	#endif
		max_loop = 0UL;
    }
#endif

	pif_performance.__state = 0;
}

BOOL pifTaskManager_Init(int max_count, int timer_count)
{
	if (!max_count) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	if (!pifObjArray_Init(&s_tasks, sizeof(PifTask), max_count, NULL)) return FALSE;
	s_it_current = NULL;
	s_task_load_start_time = (*pif_act_timer1us)();
	pif_performance._task_time1us = 0UL;
	pif_performance._task_load = 0;

	if (timer_count) {
		if (!pifObjArray_Init(&s_timers, sizeof(PifTaskTimer), timer_count, NULL)) goto fail;
	}
	return TRUE;

fail:
	pifTaskManager_Clear();
	return FALSE;
}

void pifTaskManager_Clear()
{
	pifObjArray_Clear(&s_timers);
	pifObjArray_Clear(&s_tasks);
}

PifTask *pifTaskManager_Add(PifId id, PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void *p_client, BOOL start)
{
	if (!evt_loop) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
	}

	if (!pifTask_CheckParam(&mode, period)) return NULL;

	PifObjArrayIterator it = pifObjArray_Add(&s_tasks);
	if (!it) return NULL;

	PifTask *p_owner = (PifTask *)it->data;
	pifTask_Init(p_owner, id);

	if (!pifTask_SetParam(p_owner, mode, period)) goto fail;

    p_owner->__evt_loop = evt_loop;
    p_owner->_p_client = p_client;
    p_owner->pause = (mode != TM_EXTERNAL) ? !start : TRUE;
    if (!s_it_current) s_it_current = pifObjArray_Begin(&s_tasks);
    return p_owner;

fail:
	if (p_owner) {
		pifObjArray_Remove(&s_tasks, p_owner);
	}
	return NULL;
}

void pifTaskManager_Remove(PifTask *p_task)
{
	if (s_it_current && p_task == (PifTask *)s_it_current->data) s_it_current = NULL;

	pifObjArray_Remove(&s_tasks, p_task);

	if (!pifObjArray_Count(&s_tasks)) s_it_current = NULL;
	else if (!s_it_current) s_it_current = pifObjArray_Begin(&s_tasks);
}

int pifTaskManager_Count()
{
	return pifObjArray_Count(&s_tasks);
}

PIF_INLINE PifTask *pifTaskManager_CurrentTask()
{
	return s_current_task;
}

PifTaskTimer *pifTaskManager_AddTimer(PifEvtTaskTimer evt_timer, void *p_client)
{
	PifObjArrayIterator it = pifObjArray_Add(&s_timers);
	if (!it) return NULL;

	PifTaskTimer *p_timer = (PifTaskTimer *)it->data;
	p_timer->_p_evt_timer = evt_timer;
	p_timer->_p_client = p_client;
	return p_timer;
}

void pifTaskManager_RemoveTimer(PifTaskTimer *p_timer)
{
	if (p_timer) {
		pifObjArray_Remove(&s_timers, p_timer);
	}
}

void pifTaskManager_SetIdle(PifEvtTaskIdle evt_idle)
{
	pif_evt_task_idle = evt_idle;
}

void pifTaskManager_Loop()
{
	PifTask *p_owner;
	PifTask *p_select = NULL;
	PifObjArrayIterator it_timer, it_timer_next;
	PifTaskTimer *p_timer;
	int i, n, t = 0, count = pifObjArray_Count(&s_tasks);
	uint32_t diff;
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	it_timer = pifObjArray_Begin(&s_timers);
	for (i = 0; i < pifObjArray_Count(&s_timers); i++) {
		p_timer = (PifTaskTimer *)it_timer->data;
		it_timer_next = pifObjArray_Next(it_timer);
		if (p_timer->_p_evt_timer) (*p_timer->_p_evt_timer)(p_timer->_p_client);
		it_timer = it_timer_next;
	}

	if (!s_it_current) {
		if (!count) {
			_processingIdle();
			_updateCpuLoad();
			return;
		}
		s_it_current = pifObjArray_Begin(&s_tasks);
	}

	s_loop_count += count;
	if (s_task_cutin) {
		_processingTrigger(s_task_cutin);
		p_select = s_task_cutin;
		s_task_cutin = NULL;
		trigger = TRUE;
		i = 1;
	}
	else {
		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

			if (p_owner->__trigger) {
				if (p_owner->__trigger_delay) {
					diff = pif_timer1us - p_owner->__trigger_time;
					if (diff >= p_owner->__trigger_delay) p_owner->__trigger_delay = 0;
				}
				if (!p_owner->__trigger_delay) {
					p_owner->__trigger = FALSE;
					_processingTrigger(p_owner);
					p_select = p_owner;
					trigger = TRUE;
				}
			}
			if (!p_select && !p_owner->pause && p_owner->__processing) {
				p_select = (*p_owner->__processing)(p_owner);
			}

			s_it_current = pifObjArray_Next(s_it_current);
			if (!s_it_current) {
				s_it_current = pifObjArray_Begin(&s_tasks);
				_checkLoopTime();
			}
		}
	}

	if (p_select) {
	    _processingTask(p_select, trigger);
	}
	else {
		_processingIdle();
	}
	_updateCpuLoad();
	s_pass_count += i - t;
}

void pifTaskManager_Yield()
{
	PifTask *p_owner;
	PifTask *p_select = NULL;
	int i, k, n, t = 0, count = pifObjArray_Count(&s_tasks);
	uint32_t diff;
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	s_loop_count += count;
	if (s_task_cutin && !s_task_cutin->_running) {
		_processingTrigger(s_task_cutin);
		p_select = s_task_cutin;
		s_task_cutin = NULL;
		trigger = TRUE;
		i = 1;
	}
	else {
		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

			if (p_owner->_running) goto next;
			if (s_task_stack_ptr) {
				for (k = 0; k < s_task_stack_ptr; k++) {
					if (s_task_stack[k]->disallow_yield_id && s_task_stack[k]->disallow_yield_id == p_owner->disallow_yield_id) break;
				}
				if (k < s_task_stack_ptr) goto next;
			}

			if (p_owner->__trigger) {
				if (p_owner->__trigger_delay) {
					diff = pif_timer1us - p_owner->__trigger_time;
					if (diff >= p_owner->__trigger_delay) p_owner->__trigger_delay = 0;
				}
				if (!p_owner->__trigger_delay) {
					p_owner->__trigger = FALSE;
					_processingTrigger(p_owner);
					p_select = p_owner;
					trigger = TRUE;
				}
			}
			if (!p_select && !p_owner->pause && p_owner->__processing) {
				p_select = (*p_owner->__processing)(p_owner);
			}

next:
			s_it_current = pifObjArray_Next(s_it_current);
			if (!s_it_current) {
				s_it_current = pifObjArray_Begin(&s_tasks);
				if (s_task_stack_ptr) _checkLoopTime();
			}
		}
	}

	if (p_select) {
	    _processingTask(p_select, trigger && s_task_stack_ptr);
	}
	_updateCpuLoad();
	s_pass_count += i - t;
}

void pifTaskManager_YieldMs(uint32_t time)
{
    uint32_t start;

    if (!time) return;

    start = pif_cumulative_timer1ms;
    do {
		pifTaskManager_Yield();
    } while (pif_cumulative_timer1ms - start <= time);
}

void pifTaskManager_YieldUs(uint32_t time)
{
    uint32_t start;

    if (!time) return;

	start = (*pif_act_timer1us)();
	do {
		pifTaskManager_Yield();
	} while ((*pif_act_timer1us)() - start <= time);
}

void pifTaskManager_YieldAbort(PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    if (!p_check_abort) return;

    while (1) {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
    }
}

void pifTaskManager_YieldAbortMs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    uint32_t start;

    if (!time) return;
    if (!p_check_abort) return;

    start = pif_cumulative_timer1ms;
    do {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
    } while (pif_cumulative_timer1ms - start <= time);
}

void pifTaskManager_YieldAbortUs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    uint32_t start;

    if (!time) return;
    if (!p_check_abort) return;

	start = (*pif_act_timer1us)();
	do {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
	} while ((*pif_act_timer1us)() - start <= time);
}

void pifTaskManager_AllTask(void (*callback)(PifTask *p_task))
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
	 	(*callback)((PifTask *)it->data);
		it = pifObjArray_Next(it);
	}
}

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

void pifTaskManager_Print()
{
	PifObjArrayIterator it;
	char *mode;
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t value;
#endif

   	pifLog_Printf(LT_NONE, "Task count: %d\n", pifObjArray_Count(&s_tasks));
	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		PifTask *p_owner = (PifTask *)it->data;
		if (p_owner->name) {
			pifLog_Printf(LT_NONE, "  %s", p_owner->name);
		}
		else {
			pifLog_Print(LT_NONE, "  ---");
		}
		switch (p_owner->_mode) {
			case TM_PERIOD: mode = "Period"; break;
			case TM_EXTERNAL: mode = "External"; break;
	        default: mode = "---"; break;
		}
		if (p_owner->_default_period < 1000) {
			pifLog_Printf(LT_NONE, " (%u): %s-%luus Pause=%d\n", p_owner->_id, mode, p_owner->_default_period, p_owner->pause);
		}
		else {
			pifLog_Printf(LT_NONE, " (%u): %s-%1fms Pause=%d\n", p_owner->_id, mode, p_owner->_default_period / 1000.0L, p_owner->pause);
		}
#ifdef PIF_USE_TASK_STATISTICS
		value = p_owner->__sum_execution_time[0] + p_owner->__sum_execution_time[1];
		pifLog_Printf(LT_NONE, "    Proc: M=%ldus A=%luus T=%lums\n", p_owner->_max_execution_time,
				(p_owner->__execution_count ? value / p_owner->__execution_count : 0), value / 1000);

		value = p_owner->__total_delta_time[0] + p_owner->__total_delta_time[1];
		if (value) {
			pifLog_Printf(LT_NONE, "    Delta: %luus\n", value / p_owner->__execution_count);
		}

		value = p_owner->__total_trigger_delay[0] + p_owner->__total_trigger_delay[1];
		if (value) {
			pifLog_Printf(LT_NONE, "    Trigger: M=%luus A=%luus\n", p_owner->_max_trigger_delay,
					(p_owner->__trigger_count ? value / p_owner->__trigger_count : 0));
		}
		pifLog_Print(LT_NONE, "\n");
#endif
		it = pifObjArray_Next(it);
	}

	_updateCpuLoad();
	pifLog_Printf(LT_NONE, "Task Load: %u%%\n", pif_performance._task_load);
}

#endif	// PIF_NO_LOG
