#include "core/pif_log.h"
#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"

#include <string.h>

// Central task scheduler and runtime execution loop management.

#ifndef PIF_TASK_STACK_SIZE
#define PIF_TASK_STACK_SIZE		5
#endif

// The three measurements are only carried when PIF_USE_BLOCK_TIME is defined. Reading them
// through these accessors keeps the scheduling rule written once for both configurations.
#ifdef PIF_USE_BLOCK_TIME
#define PIF_TASK_BLOCK_TIME(p_task)		((p_task)->_max_block_time)
#define PIF_TIMER_BLOCK_TIME(p_timer)	((p_timer)->_max_block_time)
#define PIF_IDLE_BLOCK_TIME				s_idle_max_block_time
#else
#define PIF_TASK_BLOCK_TIME(p_task)		0UL
#define PIF_TIMER_BLOCK_TIME(p_timer)	0UL
#define PIF_IDLE_BLOCK_TIME				0UL
#endif


static PifObjArray s_tasks;
static PifObjArrayIterator s_it_current;
static PifTask *s_task_stack[PIF_TASK_STACK_SIZE];
static int s_task_stack_ptr = 0;
static PifTask *s_current_task = NULL;
static uint32_t s_block_pretime = 0UL;		// Start of the run that is in progress between two scheduling points
static uint32_t s_task_load_start_time = 0UL;

static PifObjArray s_timers;

static PifEvtTaskIdle evt_task_idle = NULL;
static uint32_t s_idle_period;		// Default idle period in microseconds
static uint32_t s_idle_pretime;		// Idle timer in microseconds
#ifdef PIF_USE_BLOCK_TIME
static uint32_t s_idle_max_block_time = 0UL;	// Longest run of the idle callback
#endif

PifTask *g_task_cutin = NULL;

PifTask *g_realtime_task = NULL;

#ifdef PIF_DEBUG

PifActTaskSignal pif_act_task_signal = NULL;

#endif


static BOOL _isSharedTaskRunning(PifTask *p_task)
{
	int i;

	if (!p_task->disallow_yield_id) return FALSE;

	for (i = 0; i < s_task_stack_ptr; i++) {
		if (s_task_stack[i]->disallow_yield_id == p_task->disallow_yield_id) return TRUE;
	}
	return FALSE;
}

static PifTask *_processingRealTime()
{
	uint32_t current;

	current = (*pif_act_timer1us)();
	g_realtime_task->_delta_time = current - g_realtime_task->__pretime;
	if (g_realtime_task->_delta_time < g_realtime_task->__period) return NULL;

	g_realtime_task->__current_time = current;
	return g_realtime_task;
}

// Whether a run of max_block_time may start now. Timer and idle callbacks are not tasks, but
// they hold the CPU the same way, so they are judged by the same rule.
static BOOL _fitsInSlack(uint32_t max_block_time, uint32_t slack)
{
#ifdef PIF_USE_BLOCK_TIME
	if (!g_realtime_task) return TRUE;

	// The release is already due and nothing may make the delay worse.
	if (!slack) return FALSE;

	// This scheduler cannot preempt, so the delay a run imposes on the realtime task is its
	// length without yielding. It may only start while that run still fits in the time left
	// before the next release.
	if (max_block_time <= slack) return TRUE;

	// A run longer than the whole period fits in no slack at all and would be starved by this
	// rule. It is let through instead, and the delay it causes is recorded in pif_performance.
	return max_block_time >= g_realtime_task->__period;
#else
	(void)max_block_time;
	(void)slack;
	return TRUE;
#endif
}

static BOOL _fitsInRealtimeSlack(PifTask *p_owner, uint32_t slack)
{
	if (p_owner == g_realtime_task) return TRUE;

	return _fitsInSlack(PIF_TASK_BLOCK_TIME(p_owner), slack);
}

