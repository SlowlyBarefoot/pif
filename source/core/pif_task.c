#include "core/pif_obj_array.h"
#include "core/pif_log.h"
#include "core/pif_task.h"

#include <string.h>


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)
#define PIF_TASK_STACK_SIZE		5


#ifdef PIF_DEBUG

PifActTaskSignal pif_act_task_signal = NULL;

#endif

static PifObjArray s_tasks;
static PifObjArrayIterator s_it_current;
static PifTask* s_task_stack[PIF_TASK_STACK_SIZE];
static int s_task_stack_ptr = 0;
static PifTask* s_task_cutin = NULL;
static PifTask *s_current_task = NULL;

static uint32_t s_table_number;
static uint32_t s_table[PIF_TASK_TABLE_SIZE];
static uint8_t s_number = 0;

static uint32_t s_loop_count = 0UL, s_pass_count = 0UL;


static int _setTable(uint16_t period, PifTaskMode* p_mode)
{
	uint32_t gap, index, bit;
	static int base = 0;
	int i, count, num = -1;

	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
		if (!(s_table_number & (1 << i))) {
			num = i;
			break;
		}
	}
	if (num == -1) {
		pif_error = E_OVERFLOW_BUFFER;
		return -1;
	}
	bit = 1 << num;
	s_table_number |= bit;

	count = PIF_TASK_TABLE_SIZE * period;
	gap = 10000L * PIF_TASK_TABLE_SIZE / count;
	if (gap > 100) {
		index = 100 * base;
		for (i = 0; i < count / 100; i++) {
			s_table[(index / 100) & PIF_TASK_TABLE_MASK] |= bit;
			index += gap;
		}
		base++;
	}
	else {
		*p_mode = TM_ALWAYS;
	}
	return num;
}

static void _resetTable(int number)
{
	int i;
	uint32_t mask;

	mask = ~((uint32_t)1 << number);
	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
		s_table[i] &= mask;
	}
	s_table_number &= mask;
}

