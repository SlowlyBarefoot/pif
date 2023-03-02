#include "core/pif_list.h"
#include "core/pif_log.h"
#include "core/pif_task.h"

#include <string.h>


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)
#define PIF_TASK_STACK_SIZE		5


#ifdef __PIF_DEBUG__

PifActTaskSignal pif_act_task_signal = NULL;

#endif

static PifFixList s_tasks;
static PifFixListIterator s_it_current;
static PifTask* s_task_stack[PIF_TASK_STACK_SIZE];
static int s_task_stack_ptr = 0;
static PifTask* s_task_cutin = NULL;

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
	uint32_t gap;

	if (p_owner->__delay_ms) {
		gap = pif_cumulative_timer1ms - p_owner->__pretime;
		if (gap >= p_owner->__delay_ms) {
			p_owner->__delay_ms = 0;
		}
		return NULL;
	}
	return p_owner;
}

static PifTask* _processingPeriodUs(PifTask* p_owner)
{
	uint32_t current, gap;

	current = (*pif_act_timer1us)();
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__pretime = current - (gap - p_owner->_period);
		return p_owner;
	}
	return NULL;
}

static PifTask* _processingPeriodMs(PifTask* p_owner)
{
	uint32_t current, gap;

	current = pif_cumulative_timer1ms;
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__pretime = current;
		return p_owner;
	}
	return NULL;
}

static PifTask* _processingRatio(PifTask* p_owner)
{
	uint32_t gap;
#ifdef __PIF_DEBUG__
	uint32_t time;
	static uint32_t pretime;
#endif

	if (p_owner->__delay_ms) {
		gap = pif_cumulative_timer1ms - p_owner->__pretime;
		if (gap >= p_owner->__delay_ms) {
			p_owner->__delay_ms = 0;
		}
	}
	else if (s_table[s_number] & (1 << p_owner->__table_number)) {
#ifdef __PIF_DEBUG__
		time = pif_timer1sec;
		if (time != pretime) {
			p_owner->__period = 1000000.0 / p_owner->__count;
			p_owner->__count = 0;
			pretime = time;
		}
		p_owner->__count++;
#endif
		return p_owner;
	}
	return NULL;
}