// Time left before the next release of the realtime task, taken from a timer reading that the
// caller already has.
static uint32_t _realtimeSlack(uint32_t current)
{
	uint32_t delta;

	if (!g_realtime_task || g_realtime_task->pause) return 0xFFFFFFFFUL;

	delta = current - g_realtime_task->__pretime;
	if (delta >= g_realtime_task->__period) return 0UL;

	return g_realtime_task->__period - delta;
}

static void _processingTrigger(PifTask *p_owner)
{
	uint32_t current;

	switch (p_owner->_mode) {
	case TM_REALTIME:
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
	uint32_t end_time;
	uint32_t block_time;
	uint32_t delay;
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t execute_time;
	uint32_t trigger_delay;
#else
	(void)trigger;
#endif

	if (s_task_stack_ptr >= PIF_TASK_STACK_SIZE) return;

	// How far past its release the realtime task actually starts. This is the whole guarantee,
	// and _max_block_time of each task tells which task is responsible when it grows.
	if (p_owner == g_realtime_task && p_owner->_delta_time > p_owner->__period) {
		delay = p_owner->_delta_time - p_owner->__period;
		if (delay > pif_performance._max_delay) pif_performance._max_delay = delay;
		if (delay >= p_owner->__period) pif_performance._miss_count++;
	}

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
	start_time = p_owner->_last_execute_time;
	s_block_pretime = start_time;
	period = (*p_owner->__evt_loop)(p_owner);
	end_time = (*pif_act_timer1us)();
	// The run since the last scheduling point is the time the CPU was really held. Unlike the
	// execution time it excludes what a yield spent waiting, so a yielding task no longer
	// inflates the load, and a nested task is counted once in its own right.
	block_time = end_time - s_block_pretime;
	s_block_pretime = end_time;
	pif_performance._task_time1us += block_time;
#ifdef PIF_USE_BLOCK_TIME
	// The longest such run is the delay this task can impose on the realtime task.
	if (block_time > p_owner->_max_block_time) p_owner->_max_block_time = block_time;
#endif
#ifdef PIF_USE_TASK_STATISTICS
	execute_time = end_time - start_time;
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
	case TM_REALTIME:
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

static void _processingIdle(uint32_t slack)
{
	uint32_t current, delta, block_time;

	if (!evt_task_idle) return;

	current = (*pif_act_timer1us)();
	delta = current - s_idle_pretime;
	if (delta < s_idle_period) return;

	// The idle callback is work the user wants done in the time left over, so it counts as load.
	// It cannot be preempted either, so it may only start while it fits in the slack.
	if (!_fitsInSlack(PIF_IDLE_BLOCK_TIME, slack)) return;

	s_idle_pretime = current;
	(*evt_task_idle)();
	// The callback must not yield, so the whole run is one block: the CPU it held, and the delay
	// it can impose on the realtime task.
	block_time = (*pif_act_timer1us)() - current;
#ifdef PIF_USE_BLOCK_TIME
	if (block_time > s_idle_max_block_time) s_idle_max_block_time = block_time;
#endif
	pif_performance._task_time1us += block_time;
}

static void _initCpuLoad()
{
	s_task_load_start_time = (*pif_act_timer1us)();
	s_block_pretime = s_task_load_start_time;
	pif_performance._task_time1us = 0UL;
	pif_performance._task_load = 0;
}

static void _updateCpuLoad()
{
	uint32_t current_time = (*pif_act_timer1us)();
	uint32_t elapsed_time = current_time - s_task_load_start_time;
	uint32_t task_time = pif_performance._task_time1us;
	uint32_t excess = 0UL;

	// The window is closed at a scheduling point once at least a second has passed, so it is
	// never a partially filled window. A single long run makes it longer than a second, which is
	// why the ratio is taken against the measured length and not against a fixed second.
	if (elapsed_time < 1000000UL) return;

	if (task_time > elapsed_time) {
		// A run cannot outlast the window it is accounted in, so this is a callback that yielded
		// against the rule. The overflow is handed to the next window while it stays plausible,
		// and dropped beyond that so the load recovers instead of sticking at 100%.
		excess = task_time - elapsed_time;
		if (excess >= elapsed_time) excess = 0UL;
		task_time = elapsed_time;
	}

	// Multiplying first keeps the remainder that dividing first would truncate into an error of
	// up to one percent. 42s is where the product would overflow 32bit.
	if (elapsed_time <= 42000000UL) {
		pif_performance._task_load = (uint8_t)((task_time * 100) / elapsed_time);
	}
	else {
		pif_performance._task_load = (uint8_t)(task_time / (elapsed_time / 100));
	}
	s_task_load_start_time = current_time;
	pif_performance._task_time1us = excess;
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
		}
	}

	if (pif_performance.__state & 2) {		// 1sec
#ifdef PIF_DEBUG
		if (pif_performance.__max_loop_time1us > max_loop) max_loop = pif_performance.__max_loop_time1us;
	#ifndef PIF_NO_LOG
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%luus(%lur/s) MLT=%luus", value, pif_performance._count, pif_performance.__max_loop_time1us);
		}
	#endif
		pif_performance.__max_loop_time1us = 0UL;
#else
	#ifndef PIF_NO_LOG
		if (pif_log_flag.bt.performance) {
			value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "LT:%luus(%lur/s)", value, pif_performance._count);
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
	g_realtime_task = NULL;
	pifTaskManager_ResetRealtime();
	_initCpuLoad();

	if (timer_count) {
		if (!pifObjArray_Init(&s_timers, sizeof(PifTaskTimer), timer_count, NULL)) goto fail;
	}
#ifdef PIF_USE_BLOCK_TIME
	pifTaskManager_ResetBlockTime();
#endif
	return TRUE;

fail:
	pifTaskManager_Clear();
	return FALSE;
}