static PifTask* _processingAlways(PifTask* p_owner)
{
	if (p_owner->__delay_ms) {
		p_owner->_delta_time = pif_cumulative_timer1ms - p_owner->__pretime;
		if (p_owner->_delta_time >= p_owner->__delay_ms) {
			p_owner->__delay_ms = 0;
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

static PifTask* _processingRatio(PifTask* p_owner)
{
#ifdef PIF_DEBUG
	uint32_t time;
	static uint32_t pretime;
#endif

	if (p_owner->__delay_ms) {
		p_owner->_delta_time = pif_cumulative_timer1ms - p_owner->__pretime;
		if (p_owner->_delta_time >= p_owner->__delay_ms) {
			p_owner->__delay_ms = 0;
		}
	}
	else if (s_table[s_number] & (1 << p_owner->__table_number)) {
#ifdef PIF_DEBUG
		time = pif_timer1sec;
		if (time != pretime) {
			p_owner->__ratio_period = 1000000.0 / p_owner->__ratio_count;
			p_owner->__ratio_count = 0;
			pretime = time;
		}
		p_owner->__ratio_count++;
#endif
		return p_owner;
	}
	return NULL;
}

static BOOL _checkParam(PifTaskMode* p_mode, uint32_t period)
{
	switch (*p_mode) {
    case TM_RATIO:
    	if (!period || period > 100) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	else if (period == 100) {
    		*p_mode = TM_ALWAYS;
    	}
    	break;

    case TM_PERIOD:
    case TM_IDLE:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	break;
    	
	case TM_EXTERNAL_CUTIN:
		if (s_task_cutin) return FALSE;
    	break;

	case TM_TIMER:
	case TM_ALWAYS:
	case TM_EXTERNAL_ORDER:
		break;

    default:
		pif_error = E_INVALID_PARAM;
	    return FALSE;
    }
	return TRUE;
}

static BOOL _setParam(PifTask* p_owner, PifTaskMode mode, uint32_t period)
{
	int num = -1;

	if (mode == TM_RATIO) {
    	num = _setTable(period, &mode);
    	if (num == -1) return FALSE;
	}

	p_owner->_unit = 1000;

    switch (mode) {
    case TM_RATIO:
    	p_owner->__table_number = num;
    	p_owner->__processing = _processingRatio;
    	break;

    case TM_ALWAYS:
    	period = 100;
    	p_owner->__processing = _processingAlways;
    	break;

    case TM_PERIOD:
    case TM_IDLE:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriod;
		p_owner->_unit = 1;
    	break;

	case TM_EXTERNAL_CUTIN:
		s_task_cutin = p_owner;
    	period = 0;
    	p_owner->__processing = NULL;
		break;

	case TM_TIMER:
    case TM_EXTERNAL_ORDER:
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

static void _processingTrigger(PifTask* p_owner)
{
	uint32_t current;

	switch (p_owner->_mode) {
	case TM_PERIOD:
	case TM_EXTERNAL_CUTIN:
	case TM_EXTERNAL_ORDER:
		current = (*pif_act_timer1us)();
		p_owner->_delta_time = current - p_owner->__pretime;
		p_owner->__current_time = current;
		break;

	default:
		break;
	}
}

static void _processingTask(PifTask* p_owner, BOOL trigger)
{
	uint32_t period;
#ifdef PIF_USE_TASK_STATISTICS
	uint16_t trigger_delay;
	uint32_t start_time;
	int32_t execute_time;
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
    s_task_stack[s_task_stack_ptr] = p_owner;
	s_task_stack_ptr++;
	p_owner->_running = TRUE;
	p_owner->_last_execute_time = (*pif_act_timer1us)();
#ifdef PIF_USE_TASK_STATISTICS
	start_time = p_owner->_last_execute_time;
	period = (*p_owner->__evt_loop)(p_owner);
	execute_time = (*pif_act_timer1us)() - start_time;
	p_owner->_total_execution_time += execute_time;
	if (execute_time > p_owner->_max_execution_time) p_owner->_max_execution_time = execute_time;
	p_owner->__total_delta_time[p_owner->__execute_index] += p_owner->_delta_time * p_owner->_unit;
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
#else
	period = (*p_owner->__evt_loop)(p_owner);
#endif
	p_owner->_running = FALSE;
	s_task_stack_ptr--;
	s_task_stack[s_task_stack_ptr] = NULL;

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(FALSE);
#endif

	if (p_owner->_mode == TM_PERIOD) {
		if (period > 0) {
			p_owner->__period = period;
		}
		else {
			p_owner->__period = p_owner->_default_period;
		}
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


void pifTask_Init(PifTask* p_owner)
{
    pif_id++;
    p_owner->_id = pif_id;
}

BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint32_t period)
{
	if (mode == p_owner->_mode) return TRUE;

	if (p_owner->_mode == TM_EXTERNAL_CUTIN) s_task_cutin = NULL;

	if (!_checkParam(&mode, period)) return FALSE;

	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		_resetTable(p_owner->__table_number);
		break;

	default:
		break;
	}

	if (!_setParam(p_owner, mode, period)) return FALSE;

    return TRUE;
}

BOOL pifTask_ChangePeriod(PifTask* p_owner, uint32_t period)
{
	switch (p_owner->_mode & TM_MAIN_MASK) {
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

BOOL pifTask_SetTrigger(PifTask* p_owner)
{
	if (p_owner) {
		if (p_owner->__trigger) return TRUE;

		p_owner->__trigger_time = (*pif_act_timer1us)();
		p_owner->__trigger = TRUE;
		return TRUE;
	}
	return FALSE;
}

void pifTask_DelayMs(PifTask* p_owner, uint16_t delay)
{
	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		p_owner->__delay_ms = delay;
    	p_owner->__pretime = pif_cumulative_timer1ms;
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


BOOL pifTaskManager_Init(int max_count)
{
	if (!max_count) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	if (!pifObjArray_Init(&s_tasks, sizeof(PifTask), max_count, NULL)) return FALSE;
	s_it_current = NULL;

	s_table_number = 0L;
	memset(s_table, 0, sizeof(s_table));
	return TRUE;
}

void pifTaskManager_Clear()
{
	pifObjArray_Clear(&s_tasks);
}

PifTask* pifTaskManager_Add(PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void* p_client, BOOL start)
{
	if (!evt_loop) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
	}

	if (!_checkParam(&mode, period)) return NULL;

	PifObjArrayIterator it = pifObjArray_Add(&s_tasks);
	if (!it) return NULL;

	PifTask* p_owner = (PifTask*)it->data;
	pifTask_Init(p_owner);

	if (!_setParam(p_owner, mode, period)) goto fail;

    p_owner->__evt_loop = evt_loop;
    p_owner->_p_client = p_client;
    p_owner->pause = ((mode & TM_MAIN_MASK) != TM_EXTERNAL) ? !start : TRUE;
    if (!s_it_current) s_it_current = pifObjArray_Begin(&s_tasks);
    return p_owner;

fail:
	if (p_owner) {
		pifObjArray_Remove(&s_tasks, p_owner);
	}
	return NULL;
}

void pifTaskManager_Remove(PifTask* p_task)
{
	if (p_task == (PifTask*)s_it_current->data) s_it_current = NULL;

	switch (p_task->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		_resetTable(p_task->__table_number);
		break;

	default:
		break;
	}
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

void pifTaskManager_Loop()
{
	PifTask* p_owner;
	PifTask* p_select = NULL;
	PifObjArrayIterator it_idle = NULL;
	int i, n, t = 0, count = pifObjArray_Count(&s_tasks);
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	if (!s_it_current) {
		if (!count) return;
		s_it_current = pifObjArray_Begin(&s_tasks);
	}

	s_loop_count += count;
	if (s_task_cutin && s_task_cutin->__trigger) {
		s_task_cutin->__trigger = FALSE;
		_processingTrigger(s_task_cutin);
		p_select = s_task_cutin;
		trigger = TRUE;
		i = 1;
	}
	else {
		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask*)s_it_current->data;

			if (p_owner->__trigger) {
				p_owner->__trigger = FALSE;
				_processingTrigger(p_owner);
				p_select = p_owner;
				trigger = TRUE;
			}
			else if (!p_owner->pause) {
				if (p_owner->_mode == TM_TIMER) {
					(*p_owner->__evt_loop)(p_owner);
					t++;
				}
				else if (p_owner->__processing) {
					if (p_owner->_mode & TM_IDLE) {
						if (!it_idle) {
							if ((*p_owner->__processing)(p_owner)) {
								it_idle = s_it_current;
								n = i;
							}
						}
					}
					else {
						p_select = (*p_owner->__processing)(p_owner);
					}
				}
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
	else if (it_idle) {
		p_select = (PifTask*)it_idle->data;
		i = n;
		it_idle = pifObjArray_Next(it_idle);
		if (!it_idle) {
			s_it_current = pifObjArray_Begin(&s_tasks);
		}
		else {
			s_it_current = it_idle;
		}
	    _processingTask(p_select, FALSE);
	}
	s_pass_count += i - t;
}

void pifTaskManager_Yield()
{
	PifTask* p_owner;
	PifTask* p_select = NULL;
	PifObjArrayIterator it_idle = NULL;
	int i, k, n, t = 0, count = pifObjArray_Count(&s_tasks);
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	if (!s_it_current) {
		if (!count) return;
		s_it_current = pifObjArray_Begin(&s_tasks);
	}

	s_loop_count += count;
	if (s_task_cutin && s_task_cutin->__trigger && !s_task_cutin->_running) {
		s_task_cutin->__trigger = FALSE;
		_processingTrigger(s_task_cutin);
		p_select = s_task_cutin;
		trigger = TRUE;
		i = 1;
	}
	else {
		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask*)s_it_current->data;

			if (p_owner->_running) goto next;
			if (s_task_stack_ptr) {
				for (k = 0; k < s_task_stack_ptr; k++) {
					if (s_task_stack[k]->disallow_yield_id && s_task_stack[k]->disallow_yield_id == p_owner->disallow_yield_id) break;
				}
				if (k < s_task_stack_ptr) goto next;
			}

			if (p_owner->__trigger) {
				p_owner->__trigger = FALSE;
				_processingTrigger(p_owner);
				p_select = p_owner;
				trigger = TRUE;
			}
			else if (!p_owner->pause) {
				if (p_owner->_mode == TM_TIMER) {
					(*p_owner->__evt_loop)(p_owner);
					t++;
				}
				else if (p_owner->__processing) {
					if (p_owner->_mode & TM_IDLE) {
						if (!it_idle) {
							if ((*p_owner->__processing)(p_owner)) {
								it_idle = s_it_current;
								n = i;
							}
						}
					}
					else {
						p_select = (*p_owner->__processing)(p_owner);
					}
				}
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
	else if (it_idle) {
		p_select = (PifTask*)it_idle->data;
		i = n;
		it_idle = pifObjArray_Next(it_idle);
		if (!it_idle) {
			s_it_current = pifObjArray_Begin(&s_tasks);
		}
		else {
			s_it_current = it_idle;
		}
	    _processingTask(p_select, FALSE);
	}
	s_pass_count += i - t;
}

void pifTaskManager_YieldMs(int32_t time)
{
    uint32_t start;

    if (!time) return;

    start = pif_cumulative_timer1ms;
    do {
		pifTaskManager_Yield();
    } while ((int32_t)(pif_cumulative_timer1ms - start) <= time);
}

void pifTaskManager_YieldUs(int32_t time)
{
    uint32_t start;

    if (!time) return;

	start = (*pif_act_timer1us)();
	do {
		pifTaskManager_Yield();
	} while ((int32_t)((*pif_act_timer1us)() - start) <= time);
}

void pifTaskManager_YieldAbort(PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    if (!p_check_abort) return;

    while (1) {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
    }
}

void pifTaskManager_YieldAbortMs(int32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    uint32_t start;

    if (!time) return;
    if (!p_check_abort) return;

    start = pif_cumulative_timer1ms;
    do {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
    } while ((int32_t)(pif_cumulative_timer1ms - start) <= time);
}

void pifTaskManager_YieldAbortUs(int32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer)
{
    uint32_t start;

    if (!time) return;
    if (!p_check_abort) return;

	start = (*pif_act_timer1us)();
	do {
		pifTaskManager_Yield();
		if ((*p_check_abort)(p_issuer)) break;
	} while ((int32_t)((*pif_act_timer1us)() - start) <= time);
}

void pifTaskManager_AllTask(void (*callback)(PifTask *p_task))
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
	 	(*callback)((PifTask*)it->data);
		it = pifObjArray_Next(it);
	}
}

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

void pifTaskManager_Print()
{
	PifObjArrayIterator it;
	char* mode;
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t value;
#endif

   	pifLog_Printf(LT_NONE, "Task count: %d\n", pifObjArray_Count(&s_tasks));
	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		PifTask* p_owner = (PifTask*)it->data;
		if (p_owner->name) {
			pifLog_Printf(LT_NONE, "  %s", p_owner->name);
		}
		else {
			pifLog_Print(LT_NONE, "  ---");
		}
		switch (p_owner->_mode) {
			case TM_RATIO: mode = "Ratio"; break;
			case TM_ALWAYS: mode = "Always"; break;
			case TM_PERIOD: mode = "Period"; break;
			case TM_EXTERNAL_CUTIN: mode = "ExtCutin"; break;
			case TM_EXTERNAL_ORDER: mode = "ExtOrder"; break;
			case TM_TIMER: mode = "Timer"; break;
			case TM_IDLE: mode = "Idle"; break;
	        default: mode = "---"; break;
		}
		pifLog_Printf(LT_NONE, " (%u): %s-%lu\n", p_owner->_id, mode, p_owner->_default_period);
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
}

#ifdef PIF_DEBUG

void pifTaskManager_PrintRatioTable()
{
	int i;

	pifLog_Printf(LT_INFO, "Task Ratio Table Size=%d", PIF_TASK_TABLE_SIZE);
	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
		pifLog_Printf(LT_NONE, "\n %3d : %8lX", i, s_table[i]);
	}
}

#endif	// PIF_DEBUG

#endif	// PIF_NO_LOG
