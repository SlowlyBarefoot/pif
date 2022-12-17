#include "core/pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
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
		p_owner->__pretime = current;
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
    	
    default:
    	break;
    }
	return TRUE;
}

static BOOL _setParam(PifTask* p_owner, PifTaskMode mode, uint16_t period)
{
	int num = -1;

    switch (mode) {
    case TM_RATIO:
    	num = _setTable(period, &mode);
    	if (num == -1) return FALSE;
    	if (mode == TM_ALWAYS) period = 100;
    	p_owner->__table_number = num;
    	p_owner->__processing = _processingRatio;
    	break;

    case TM_ALWAYS:
    	period = 100;
    	p_owner->__processing = _processingAlways;
    	break;

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    	p_owner->__pretime = pif_cumulative_timer1ms;
    	p_owner->__processing = _processingPeriodMs;
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriodUs;
    	break;

    case TM_NEED:
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

	if (s_task_stack_ptr >= PIF_TASK_STACK_SIZE) return FALSE;

#ifdef __PIF_DEBUG__
    if (pif_act_task_signal) (*pif_act_task_signal)(TRUE);
#endif

    s_task_stack[s_task_stack_ptr] = p_owner;
	s_task_stack_ptr++;
	p_owner->__running = TRUE;
	period = (*p_owner->__evt_loop)(p_owner);
	p_owner->__running = FALSE;
	s_task_stack_ptr--;
	s_task_stack[s_task_stack_ptr] = NULL;

#ifdef __PIF_DEBUG__
    if (pif_act_task_signal) (*pif_act_task_signal)(FALSE);
#endif

	switch (p_owner->_mode) {
	case TM_CHANGE_MS:
	case TM_CHANGE_US:
		if (period > 0) p_owner->_period = period;
		break;

	default:
		break;
	}
	return TRUE;
}

static void _checkLoopTime()
{
#if defined(__PIF_DEBUG__) || !defined(__PIF_NO_LOG__)
	uint32_t value;
#endif
#ifndef __PIF_NO_LOG__
	static uint8_t use_rate = 0;
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

	switch (pif_performance.__state) {
	case 1:
		pif_performance._use_rate = 100 - 100 * s_pass_count / s_loop_count;
		s_loop_count = 0UL;
		s_pass_count = 0UL;
#ifndef __PIF_NO_LOG__
		if (use_rate != pif_performance._use_rate) {
			use_rate = pif_performance._use_rate;
	    	pifLog_Printf(LT_INFO, "Use Rate: %u%%", use_rate);
		}
#endif

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
		pif_performance.__state = FALSE;
		break;

	case 2:
#ifdef __PIF_DEBUG__
	#ifndef __PIF_NO_LOG__
    	pifLog_Printf(LT_INFO, "MLT=%luus", max_loop);
	#endif
		max_loop = 0UL;
#endif
    	pif_performance._count = 0;
		pif_performance.__state = FALSE;
		break;
    }
}


void pifTask_Init(PifTask* p_owner)
{
    pif_id++;
    p_owner->_id = pif_id;
}

BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint16_t period)
{
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
		p_owner->_period = period;
		break;

	default:
		pif_error = E_CANNOT_USE;
		return FALSE;
	}
	return TRUE;
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
    p_owner->pause = (mode != TM_NEED) ? !start : TRUE;
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
	int i, count = pifFixList_Count(&s_tasks);

	if (!s_it_current) {
		if (!count) return;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	s_loop_count += count;
	for (i = 0; i < count && !p_select; i++) {
		p_owner = (PifTask*)s_it_current->data;

		if (p_owner->immediate) {
			p_owner->immediate = FALSE;
			p_select = p_owner;
		}
		else if (!p_owner->pause) {
			if (p_owner->__processing) {
				p_select = (*p_owner->__processing)(p_owner);
			}
		}

		s_it_current = pifFixList_Next(s_it_current);
		if (!s_it_current) {
			s_it_current = pifFixList_Begin(&s_tasks);
		}
	}
	s_pass_count += i;

	if (p_select) {
	    _processingTask(p_select);
	}

    _checkLoopTime();
}

BOOL pifTaskManager_Yield()
{
	PifTask* p_owner;
	PifTask* p_select = NULL;
	int i, k, count = pifFixList_Count(&s_tasks);
	BOOL rtn = TRUE;

	if (!s_it_current) {
		if (!count) return FALSE;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	s_loop_count += count;
	for (i = 0; i < count && !p_select; i++) {
		p_owner = (PifTask*)s_it_current->data;

		if (p_owner->__running) goto next;
		if (s_task_stack_ptr) {
			for (k = 0; k < s_task_stack_ptr; k++) {
				if (s_task_stack[k]->disallow_yield_id && s_task_stack[k]->disallow_yield_id == p_owner->disallow_yield_id) break;
			}
			if (k < s_task_stack_ptr) goto next;
		}

		if (p_owner->immediate) {
			p_owner->immediate = FALSE;
			p_select = p_owner;
		}
		else if (!p_owner->pause) {
			if (p_owner->__processing) {
				p_select = (*p_owner->__processing)(p_owner);
			}
		}

next:
		s_it_current = pifFixList_Next(s_it_current);
		if (!s_it_current) {
			s_it_current = pifFixList_Begin(&s_tasks);
		}
	}
	s_pass_count += i;

	if (p_select) {
	    rtn = _processingTask(p_select);
	}

    _checkLoopTime();
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

#if !defined(__PIF_NO_LOG__) && defined(__PIF_DEBUG__)

void pifTaskManager_Print()
{
	PifFixListIterator it;

	if (!pif_log_flag.bt.task) return;

	it = pifFixList_Begin(&s_tasks);
	while (it) {
		PifTask* p_owner = (PifTask*)it->data;
		if (p_owner->_mode == TM_RATIO) {
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_INFO, "Task Id=%d Ratio=%d%% Period=%2fus",
					p_owner->_id, p_owner->_period, p_owner->__period);
#endif
		}
		it = pifFixList_Next(it);
	}
}

void pifTaskManager_PrintRatioTable()
{
	int i;

	pifLog_Printf(LT_INFO, "Task Ratio Table Size=%d", PIF_TASK_TABLE_SIZE);
	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
		pifLog_Printf(LT_NONE, "\n %3d : %8lX", i, s_table[i]);
	}
}

#endif