void pifTaskManager_Clear()
{
	g_realtime_task = NULL;
	pifTaskManager_ResetRealtime();
	_initCpuLoad();
#ifdef PIF_USE_BLOCK_TIME
	pifTaskManager_ResetBlockTime();
#endif

	pifObjArray_Clear(&s_timers);
	pifObjArray_Clear(&s_tasks);
}

void pifTaskManager_ResetRealtime()
{
	pif_performance._max_delay = 0UL;
	pif_performance._miss_count = 0UL;
}

#ifdef PIF_USE_BLOCK_TIME

// The three block time measurements, the one of each task and the ones of the timer and idle
// callbacks, are cleared here together so that each value has a single owner.
void pifTaskManager_ResetBlockTime()
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		pifTask_ResetMaxBlockTime((PifTask *)it->data);
		it = pifObjArray_Next(it);
	}

	it = pifObjArray_Begin(&s_timers);
	while (it) {
		((PifTaskTimer *)it->data)->_max_block_time = 0UL;
		it = pifObjArray_Next(it);
	}

	s_idle_max_block_time = 0UL;
}

#endif

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
	if (p_task->_mode == TM_REALTIME) {
		g_realtime_task = NULL;
		pifTaskManager_ResetRealtime();
	}

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
	p_timer->_evt_timer = evt_timer;
	p_timer->_p_client = p_client;
	return p_timer;
}

void pifTaskManager_RemoveTimer(PifTaskTimer *p_timer)
{
	if (p_timer) {
		pifObjArray_Remove(&s_timers, p_timer);
	}
}

void pifTaskManager_SetIdle(PifEvtTaskIdle evt_idle, uint32_t period_ms)
{
	evt_task_idle = evt_idle;
	s_idle_period = period_ms * 1000UL;
	s_idle_pretime = (*pif_act_timer1us)();
#ifdef PIF_USE_BLOCK_TIME
	// The measurement belongs to the callback that was set, not to the one replacing it.
	s_idle_max_block_time = 0UL;
#endif
}