static BOOL _checkParam(PifTaskMode* p_mode, uint16_t period)
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

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    case TM_IDLE_MS:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}

    	if (!pif_act_timer1us) {
    		pif_error = E_CANNOT_USE;
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

static BOOL _setParam(PifTask* p_owner, PifTaskMode mode, uint16_t period)
{
	int num = -1;

	if (mode == TM_RATIO) {
    	num = _setTable(period, &mode);
    	if (num == -1) return FALSE;
	}

    switch (mode) {
    case TM_RATIO:
    	p_owner->__table_number = num;
    	p_owner->__processing = _processingRatio;
    	break;

    case TM_ALWAYS:
    	period = 100;
    	p_owner->__processing = _processingAlways;
    	break;

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    case TM_IDLE_MS:
    	p_owner->__pretime = pif_cumulative_timer1ms;
    	p_owner->__processing = _processingPeriodMs;
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriodUs;
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
    p_owner->_period = period;
	return TRUE;
}

static BOOL _processingTask(PifTask* p_owner)
{
	uint16_t period;
	uint32_t start_time, execute_time;

	if (s_task_stack_ptr >= PIF_TASK_STACK_SIZE) return FALSE;

#ifdef __PIF_DEBUG__
    if (pif_act_task_signal) (*pif_act_task_signal)(TRUE);
#endif

    s_task_stack[s_task_stack_ptr] = p_owner;
	s_task_stack_ptr++;
	p_owner->_running = TRUE;
	if (pif_act_timer1us) {
		start_time = (*pif_act_timer1us)();
		period = (*p_owner->__evt_loop)(p_owner);
		execute_time = (*pif_act_timer1us)() - start_time;
		p_owner->_execution_count++;
		if (execute_time > p_owner->_max_execution_time) p_owner->_max_execution_time = execute_time;
		p_owner->_total_execution_time += execute_time;
	}
	else {
		period = (*p_owner->__evt_loop)(p_owner);
	}
	p_owner->_running = FALSE;
	s_task_stack_ptr--;
	s_task_stack[s_task_stack_ptr] = NULL;

#ifdef __PIF_DEBUG__
    if (pif_act_task_signal) (*pif_act_task_signal)(FALSE);
#endif

	switch (p_owner->_mode) {
	case TM_CHANGE_MS:
		if (period > 0) {
			p_owner->_period = period;
			p_owner->__pretime = pif_cumulative_timer1ms;
		}
		break;

	case TM_CHANGE_US:
		if (period > 0) {
			p_owner->_period = period;
			p_owner->__pretime = (*pif_act_timer1us)();
		}
		break;

	default:
		break;
	}
	return TRUE;
}

static void _checkLoopTime()
{
	static uint8_t timer_10ms = 0;
#if defined(__PIF_DEBUG__) || !defined(__PIF_NO_LOG__)
	uint32_t value;
#endif
#ifdef __PIF_DEBUG__
	static uint32_t pretime = 0UL;
	static uint32_t max_loop = 0UL;

	if (pif_act_timer1us) {
		value = (*pif_act_timer1us)() - pretime;
		if (value > pif_performance.__max_loop_time1us) {
			pif_performance.__max_loop_time1us = value;
		}
		pretime = (*pif_act_timer1us)();
	}
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
#ifdef __PIF_DEBUG__
		if (pif_performance.__max_loop_time1us > max_loop) max_loop = pif_performance.__max_loop_time1us;
	#ifndef __PIF_NO_LOG__
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%uns(%lur/s) MLT=%luus", value, pif_performance._count, pif_performance.__max_loop_time1us);
		}
	#endif
		pif_performance.__max_loop_time1us = 0UL;
#else
	#ifndef __PIF_NO_LOG__
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%uns(%lur/s)", value, pif_performance._count);
        }
	#endif
#endif
    	pif_performance._count = 0;
	}

#ifdef __PIF_DEBUG__
	if (pif_performance.__state & 4) {		// 1min
	#ifndef __PIF_NO_LOG__
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

BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint16_t period)
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

