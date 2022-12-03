#include "core/pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "core/pif_task.h"

#include <string.h>


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)


#ifdef __PIF_DEBUG__

PifActTaskMeasure pif_act_task_loop = NULL;
PifActTaskMeasure pif_act_task_yield = NULL;

#endif

static PifFixList s_tasks;
static PifFixListIterator s_it_current;
static PifTask* s_current_task;

static uint32_t s_table_number;
static uint32_t s_table[PIF_TASK_TABLE_SIZE];
static uint8_t s_number = 0;


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

static void _processingAlways(PifTask* p_owner)
{
	uint32_t gap;

	if (p_owner->__delay_ms) {
		gap = pif_cumulative_timer1ms - p_owner->__pretime;
		if (gap >= p_owner->__delay_ms) {
			p_owner->__delay_ms = 0;
		}
	}
	else {
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
	}
}

static void _processingPeriodUs(PifTask* p_owner)
{
	uint32_t current, gap;

	current = (*pif_act_timer1us)();
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		p_owner->__pretime = current;
	}
}

static void _processingPeriodMs(PifTask* p_owner)
{
	uint32_t current, gap;

	current = pif_cumulative_timer1ms;
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		p_owner->__pretime = current;
	}
}

static void _processingChangeUs(PifTask* p_owner)
{
	uint16_t period;
	uint32_t current, gap;

	current = (*pif_act_timer1us)();
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__running = TRUE;
		period = (*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		if (period > 0) p_owner->_period = period;
		p_owner->__pretime = current;
	}
}

static void _processingChangeMs(PifTask* p_owner)
{
	uint16_t period;
	uint32_t current, gap;

	current = pif_cumulative_timer1ms;
	gap = current - p_owner->__pretime;
	if (gap >= p_owner->_period) {
		p_owner->__running = TRUE;
		period = (*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		if (period > 0) p_owner->_period = period;
		p_owner->__pretime = current;
	}
}

static void _processingRatio(PifTask* p_owner)
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
#endif
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
#ifdef __PIF_DEBUG__
		p_owner->__count++;
#endif
	}
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

    case TM_ALWAYS:
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
    	p_owner->__pretime = pif_cumulative_timer1ms;
    	p_owner->__processing = _processingPeriodMs;
    	break;

    case TM_CHANGE_MS:
    	p_owner->__pretime = pif_cumulative_timer1ms;
    	p_owner->__processing = _processingChangeMs;
    	break;

    case TM_PERIOD_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingPeriodUs;
    	break;

    case TM_CHANGE_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	p_owner->__processing = _processingChangeUs;
    	break;

    default:
    	break;
    }

    p_owner->_mode = mode;
    p_owner->_period = period;
	return TRUE;
}

#ifndef __PIF_NO_LOG__

static void _checkLoopTime(BOOL yield)
{
#ifdef __PIF_DEBUG__
	static int step = 0;
	static uint32_t pretime;
	uint32_t gap;

	if (pif_act_timer1us) {
		if (!step) {
			if (!yield) step = 1;
		}
		else {
			gap = (*pif_act_timer1us)() - pretime;
			if (gap > pif_performance.__max_loop_time1us) {
				pif_performance.__max_loop_time1us = gap;
				pifLog_Printf(LT_NONE, "\nMLT: %luus", pif_performance.__max_loop_time1us);
			}
		}
		pretime = (*pif_act_timer1us)();
	}
#else
	(void)yield;
#endif

	if (pif_log_flag.bt.performance) {
		pif_performance._count++;
		if (pif_performance.__state) {
        	uint32_t value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "Performance: %lur/s, %uns", pif_performance._count, value);
        	pif_performance._count = 0;
    		pif_performance.__state = FALSE;
        }
    }
}

#endif


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

	if (!_setParam(p_owner, mode, period)) goto fail;

    p_owner->__evt_loop = evt_loop;
    p_owner->_p_client = p_client;
    p_owner->pause = !start;
    return p_owner;

fail:
	if (p_owner) {
		pifFixList_Remove(&s_tasks, p_owner);
	}
	return NULL;
}

void pifTaskManager_Remove(PifTask* p_task)
{
	switch (p_task->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		_resetTable(p_task->__table_number);
		break;

	default:
		break;
	}
	pifFixList_Remove(&s_tasks, p_task);
}

int pifTaskManager_Count()
{
	return pifFixList_Count(&s_tasks);
}

void pifTaskManager_Loop()
{
#if !defined(__PIF_NO_LOG__) && defined(__PIF_DEBUG__)
	static uint8_t sec = 0;
#endif

	PifFixListIterator it = s_it_current ? s_it_current : pifFixList_Begin(&s_tasks);
	while (it) {
		s_it_current = it;
		PifTask* p_owner = (PifTask*)it->data;
		s_current_task = p_owner;
		if (p_owner->immediate) {
			p_owner->immediate = FALSE;
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
		}
		else if (!p_owner->pause) (*p_owner->__processing)(p_owner);
		s_current_task = NULL;
#ifdef __PIF_DEBUG__
	    if (pif_act_task_loop) (*pif_act_task_loop)();
#endif
		it = pifFixList_Next(it);
	}

	s_number = (s_number + 1) & PIF_TASK_TABLE_MASK;
	s_it_current = NULL;

#if !defined(__PIF_NO_LOG__) && defined(__PIF_DEBUG__)
    if (sec != pif_datetime.second) {
    	pifTaskManager_Print();
    	sec = pif_datetime.second;
    }
#endif

#ifndef __PIF_NO_LOG__
    _checkLoopTime(FALSE);
#endif
}

void pifTaskManager_Yield()
{
	if (!pifFixList_Count(&s_tasks)) return;

	s_it_current = pifFixList_Next(s_it_current);
	if (!s_it_current) {
		s_number = (s_number + 1) & PIF_TASK_TABLE_MASK;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	PifTask* p_owner = (PifTask*)s_it_current->data;
	if (!p_owner->__running) {
		if (s_current_task->disallow_yield_id && s_current_task->disallow_yield_id == p_owner->disallow_yield_id) return;
		if (p_owner->immediate) {
			p_owner->immediate = FALSE;
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
		}
		else if (!p_owner->pause) (*p_owner->__processing)(p_owner);

#ifdef __PIF_DEBUG__
		if (pif_act_task_yield) (*pif_act_task_yield)();
#endif
	}

#ifndef __PIF_NO_LOG__
    _checkLoopTime(TRUE);
#endif
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

    if (!pif_act_timer1us) {
        start = pif_cumulative_timer1ms * 1000;
        do {
    		pifTaskManager_Yield();
		} while (pif_cumulative_timer1ms * 1000 - start <= time);
    }
    else {
    	start = (*pif_act_timer1us)();
		do {
			pifTaskManager_Yield();
		} while ((*pif_act_timer1us)() - start <= time);
    }
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