void pifTaskManager_Loop()
{
	PifTask *p_owner;
	PifTask *p_select = NULL;
	PifObjArrayIterator it_timer, it_timer_next;
	PifTaskTimer *p_timer;
	int i, n, count = pifObjArray_Count(&s_tasks);
	uint32_t diff, now, block_start, block_time;
	uint32_t slack;
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	// Timer callbacks are not part of the task ring, but they hold the CPU exactly as a task
	// does, so the time left before the realtime release limits them the same way. The slack is
	// derived from the reading above rather than from a new one.
	slack = _realtimeSlack(pif_timer1us);

	block_start = pif_timer1us;
	it_timer = pifObjArray_Begin(&s_timers);
	for (i = 0; i < pifObjArray_Count(&s_timers); i++) {
		p_timer = (PifTaskTimer *)it_timer->data;
		it_timer_next = pifObjArray_Next(it_timer);
		if (p_timer->_evt_timer && _fitsInSlack(PIF_TIMER_BLOCK_TIME(p_timer), slack)) {
			(*p_timer->_evt_timer)(p_timer->_p_client);
			// The callback must not yield, so its whole run is one block. The end of one block is
			// the start of the next, which keeps this to a single timer reading per callback.
			now = (*pif_act_timer1us)();
			block_time = now - block_start;
			block_start = now;
#ifdef PIF_USE_BLOCK_TIME
			if (block_time > p_timer->_max_block_time) p_timer->_max_block_time = block_time;
#endif
			pif_performance._task_time1us += block_time;
			// What this callback consumed is no longer available to the next one.
			slack = _realtimeSlack(now);
		}
		it_timer = it_timer_next;
	}

	if (!s_it_current) {
		if (!count) {
			_processingIdle(slack);
			_updateCpuLoad();
			return;
		}
		s_it_current = pifObjArray_Begin(&s_tasks);
	}

	if (g_task_cutin) {
		_processingTrigger(g_task_cutin);
		p_select = g_task_cutin;
		g_task_cutin = NULL;
		trigger = TRUE;
	}
	else {
		// The realtime task takes priority at its release. The check does not depend on the ring,
		// so it runs once per loop rather than once per task, and the ring keeps its position.
		// When it is not due yet, the time left until the release limits which task may start.
		// The slack is taken again here because the timer callbacks above consumed part of it.
		if (g_realtime_task && !g_realtime_task->pause) {
			p_select = _processingRealTime();
			if (!p_select) slack = g_realtime_task->__period - g_realtime_task->_delta_time;
		}

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
			if (!p_select && !p_owner->pause && p_owner->__processing && _fitsInRealtimeSlack(p_owner, slack)) {
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
		_processingIdle(slack);
	}
	_updateCpuLoad();
}

void pifTaskManager_Yield()
{
	PifTask *p_owner;
	PifTask *p_select = NULL;
	int i, n, count = pifObjArray_Count(&s_tasks);
	uint32_t diff;
	uint32_t slack = 0xFFFFFFFFUL;
	BOOL trigger = FALSE;

	pif_timer1us = (*pif_act_timer1us)();

	// The run that ends at this yield is CPU time the task really held, and it is what the
	// realtime task would have had to wait for. Timer and idle callbacks must not yield, so a
	// yield always happens inside a task and s_block_pretime always belongs to s_current_task.
	if (s_current_task) {
		diff = pif_timer1us - s_block_pretime;
		pif_performance._task_time1us += diff;
#ifdef PIF_USE_BLOCK_TIME
		if (diff > s_current_task->_max_block_time) s_current_task->_max_block_time = diff;
#endif
	}
	s_block_pretime = pif_timer1us;

	if (g_task_cutin && !g_task_cutin->_running) {
		_processingTrigger(g_task_cutin);
		p_select = g_task_cutin;
		g_task_cutin = NULL;
		trigger = TRUE;
	}
	else {
		// While the realtime task is the one that yielded there is no pending release to protect,
		// so the slack does not limit anything and the waiting task must be allowed to run.
		if (g_realtime_task && !g_realtime_task->pause && !g_realtime_task->_running) {
			if (_processingRealTime()) {
				slack = 0UL;
				if (!_isSharedTaskRunning(g_realtime_task)) p_select = g_realtime_task;
			}
			else {
				slack = g_realtime_task->__period - g_realtime_task->_delta_time;
			}
		}

		for (i = n = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

			if (p_owner->_running) goto next;
			if (_isSharedTaskRunning(p_owner)) goto next;

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
			if (!p_select && !p_owner->pause && p_owner->__processing && _fitsInRealtimeSlack(p_owner, slack)) {
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
#ifdef PIF_USE_BLOCK_TIME
	uint32_t block_time;
#endif
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
			case TM_REALTIME: mode = "RealTime"; break;
			case TM_PERIOD: mode = "Period"; break;
			case TM_EXTERNAL: mode = "External"; break;
	        default: mode = "---"; break;
		}
		if (p_owner->_default_period < 1000) {
			pifLog_Printf(LT_NONE, " (%u): %s-%luus Pause=%d\n", p_owner->_id, mode, p_owner->_default_period, p_owner->pause);
		}
		else {
			pifLog_Printf(LT_NONE, " (%u): %s-%1fms Pause=%d\n", p_owner->_id, mode, p_owner->_default_period / 1000.0, p_owner->pause);
		}
#ifdef PIF_USE_BLOCK_TIME
		pifLog_Printf(LT_NONE, "    Block: M=%luus\n", p_owner->_max_block_time);
#endif
#ifdef PIF_USE_TASK_STATISTICS
		// A is the moving average over the last n executions, not an average since boot.
		value = p_owner->__sum_execution_time[0] + p_owner->__sum_execution_time[1];
		pifLog_Printf(LT_NONE, "    Proc: M=%luus A=%luus(n=%u)\n", p_owner->_max_execution_time,
				(p_owner->__execution_count ? value / p_owner->__execution_count : 0),
				p_owner->__execution_count);

		value = p_owner->__total_delta_time[0] + p_owner->__total_delta_time[1];
		if (value) {
			pifLog_Printf(LT_NONE, "    Delta: %luus\n", value / p_owner->__execution_count);
		}

		value = p_owner->__total_trigger_delay[0] + p_owner->__total_trigger_delay[1];
		if (value) {
			pifLog_Printf(LT_NONE, "    Trigger: M=%luus A=%luus\n", p_owner->_max_trigger_delay,
					(p_owner->__trigger_count ? value / p_owner->__trigger_count : 0));
		}
#endif
#ifdef PIF_USE_BLOCK_TIME
		pifLog_Print(LT_NONE, "\n");
#endif
		it = pifObjArray_Next(it);
	}

	if (g_realtime_task) {
		pifLog_Printf(LT_NONE, "RealTime: MaxDelay=%luus Miss=%lu\n", pif_performance._max_delay,
				pif_performance._miss_count);
	}

#ifdef PIF_USE_BLOCK_TIME
	// Timer and idle callbacks run outside the task ring, so their block time is not in the list
	// above even though it delays the realtime task just as a task does.
	block_time = 0UL;
	it = pifObjArray_Begin(&s_timers);
	while (it) {
		PifTaskTimer *p_timer = (PifTaskTimer *)it->data;
		if (p_timer->_max_block_time > block_time) block_time = p_timer->_max_block_time;
		it = pifObjArray_Next(it);
	}
	if (block_time || s_idle_max_block_time) {
		pifLog_Printf(LT_NONE, "Callback block: Timer=%luus Idle=%luus\n", block_time,
				s_idle_max_block_time);
	}
#endif

	pifLog_Printf(LT_NONE, "Task Load: %u%%\n", pif_performance._task_load);
}

#endif	// PIF_NO_LOG