BOOL pifTask_ChangePeriod(PifTask* p_owner, uint16_t period)
{
	switch (p_owner->_mode) {
	case TM_PERIOD_MS:
	case TM_PERIOD_US:
	case TM_IDLE_MS:
		p_owner->_period = period;
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
		if (pif_act_timer1us) p_owner->__trigger_time = (*pif_act_timer1us)();
		else p_owner->__trigger_time = pif_cumulative_timer1ms;
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

uint32_t pifTask_GetDeltaTime(PifTask* p_owner, BOOL reset)
{
	uint32_t currect, delta;

    if (!pif_act_timer1us) {
		currect = pif_cumulative_timer1ms * 1000;
	}
	else {
		currect = (*pif_act_timer1us)();
	}
	delta = currect - p_owner->__last_execute_time;
	if (reset) {
		if (p_owner->__last_execute_time) {
			p_owner->_total_period_time += delta;
			p_owner->_period_count++;
		}
		p_owner->__last_execute_time = currect;
	}
	return delta;
}


BOOL pifTaskManager_Init(int max_count)
{
	if (!pifFixList_Init(&s_tasks, sizeof(PifTask), max_count)) return FALSE;
	s_it_current = NULL;

	s_table_number = 0L;
	memset(s_table, 0, sizeof(s_table));
	return TRUE;
}

void pifTaskManager_Clear()
{
	pifFixList_Clear(&s_tasks, NULL);
}

PifTask* pifTaskManager_Add(PifTaskMode mode, uint16_t period, PifEvtTaskLoop evt_loop, void* p_client, BOOL start)
{
	if (!evt_loop) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
	}

	if (!_checkParam(&mode, period)) return NULL;

	PifTask* p_owner = (PifTask*)pifFixList_AddFirst(&s_tasks);
	if (!p_owner) return NULL;

	pifTask_Init(p_owner);

	if (!_setParam(p_owner, mode, period)) goto fail;

    p_owner->__evt_loop = evt_loop;
    p_owner->_p_client = p_client;
    p_owner->pause = (mode != TM_EXTERNAL_ORDER && mode != TM_EXTERNAL_CUTIN) ? !start : TRUE;
    if (!s_it_current) s_it_current = pifFixList_Begin(&s_tasks);
    return p_owner;

fail:
	if (p_owner) {
		pifFixList_Remove(&s_tasks, p_owner);
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
	pifFixList_Remove(&s_tasks, p_task);

	if (!pifFixList_Count(&s_tasks)) s_it_current = NULL;
	else if (!s_it_current) s_it_current = pifFixList_Begin(&s_tasks);
}

int pifTaskManager_Count()
{
	return pifFixList_Count(&s_tasks);
}

void pifTaskManager_Loop()
{
	PifTask* p_owner;
	PifTask* p_select = NULL;
	PifTask* p_idle = NULL;
	PifFixListIterator it_idle = NULL;
	int i, n, t = 0, count = pifFixList_Count(&s_tasks);
	BOOL trigger = FALSE;

	if (pif_act_timer1us) pif_timer1us = (*pif_act_timer1us)();

	if (!s_it_current) {
		if (!count) return;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	s_loop_count += count;
	if (s_task_cutin && s_task_cutin->__trigger) {
		s_task_cutin->__trigger = FALSE;
		p_select = s_task_cutin;
		trigger = TRUE;
		i = 1;
	}
	else {
		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask*)s_it_current->data;

			if (p_owner->__trigger) {
				p_owner->__trigger = FALSE;
				p_select = p_owner;
				trigger = TRUE;
			}
			else if (!p_owner->pause) {
				if (p_owner->_mode == TM_TIMER) {
					(*p_owner->__evt_loop)(p_owner);
					t++;
				}
				else if (p_owner->__processing) {
					if (p_owner->_mode == TM_IDLE_MS) {
						if (!p_idle) {
							p_idle = (*p_owner->__processing)(p_owner);
							if (p_idle) {
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

			s_it_current = pifFixList_Next(s_it_current);
			if (!s_it_current) {
				s_it_current = pifFixList_Begin(&s_tasks);
				_checkLoopTime();
			}
		}
	}

	if (p_select) {
		if (trigger) {
			if (pif_act_timer1us) p_select->_trigger_delay = (*pif_act_timer1us)() - p_select->__trigger_time;
			else p_select->_trigger_delay = pif_cumulative_timer1ms - p_select->__trigger_time;
			if (p_select->_trigger_delay > p_select->_max_trigger_delay) p_select->_max_trigger_delay = p_select->_trigger_delay;
			p_select->_total_trigger_delay += p_select->_trigger_delay;
		}
	    _processingTask(p_select);
	}
	else if (p_idle) {
		i = n;
		it_idle = pifFixList_Next(it_idle);
		if (!it_idle) {
			s_it_current = pifFixList_Begin(&s_tasks);
		}
		else {
			s_it_current = it_idle;
		}
	    _processingTask(p_idle);
	}
	s_pass_count += i - t;
}

BOOL pifTaskManager_Yield()
{
	PifTask* p_owner;
	PifTask* p_select = NULL;
	PifTask* p_idle = NULL;
	PifFixListIterator it_idle = NULL;
	int i, k, n, t = 0, count = pifFixList_Count(&s_tasks);
	BOOL trigger = FALSE;
	BOOL rtn = TRUE;

	if (pif_act_timer1us) pif_timer1us = (*pif_act_timer1us)();

	if (!s_it_current) {
		if (!count) return FALSE;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	s_loop_count += count;
	if (s_task_cutin && s_task_cutin->__trigger && !s_task_cutin->_running) {
		s_task_cutin->__trigger = FALSE;
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
				p_select = p_owner;
				trigger = TRUE;
			}
			else if (!p_owner->pause) {
				if (p_owner->_mode == TM_TIMER) {
					(*p_owner->__evt_loop)(p_owner);
					t++;
				}
				else if (p_owner->__processing) {
					if (p_owner->_mode == TM_IDLE_MS) {
						if (!p_idle) {
							p_idle = (*p_owner->__processing)(p_owner);
							if (p_idle) {
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
			s_it_current = pifFixList_Next(s_it_current);
			if (!s_it_current) {
				s_it_current = pifFixList_Begin(&s_tasks);
				if (s_task_stack_ptr) _checkLoopTime();
			}
		}
	}

	if (p_select) {
		if (trigger && s_task_stack_ptr) {
			if (pif_act_timer1us) p_select->_trigger_delay = (*pif_act_timer1us)() - p_select->__trigger_time;
			else p_select->_trigger_delay = pif_cumulative_timer1ms - p_select->__trigger_time;
			if (p_select->_trigger_delay > p_select->_max_trigger_delay) p_select->_max_trigger_delay = p_select->_trigger_delay;
			p_select->_total_trigger_delay += p_select->_trigger_delay;
		}
	    rtn = _processingTask(p_select);
	}
	else if (p_idle) {
		i = n;
		it_idle = pifFixList_Next(it_idle);
		if (!it_idle) {
			s_it_current = pifFixList_Begin(&s_tasks);
		}
		else {
			s_it_current = it_idle;
		}
	    rtn = _processingTask(p_idle);
	}
	s_pass_count += i - t;
    return rtn;
}

BOOL pifTaskManager_YieldMs(uint32_t time)
{
    uint32_t start;

    if (!time) return FALSE;

    start = pif_cumulative_timer1ms;
    do {
		if (!pifTaskManager_Yield()) return FALSE;
    } while (pif_cumulative_timer1ms - start <= time);
    return TRUE;
}

BOOL pifTaskManager_YieldUs(uint32_t time)
{
    uint32_t start;

    if (!time) return FALSE;

    if (!pif_act_timer1us) {
        start = pif_cumulative_timer1ms * 1000;
        do {
    		if (!pifTaskManager_Yield()) return FALSE;
		} while (pif_cumulative_timer1ms * 1000 - start <= time);
    }
    else {
    	start = (*pif_act_timer1us)();
		do {
			if (!pifTaskManager_Yield()) return FALSE;
		} while ((*pif_act_timer1us)() - start <= time);
    }
    return TRUE;
}

void pifTaskManager_Print()
{
	PifFixListIterator it;
	const char* mode[] = { "Ratio", "Always", "PeriodMs", "PeriodUs", "ChangeMs", "ChangeUs", "ExtCutin", "ExtOrder", "Timer", "IdleMs" };

   	pifLog_Printf(LT_NONE, "Task count: %d\n", pifFixList_Count(&s_tasks));
	it = pifFixList_Begin(&s_tasks);
	while (it) {
		PifTask* p_owner = (PifTask*)it->data;
		if (p_owner->name) {
			pifLog_Printf(LT_NONE, "  %s", p_owner->name);
		}
		else {
			pifLog_Print(LT_NONE, "  ---");
		}
		pifLog_Printf(LT_NONE, " (%d): %s-%d,  proc: M=%dus, A=%dus T=%dms", p_owner->_id, mode[p_owner->_mode], p_owner->_period,
				p_owner->_max_execution_time, p_owner->_total_execution_time / p_owner->_execution_count, p_owner->_total_execution_time / 1000);
		if (p_owner->_total_period_time) {
			pifLog_Printf(LT_NONE, ",  period: %dus", p_owner->_total_period_time / p_owner->_period_count);
		}
		if (p_owner->_total_trigger_delay) {
			pifLog_Printf(LT_NONE, ",  delay: M=%dus A=%dus", p_owner->_max_trigger_delay, p_owner->_total_trigger_delay / p_owner->_execution_count);
		}
		pifLog_Print(LT_NONE, "\n");
		it = pifFixList_Next(it);
	}
}

#ifdef __PIF_DEBUG__

void pifTaskManager_PrintRatioTable()
{
	int i;

	pifLog_Printf(LT_INFO, "Task Ratio Table Size=%d", PIF_TASK_TABLE_SIZE);
	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
		pifLog_Printf(LT_NONE, "\n %3d : %8lX", i, s_table[i]);
	}
}

#endif	// __PIF_DEBUG__