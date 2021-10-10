#include <string.h>

#include "pif_list.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifTask.h"


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)


static PifDList s_tasks;
static PifDListIterator s_it_current;

static uint32_t s_table_number;
static uint32_t s_table[PIF_TASK_TABLE_SIZE];
static uint8_t s_number = 0;


static void _Processing(PifTask* p_owner, BOOL ratio)
{
	uint16_t period;
	uint32_t time, gap;

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
		p_owner->__running = TRUE;
		(*p_owner->__evt_loop)(p_owner);
		p_owner->__running = FALSE;
		break;

	case TM_PERIOD_US:
		time = (*pif_act_timer1us)();
		gap = time - p_owner->__pretime;
		if (gap >= p_owner->_period) {
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			p_owner->__pretime = time;
		}
		break;

	case TM_PERIOD_MS:
		time = 1000L * pif_timer1sec + pif_timer1ms;
		gap = time - p_owner->__pretime;
		if (gap >= p_owner->_period) {
			p_owner->__running = TRUE;
			(*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			p_owner->__pretime = time;
		}
		break;

	case TM_CHANGE_US:
		time = (*pif_act_timer1us)();
		gap = time - p_owner->__pretime;
		if (gap >= p_owner->_period) {
			p_owner->__running = TRUE;
			period = (*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			if (period > 0) p_owner->_period = period;
			p_owner->__pretime = time;
		}
		break;

	case TM_CHANGE_MS:
		time = 1000L * pif_timer1sec + pif_timer1ms;
		gap = time - p_owner->__pretime;
		if (gap >= p_owner->_period) {
			p_owner->__running = TRUE;
			period = (*p_owner->__evt_loop)(p_owner);
			p_owner->__running = FALSE;
			if (period > 0) p_owner->_period = period;
			p_owner->__pretime = time;
		}
		break;

	default:
		if (ratio) {
#ifdef __PIF_DEBUG__
			time = pif_timer1sec;
			if (time != p_owner->__pretime) {
				p_owner->__period = 1000000.0 / p_owner->__count;
				p_owner->__count = 0;
				p_owner->__pretime = time;
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

/**
 * @fn pifTask_Init
 * @brief
 * @param p_owner
 */
void pifTask_Init(PifTask* p_owner)
{
    pif_id++;
    p_owner->_id = pif_id;
}

/**
 * @fn pifTask_SetPeriod
 * @brief Task의 주기를 설정한다.
 * @param p_owner Task 자신
 * @param period Task 주기 (ms, us)
 */
void pifTask_SetPeriod(PifTask* p_owner, uint16_t period)
{
	switch (p_owner->_mode) {
	case TM_PERIOD_MS:
	case TM_PERIOD_US:
		p_owner->_period = period;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "Task:ChangePeriod(P:%u)", period);
#endif
		break;
	}
}


/**
 * @fn pifTaskManager_Init
 * @brief Task용 구조체를 초기화한다.
 */
void pifTaskManager_Init()
{
	pifDList_Init(&s_tasks);
	s_it_current = NULL;

	s_table_number = 0L;
	memset(s_table, 0, sizeof(s_table));
}

/**
 * @fn pifTaskManager_Clear
 * @brief Task용 메모리를 반환한다.
 */
void pifTaskManager_Clear()
{
	pifDList_Clear(&s_tasks);
}

/**
 * @fn pifTaskManager_Add
 * @brief Task를 추가한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param evt_loop Task 함수
 * @param p_client 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifTaskManager_Add(PifTaskMode mode, uint16_t period, PifEvtTaskLoop evt_loop, void* p_client, BOOL start)
{
	uint32_t count, gap, index;
	static int base = 0;
	int i, num = -1;

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

    switch (mode) {
    case TM_RATIO:
    	for (i = 0; i < PIF_TASK_TABLE_SIZE; i++) {
    		if (!(s_table_number & (1 << i))) {
    			num = i;
    			break;
    		}
    	}
    	if (i >= PIF_TASK_TABLE_SIZE) {
    		pif_error = E_OVERFLOW_BUFFER;
		    return NULL;
    	}
    	s_table_number |= 1 << num;

		count = (PIF_TASK_TABLE_SIZE - 1) * period + 100;
		gap = 10000L * PIF_TASK_TABLE_SIZE / count;
		if (gap > 100) {
			index = 100 * base;
			for (uint16_t i = 0; i < count / 100; i++) {
				s_table[(index / 100) & PIF_TASK_TABLE_MASK] |= 1 << num;
				index += gap;
			}
			base++;
		}
		else {
			mode = TM_ALWAYS;
			period = 100;
		}
    	break;

    case TM_ALWAYS:
    	mode = TM_ALWAYS;
    	period = 100;
    	break;

    default:
    	break;
    }

	PifTask* p_owner = (PifTask*)pifDList_AddLast(&s_tasks, sizeof(PifTask));
	if (!p_owner) return NULL;

    switch (mode) {
    case TM_RATIO:
    	p_owner->__table_number = num;
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
}

/**
 * @fn pifTaskManager_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTaskManager_Loop()
{
#ifdef __PIF_DEBUG__
	static uint8_t sec = 0;
#endif

	PifDListIterator it = s_it_current ? s_it_current : pifDList_Begin(&s_tasks);
	while (it) {
		s_it_current = it;
		PifTask* p_owner = (PifTask*)it->data;
		_Processing(p_owner, s_table[s_number] & (1 << p_owner->__table_number));
		it = pifDList_Next(it);
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

/**
 * @fn pifTaskManager_Yield
 * @brief Task내에서 대기중에 다른 Task를 호출하고자 할 경우에 사용하는 함수이다.
 */
void pifTaskManager_Yield()
{
	if (!pifDList_Size(&s_tasks)) return;

	s_it_current = pifDList_Next(s_it_current);
	if (!s_it_current) {
		s_number = (s_number + 1) & PIF_TASK_TABLE_MASK;
		s_it_current = pifDList_Begin(&s_tasks);
	}

	PifTask* p_owner = (PifTask*)s_it_current->data;
	if (!p_owner->__running) {
		_Processing(p_owner, s_table[s_number] & (1 << p_owner->__table_number));
	}
}

/**
 * @fn pifTaskManager_YieldMs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param usTime
 */
void pifTaskManager_YieldMs(uint32_t unTime)
{
    uint32_t unCurrent = 1000L * pif_timer1sec + pif_timer1ms;
    uint32_t unTarget = unCurrent + unTime;

	if (!pifDList_Size(&s_tasks)) return;
    if (!unTime) return;

    if (unTarget < unCurrent) {
    	while (unCurrent <= 0xFFFFFFFF) {
    		pifTaskManager_Yield();
    		unCurrent = 1000L * pif_timer1sec + pif_timer1ms;
    	}
    }
	while (unCurrent < unTarget) {
		pifTaskManager_Yield();
		unCurrent = 1000L * pif_timer1sec + pif_timer1ms;
	}
}

/**
 * @fn pifTaskManager_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param usTime
 */
void pifTaskManager_YieldUs(uint32_t unTime)
{
    uint32_t unCurrent = (*pif_act_timer1us)();
    uint32_t unTarget = unCurrent + unTime;

	if (!pifDList_Size(&s_tasks)) return;
    if (!unTime) return;
    if (!pif_act_timer1us) {
    	pifTaskManager_YieldMs((unTime + 999) / 1000);
    	return;
    }

    if (unTarget < unCurrent) {
    	while (unCurrent <= 0xFFFFFFFF) {
    		pifTaskManager_Yield();
    		unCurrent = (*pif_act_timer1us)();
    	}
    }
	while (unCurrent < unTarget) {
		pifTaskManager_Yield();
		unCurrent = (*pif_act_timer1us)();
	}
}

/**
 * @fn pifTaskManager_YieldPeriod
 * @brief loop내에서 지정된 주기동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param p_owner
 */
void pifTaskManager_YieldPeriod(PifTask *p_owner)
{
	PifDListIterator it;

	switch (p_owner->_mode) {
	case TM_RATIO:
	case TM_ALWAYS:
		it = pifDList_Begin(&s_tasks);
		while (it) {
			pifTaskManager_Yield();
			it = pifDList_Next(it);
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

/**
 * @fn pifTaskManager_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTaskManager_Print()
{
	PifDListIterator it;

	if (!pif_stLogFlag.bt.Task) return;

	it = pifDList_Begin(&s_tasks);
	while (it) {
		PifTask* p_owner = (PifTask*)it->data;
		if (p_owner->_mode == TM_RATIO) {
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_enInfo, "Task Id=%d Ratio=%d%% Period=%2fus",
					p_owner->_id, p_owner->_period, p_owner->__period);
#endif
		}
		it = pifDList_Next(it);
	}
}

#endif
