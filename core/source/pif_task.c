#include <string.h>

#include "pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_task.h"


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)


#ifdef __PIF_DEBUG__

PifActTaskMeasure pif_act_task_loop = NULL;
PifActTaskMeasure pif_act_task_yield = NULL;

#endif

static PifFixList s_tasks;
static PifFixListIterator s_it_current;

static uint32_t s_table_number;
static uint32_t s_table[PIF_TASK_TABLE_SIZE];
static uint8_t s_number = 0;


static int _setTable(uint16_t period, PifTaskMode* p_mode)
{
	uint32_t count, gap, index, bit;
	static int base = 0;
	int i, num = -1;

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
		for (uint16_t i = 0; i < count / 100; i++) {
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

static void _processing(PifTask* p_owner, BOOL ratio)
{
	uint16_t period;
	uint32_t time, gap;
#ifdef __PIF_DEBUG__
	static uint32_t pretime;
#endif

	if (p_owner->pause) return;
	else if (p_owner->immediate) {
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		p_owner->immediate = FALSE;
		return;
	}

	switch (p_owner->_mode) {
	case TM_ALWAYS:
		if (p_owner->__delay_ms) {
			gap = pif_cumulative_timer1ms - p_owner->__pretime;
			if (gap > p_owner->__delay_ms) {
				p_owner->__delay_ms = 0;
			}
		}
		else {
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
		}
		break;

	case TM_PERIOD_US:
		gap = pif_cumulative_timer1us - p_owner->__pretime;
		if (gap > p_owner->_period) {
			p_owner->__pretime = pif_cumulative_timer1us;
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
		}
		break;

	case TM_PERIOD_MS:
		time = 1000L * pif_timer1sec + pif_timer1ms;
		gap = time - p_owner->__pretime;
		if (gap > p_owner->_period) {
			p_owner->__pretime = time;
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
		}
		break;

	case TM_CHANGE_US:
		gap = pif_cumulative_timer1us - p_owner->__pretime;
		if (gap > p_owner->_period) {
			p_owner->__pretime = pif_cumulative_timer1us;
			p_owner->__running = TRUE;
			period = (*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			if (period > 0) p_owner->_period = period;
		}
		break;

	case TM_CHANGE_MS:
		time = 1000L * pif_timer1sec + pif_timer1ms;
		gap = time - p_owner->__pretime;
		if (gap > p_owner->_period) {
			p_owner->__pretime = time;
			p_owner->__running = TRUE;
			period = (*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			if (period > 0) p_owner->_period = period;
		}
		break;

	default:
		if (p_owner->__delay_ms) {
			gap = pif_cumulative_timer1ms - p_owner->__pretime;
			if (gap > p_owner->__delay_ms) {
				p_owner->__delay_ms = 0;
			}
		}
		else if (ratio) {
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
	int num = -1;

	switch (mode) {
    case TM_RATIO:
    	if (!period || period > 100) {
    		pif_error = E_INVALID_PARAM;
		    return FALSE;
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

	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		_resetTable(p_owner->__table_number);
		break;

	default:
		break;
	}

    switch (mode) {
    case TM_RATIO:
    	num = _setTable(period, &mode);
    	if (num == -1) return FALSE;
    	if (mode == TM_ALWAYS) period = 100;
    	p_owner->__table_number = num;
    	break;

    case TM_ALWAYS:
    	period = 100;
    	break;

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    	p_owner->__pretime = 1000L * pif_timer1sec + pif_timer1ms;
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	break;
    }

    p_owner->_mode = mode;
    p_owner->_period = period;
    return TRUE;
}

void pifTask_ChangePeriod(PifTask* p_owner, uint16_t period)
{
	switch (p_owner->_mode) {
	case TM_PERIOD_MS:
	case TM_PERIOD_US:
		p_owner->_period = period;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "Task:ChangePeriod(P:%u)", period);
#endif
		break;
	}
}

void pifTask_DelayMs(PifTask* p_owner, uint16_t delay)
{
	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		p_owner->__delay_ms = delay;
    	p_owner->__pretime = 1000L * pif_timer1sec + pif_timer1ms;
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
	int num = -1;

	if (!evt_loop) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
	}

	switch (mode) {
    case TM_RATIO:
    	if (!period || period > 100) {
    		pif_error = E_INVALID_PARAM;
		    return NULL;
    	}
    	break;

    case TM_ALWAYS:
    	break;

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return NULL;
    	}
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	if (!period) {
    		pif_error = E_INVALID_PARAM;
		    return NULL;
    	}

    	if (!pif_act_timer1us) {
    		pif_error = E_CANNOT_USE;
		    return NULL;
        }
    	break;
    }

	PifTask* p_owner = (PifTask*)pifFixList_AddFirst(&s_tasks);
	if (!p_owner) return NULL;

    switch (mode) {
    case TM_RATIO:
    	num = _setTable(period, &mode);
    	if (num == -1) goto fail;
    	if (mode == TM_ALWAYS) period = 100;
    	p_owner->__table_number = num;
    	break;

    case TM_ALWAYS:
    	period = 100;
    	break;

    case TM_PERIOD_MS:
    case TM_CHANGE_MS:
    	p_owner->__pretime = 1000L * pif_timer1sec + pif_timer1ms;
    	break;

    case TM_PERIOD_US:
    case TM_CHANGE_US:
    	p_owner->__pretime = (*pif_act_timer1us)();
    	break;

    default:
    	break;
    }

    p_owner->_mode = mode;
    p_owner->_period = period;
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
#ifdef __PIF_DEBUG__
	static uint8_t sec = 0;
#endif

	PifFixListIterator it = s_it_current ? s_it_current : pifFixList_Begin(&s_tasks);
	while (it) {
		if (pif_act_timer1us) {
			pif_cumulative_timer1us = (*pif_act_timer1us)();
		}

		s_it_current = it;
		PifTask* p_owner = (PifTask*)it->data;
		_processing(p_owner, s_table[s_number] & (1 << p_owner->__table_number));
#ifdef __PIF_DEBUG__
	    if (pif_act_task_loop) (*pif_act_task_loop)();
#endif
		it = pifFixList_Next(it);
	}

	s_number = (s_number + 1) & PIF_TASK_TABLE_MASK;
	s_it_current = NULL;

#ifdef __PIF_DEBUG__
    if (sec != pif_datetime.second) {
    	pifTaskManager_Print();
    	sec = pif_datetime.second;
    }
#endif
}

void pifTaskManager_Yield()
{
	if (!pifFixList_Count(&s_tasks)) return;

	if (pif_act_timer1us) {
		pif_cumulative_timer1us = (*pif_act_timer1us)();
	}

	s_it_current = pifFixList_Next(s_it_current);
	if (!s_it_current) {
		s_number = (s_number + 1) & PIF_TASK_TABLE_MASK;
		s_it_current = pifFixList_Begin(&s_tasks);
	}

	PifTask* p_owner = (PifTask*)s_it_current->data;
	if (!p_owner->__running) {
		_processing(p_owner, s_table[s_number] & (1 << p_owner->__table_number));

#ifdef __PIF_DEBUG__
		if (pif_act_task_yield) (*pif_act_task_yield)();
#endif
	}
}

void pifTaskManager_YieldMs(uint32_t time)
{
    uint32_t start = 1000L * pif_timer1sec + pif_timer1ms;
    uint32_t current;

	if (!pifFixList_Count(&s_tasks)) return;
    if (!time) return;

    do {
		pifTaskManager_Yield();
        current = 1000L * pif_timer1sec + pif_timer1ms;
    } while (current - start <= time);
}

void pifTaskManager_YieldUs(uint32_t time)
{
    uint32_t start;
    uint32_t current;
    uint32_t delay;

	if (!pifFixList_Count(&s_tasks)) return;
    if (!time) return;

    if (!pif_act_timer1us) {
    	delay = (time + 999) / 1000;
    	pifTaskManager_YieldMs(delay > 0 ? delay : 1);
    }
    else {
    	start = (*pif_act_timer1us)();
		do {
			pifTaskManager_Yield();
			current = (*pif_act_timer1us)();
		} while (current - start <= time);
    }
}

void pifTaskManager_YieldPeriod(PifTask *p_owner)
{
	PifFixListIterator it;

	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		it = pifFixList_Begin(&s_tasks);
		while (it) {
			pifTaskManager_Yield();
			it = pifFixList_Next(it);
		}
		break;

	case TM_PERIOD_MS:
	case TM_CHANGE_MS:
		pifTaskManager_YieldMs(p_owner->_period);
		break;

	case TM_PERIOD_US:
	case TM_CHANGE_US:
		pifTaskManager_YieldUs(p_owner->_period);
		break;

	default:
		break;
	}
}

#ifdef __PIF_DEBUG__

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
