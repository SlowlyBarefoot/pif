#include "core/pif_log.h"
#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"

#include <string.h>

// Central task scheduler and runtime execution loop management.

#ifndef PIF_TASK_STACK_SIZE
#define PIF_TASK_STACK_SIZE		5
#endif

// Margin a run has to leave free before the realtime release, on top of its own length. It stands
// for what the scheduler spends between deciding that a run fits and dispatching the release that
// follows it, which no block time can measure. The minimum is only a starting point: a release
// that turns out to have been late raises the margin and on time releases lower it again, so it
// settles at what this build and this task set actually cost.
#ifndef PIF_TASK_GUARD_MIN_US
#define PIF_TASK_GUARD_MIN_US	2UL
#endif
#ifndef PIF_TASK_GUARD_MAX_US
#define PIF_TASK_GUARD_MAX_US	100UL
#endif

// Raised in larger steps than it is lowered, so the margin settles just above the cost instead of
// oscillating across it.
#define PIF_TASK_GUARD_UP_US	4UL
#define PIF_TASK_GUARD_DOWN_US	1UL

// How many times in a row the margin above may hold a release back before it is let through
// anyway. The margin alone gives no bound: a run shorter than the realtime period but longer
// than the slack it happens to be offered can be refused on every visit, and nothing in the rule
// makes the next visit any more likely to succeed. With this, the wait a task can suffer is
// stated instead: at most this many passes of the ring while the task is already due, after
// which it runs and the release it delays is counted as a guard lapse.
// It is a bound, not a target. Set it low and the guarantee tightens while realtime jitter
// grows, because more runs are let through against the margin; set it high and the opposite.
// PifTask::max_skip and PifTaskTimer::max_skip override it per owner.
#ifndef PIF_TASK_MAX_SKIP
#define PIF_TASK_MAX_SKIP		10
#endif

// The three measurements are only carried when PIF_USE_BLOCK_TIME is defined. Reading them
// through these accessors keeps the scheduling rule written once for both configurations.
#ifdef PIF_USE_BLOCK_TIME
#define PIF_TASK_BLOCK_TIME(p_task)		((p_task)->_block_time._max)
#define PIF_TIMER_BLOCK_TIME(p_timer)	((p_timer)->_block_time._max)
#define PIF_IDLE_BLOCK_TIME				s_idle_block_time._max
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
static PifBlockTime s_idle_block_time;			// Longest run of the idle callback

// Margin a run has to leave free before the realtime release, and how many runs were let through
// without it. Both belong to the scheduling rule in this file and nothing outside it reads them,
// so pifTaskManager_Print() is where they are reported.
static uint32_t s_task_guard = PIF_TASK_GUARD_MIN_US;
static uint32_t s_guard_lapse_count = 0UL;
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

// Whether the realtime task is due, judged against a timer reading the caller already has. A
// release comes from a trigger as well as from the period, and either one gives the task priority
// over the whole ring. The trigger is tested first because it names an exact time while the
// period only names a grid.
static PifTask *_processingRealTime(uint32_t current, BOOL *p_trigger)
{
	uint32_t diff;

	if (g_realtime_task->__trigger) {
		if (g_realtime_task->__trigger_delay) {
			// An interrupt can set the trigger after the caller took its reading, which puts the
			// trigger time later than the reading and wraps the subtraction. Read unsigned that
			// looks like a delay long expired, and the release would fire at once instead of
			// after the delay asked for, so the wrapped case is rejected by its sign.
			diff = current - g_realtime_task->__trigger_time;
			if ((int32_t)diff >= 0 && diff >= g_realtime_task->__trigger_delay) {
				g_realtime_task->__trigger_delay = 0;
			}
		}
		if (!g_realtime_task->__trigger_delay) {
			g_realtime_task->__trigger = FALSE;
			g_realtime_task->_delta_time = current - g_realtime_task->__pretime;
			g_realtime_task->__current_time = current;
			*p_trigger = TRUE;
			return g_realtime_task;
		}
	}

	// A pause stops the periodic release but not a trigger, exactly as it does for every other
	// task. That is why it is tested here and not at the call: this function is the only place
	// the realtime task is released, so the test has to cover both kinds of release.
	if (g_realtime_task->pause) return NULL;

	// Released by trigger alone, so there is no grid to test.
	if (!g_realtime_task->__period) return NULL;

	g_realtime_task->_delta_time = current - g_realtime_task->__pretime;
	if (g_realtime_task->_delta_time < g_realtime_task->__period) return NULL;

	g_realtime_task->__current_time = current;
	return g_realtime_task;
}

// Whether a run of max_block_time may start now. Timer and idle callbacks are not tasks, but
// they hold the CPU the same way, so they are judged by the same rule.
// p_skip_count is where the consecutive refusals of this owner are kept, and NULL asks for no
// bound at all: the idle callback is the work to be done with the time left over, so having none
// left is the answer rather than a wait to be cut short.
// The caller must only ask about a run it would actually start, because a refusal is counted:
// asking on behalf of a task that is not due yet would spend the bound on nothing.
static BOOL _fitsInSlack(uint32_t max_block_time, uint32_t slack, uint16_t *p_skip_count, uint16_t max_skip)
{
#ifdef PIF_USE_BLOCK_TIME
	uint32_t guard;

	if (!g_realtime_task) return TRUE;

	// The release is already due and nothing may make the delay worse. Not counted as a refusal:
	// the release is dispatched before the ring is reached again, so the slack the next visit is
	// offered is a full period and the wait ends on its own.
	if (!slack) return FALSE;

	// This scheduler cannot preempt, so the delay a run imposes on the realtime task is its
	// length without yielding. It may only start while that length, and the margin covering what
	// the scheduler itself spends getting back here, both still fit in the time left before the
	// next release.
	guard = s_task_guard;
	if (slack > guard && max_block_time <= slack - guard) {
		if (p_skip_count) *p_skip_count = 0;
		return TRUE;
	}

	// A realtime task with no period starves nothing, because its slack is finite only while a
	// delayed trigger is pending and that ends on its own.
	if (!g_realtime_task->__period) return FALSE;

	// A run longer than the whole period fits in no slack at all, and one that has been refused
	// max_skip times in a row has waited as long as this rule promises. Either way it is let
	// through and counted: the guarantee does not hold for that release, and _guard_lapse_count
	// is the only place that says so.
	if (max_block_time < g_realtime_task->__period) {
		if (!p_skip_count) return FALSE;
		if (!max_skip) max_skip = PIF_TASK_MAX_SKIP;
		if (++(*p_skip_count) < max_skip) return FALSE;
		*p_skip_count = 0;
	}

	s_guard_lapse_count++;
	return TRUE;
#else
	(void)max_block_time;
	(void)slack;
	(void)max_skip;
	if (p_skip_count) *p_skip_count = 0;
	return TRUE;
#endif
}

static BOOL _fitsInRealtimeSlack(PifTask *p_owner, uint32_t slack)
{
	if (p_owner == g_realtime_task) return TRUE;

	return _fitsInSlack(PIF_TASK_BLOCK_TIME(p_owner), slack, &p_owner->__skip_count, p_owner->max_skip);
}

// Time left before the next release of the realtime task, taken from a timer reading that the
// caller already has.
static uint32_t _realtimeSlack(uint32_t current)
{
	uint32_t slack = 0xFFFFFFFFUL;
	uint32_t delta;

	if (!g_realtime_task || g_realtime_task->pause) return 0xFFFFFFFFUL;

	// A pending trigger names a release time that is already known, so it is what the slack is
	// measured against. While no trigger is pending there is nothing to protect, and that is what
	// keeps a task released by trigger alone from holding every other task for as long as its
	// trigger takes to arrive.
	if (g_realtime_task->__trigger) {
		delta = current - g_realtime_task->__trigger_time;
		// The trigger is newer than this reading, so none of its delay has elapsed yet.
		if ((int32_t)delta < 0) delta = 0UL;
		if (delta >= g_realtime_task->__trigger_delay) return 0UL;
		slack = g_realtime_task->__trigger_delay - delta;
	}

	if (g_realtime_task->__period) {
		delta = current - g_realtime_task->__pretime;
		if (delta >= g_realtime_task->__period) return 0UL;
		delta = g_realtime_task->__period - delta;
		if (delta < slack) slack = delta;
	}

	return slack;
}

#ifdef PIF_USE_BLOCK_TIME

// Fed with how late the realtime release actually turned out to be. A late release means the
// margin did not cover what the scheduler spends, so it grows; on time releases let it shrink
// back, which keeps a margin that grew during a busy phase from costing throughput for good.
static void _updateTaskGuard(uint32_t delay)
{
	if (delay) {
		if (s_task_guard >= PIF_TASK_GUARD_MAX_US - PIF_TASK_GUARD_UP_US) {
			s_task_guard = PIF_TASK_GUARD_MAX_US;
		}
		else {
			s_task_guard += PIF_TASK_GUARD_UP_US;
		}
	}
	else if (s_task_guard >= PIF_TASK_GUARD_MIN_US + PIF_TASK_GUARD_DOWN_US) {
		s_task_guard -= PIF_TASK_GUARD_DOWN_US;
	}
	else {
		s_task_guard = PIF_TASK_GUARD_MIN_US;
	}
}

#endif

static void _processingTrigger(PifTask *p_owner)
{
	switch (p_owner->_mode) {
	case TM_REALTIME:
	case TM_PERIOD:
	case TM_EXTERNAL:
		p_owner->_delta_time = pif_timer1us - p_owner->__pretime;
		p_owner->__current_time = pif_timer1us;
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
	uint32_t diff;
	uint32_t delay;
	uint32_t trigger_diff;
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t execute_time;
#endif

	if (s_task_stack_ptr >= PIF_TASK_STACK_SIZE) return;

	// One reading serves both the release measurements below and the start of the run, so the
	// dispatch is timed where it happens rather than where it was decided.
	start_time = (*pif_act_timer1us)();

	// How long the release waited after the trigger that caused it. An interrupt can set a new
	// trigger between the reading above and here, which puts the trigger time later than the
	// reading and wraps the subtraction; the latency is zero in that case, not an hour.
	trigger_diff = 0UL;
	if (trigger) {
		trigger_diff = start_time - p_owner->__trigger_time;
		if ((int32_t)trigger_diff < 0) trigger_diff = 0UL;
	}

	// How far past its own period the task actually starts. Nothing else reports this for a period
	// task, and for the realtime task it is the whole guarantee: the block time of each task tells
	// which task is responsible when it grows.
	delay = 0UL;
	if (p_owner->__period) {
		diff = start_time - p_owner->__pretime;
		if (diff > p_owner->__period) delay = diff - p_owner->__period;
	}
#ifdef PIF_USE_TASK_STATISTICS
	if (delay > p_owner->_max_delay) p_owner->_max_delay = delay;
#endif

	if (p_owner == g_realtime_task) {
#ifdef PIF_USE_BLOCK_TIME
		// Only the lateness against the period feeds the margin, and it is read before the trigger
		// is folded in below. A trigger with no delay is not a release the scheduler could have
		// reserved time for, so the lateness it produces says nothing about whether the margin is
		// large enough and would only ratchet it to the maximum. A task released by trigger alone
		// therefore keeps the margin it was configured with.
		_updateTaskGuard(delay);
#endif
		// A release by a trigger is late by the time since the trigger rather than by the time
		// past the period, so what is reported is whichever of the two is larger.
		if (trigger_diff > delay) delay = trigger_diff;
		if (delay > pif_performance._max_delay) pif_performance._max_delay = delay;
		if (p_owner->__period && delay >= p_owner->__period) pif_performance._miss_count++;
	}

	p_owner->__pretime = p_owner->__current_time;

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(TRUE);
#endif

#ifdef PIF_USE_TASK_STATISTICS
	if (trigger) {
		if (trigger_diff > p_owner->_max_trigger_delay) p_owner->_max_trigger_delay = trigger_diff;
		p_owner->__total_trigger_delay[p_owner->__trigger_index] += trigger_diff;
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

	// The bound counts refusals since the last run, and a trigger dispatches without asking the
	// slack rule at all, so the count is cleared where every release ends up rather than only on
	// the path that tests it.
	p_owner->__skip_count = 0;

	s_current_task = p_owner;
    s_task_stack[s_task_stack_ptr] = p_owner;
	s_task_stack_ptr++;
	p_owner->_running = TRUE;
	p_owner->_last_execute_time = start_time;
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
	// The longest such run is the delay this task can impose on the realtime task. A run the task
	// itself declared unrepresentative is kept out of that estimate, but it was still added to the
	// load above: the CPU was held either way.
	if (!p_owner->__ignore_block) pifTask_UpdateBlockTime(&p_owner->_block_time, block_time);
	p_owner->__ignore_block = FALSE;
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
	// It cannot be preempted either, so it may only start while it fits in the slack. No bound is
	// asked for: having no time left over is the answer for idle work, not a wait to cut short.
	if (!_fitsInSlack(PIF_IDLE_BLOCK_TIME, slack, NULL, 0)) return;

	s_idle_pretime = current;
	(*evt_task_idle)();
	// The callback must not yield, so the whole run is one block: the CPU it held, and the delay
	// it can impose on the realtime task.
	block_time = (*pif_act_timer1us)() - current;
#ifdef PIF_USE_BLOCK_TIME
	pifTask_UpdateBlockTime(&s_idle_block_time, block_time);
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

	// Dropped before the array it points into is cleared. Left set it would survive as a pointer
	// to a freed node, and pifTaskManager_Add() only fills in a ring pointer that is NULL, so the
	// next task would leave it in place and the ring would walk the free list.
	s_it_current = NULL;

	pifObjArray_Clear(&s_timers);
	pifObjArray_Clear(&s_tasks);
}

void pifTaskManager_ResetRealtime()
{
	pif_performance._max_delay = 0UL;
	pif_performance._miss_count = 0UL;
#ifdef PIF_USE_BLOCK_TIME
	// The margin starts at its lower bound and finds its own level from the releases that follow.
	s_task_guard = PIF_TASK_GUARD_MIN_US;
	s_guard_lapse_count = 0UL;
#endif
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
		pifTask_ResetBlockTime(&((PifTaskTimer *)it->data)->_block_time);
		it = pifObjArray_Next(it);
	}

	pifTask_ResetBlockTime(&s_idle_block_time);
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
	pifTask_ResetBlockTime(&s_idle_block_time);
#endif
}

void pifTaskManager_Loop()
{
	PifTask *p_owner;
	PifTask *p_select = NULL;
	PifObjArrayIterator it_timer, it_timer_next;
	PifTaskTimer *p_timer;
	int i, count = pifObjArray_Count(&s_tasks);
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
		if (p_timer->_evt_timer &&
				_fitsInSlack(PIF_TIMER_BLOCK_TIME(p_timer), slack, &p_timer->__skip_count, p_timer->max_skip)) {
			(*p_timer->_evt_timer)(p_timer->_p_client);
			// The callback must not yield, so its whole run is one block. The end of one block is
			// the start of the next, which keeps this to a single timer reading per callback.
			now = (*pif_act_timer1us)();
			block_time = now - block_start;
			block_start = now;
#ifdef PIF_USE_BLOCK_TIME
			pifTask_UpdateBlockTime(&p_timer->_block_time, block_time);
#endif
			pif_performance._task_time1us += block_time;
			// What this callback consumed is no longer available to the next one, and the ring
			// below is judged against the same reading.
			pif_timer1us = now;
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
		// When it is not due yet, the slack taken above is what limits which task may start.
		if (g_realtime_task) {
			p_select = _processingRealTime(pif_timer1us, &trigger);
		}

		for (i = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

			// Released by _processingRealTime() above and nowhere else. Visiting it here would
			// only repeat the same tests against the same reading, and its slack is measured
			// against itself, so the ring holds no dispatch path for it.
			if (p_owner == g_realtime_task) goto next;

			if (p_owner->__trigger) {
				if (p_owner->__trigger_delay) {
					// A negative difference means an interrupt set the trigger after the reading
					// at the top of this loop was taken, so none of the delay has elapsed yet.
					// Left unsigned it would wrap and read as long expired.
					diff = pif_timer1us - p_owner->__trigger_time;
					if ((int32_t)diff >= 0 && diff >= p_owner->__trigger_delay) {
						p_owner->__trigger_delay = 0;
					}
				}
				if (!p_owner->__trigger_delay) {
					p_owner->__trigger = FALSE;
					_processingTrigger(p_owner);
					p_select = p_owner;
					trigger = TRUE;
				}
			}
			// Due first, slack second. The slack rule counts the releases it holds back, so
			// asking it about a task that is not due yet would spend that count on a release
			// that was never wanted and the bound would never reach a task that is waiting.
			if (!p_select && !p_owner->pause && p_owner->__processing) {
				PifTask *p_due = (*p_owner->__processing)(p_owner);
				if (p_due && _fitsInRealtimeSlack(p_owner, slack)) p_select = p_due;
			}

next:
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
	int i, count = pifObjArray_Count(&s_tasks);
	uint32_t diff;
	uint32_t slack = 0xFFFFFFFFUL;
	BOOL trigger = FALSE;

	// There is nothing to yield to before the ring exists, which is where boot is while the tasks
	// are still being registered. A driver that waits with pifTaskManager_YieldMs() inside its own
	// initialization therefore spins instead of dispatching, and the ring pointer below is safe to
	// dereference: it is NULL only while no task is registered.
	// Nothing is skipped by leaving here. No task can be running with none registered, so there is
	// no block time to account for, and the CPU load window stays open until the first loop, which
	// reports the boot as the idle time it was.
	if (!s_it_current) return;

	pif_timer1us = (*pif_act_timer1us)();

	// The run that ends at this yield is CPU time the task really held, and it is what the
	// realtime task would have had to wait for. Timer and idle callbacks must not yield, so a
	// yield always happens inside a task and s_block_pretime always belongs to s_current_task.
	if (s_current_task) {
		diff = pif_timer1us - s_block_pretime;
		pif_performance._task_time1us += diff;
#ifdef PIF_USE_BLOCK_TIME
		// The flag is cleared where the execution ends, so every run of an execution the task
		// declared unrepresentative is skipped, not only the last one.
		if (!s_current_task->__ignore_block) {
			pifTask_UpdateBlockTime(&s_current_task->_block_time, diff);
		}
#endif
	}
	s_block_pretime = pif_timer1us;

	// A task holding the same resource can be on the stack at a yield, and the cut in path was
	// the one place that dispatched without asking. The request is left pending when it cannot be
	// taken, so it is delayed rather than dropped, and the selection below still finds other work
	// meanwhile. pifTaskManager_Loop() needs no such test: it is the outermost caller, so its
	// stack is empty and nothing can be holding a resource.
	if (g_task_cutin && !g_task_cutin->_running && !_isSharedTaskRunning(g_task_cutin)) {
		_processingTrigger(g_task_cutin);
		p_select = g_task_cutin;
		g_task_cutin = NULL;
		trigger = TRUE;
	}
	else {
		// While the realtime task is the one that yielded there is no pending release to protect,
		// so the slack does not limit anything and the waiting task must be allowed to run.
		if (g_realtime_task && !g_realtime_task->_running) {
			// Asked before the release is taken, because consuming a pending trigger and then
			// finding the release undispatchable would drop it.
			if (!_isSharedTaskRunning(g_realtime_task)) {
				p_select = _processingRealTime(pif_timer1us, &trigger);
			}
			// Not taken here. _realtimeSlack() reports no slack while the release is due, which
			// keeps the ring below from making the delay worse.
			if (!p_select) slack = _realtimeSlack(pif_timer1us);
		}

		for (i = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

			// See the same skip in pifTaskManager_Loop(). The block above is the only place the
			// realtime task is released, here as there.
			if (p_owner == g_realtime_task) goto next;
			if (p_owner->_running) goto next;
			if (_isSharedTaskRunning(p_owner)) goto next;

			if (p_owner->__trigger) {
				if (p_owner->__trigger_delay) {
					// See the same test in pifTaskManager_Loop(): the wrapped case is a trigger
					// newer than the reading, not a delay that expired long ago.
					diff = pif_timer1us - p_owner->__trigger_time;
					if ((int32_t)diff >= 0 && diff >= p_owner->__trigger_delay) {
						p_owner->__trigger_delay = 0;
					}
				}
				if (!p_owner->__trigger_delay) {
					p_owner->__trigger = FALSE;
					_processingTrigger(p_owner);
					p_select = p_owner;
					trigger = TRUE;
				}
			}
			// Due first, slack second. The slack rule counts the releases it holds back, so
			// asking it about a task that is not due yet would spend that count on a release
			// that was never wanted and the bound would never reach a task that is waiting.
			if (!p_select && !p_owner->pause && p_owner->__processing) {
				PifTask *p_due = (*p_owner->__processing)(p_owner);
				if (p_due && _fitsInRealtimeSlack(p_owner, slack)) p_select = p_due;
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
			pifLog_Printf(LT_NONE, " (%u): %s-%1fms Pause=%d\n", p_owner->_id, mode, p_owner->_default_period / 1000.0L, p_owner->pause);
		}
#ifdef PIF_USE_BLOCK_TIME
		pifLog_Printf(LT_NONE, "    Block: M=%luus\n", p_owner->_block_time._max);
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

		// How late this task started against its own period. A trigger release is reported on the
		// line above instead, because it is not measured against the period.
		if (p_owner->_max_delay) {
			pifLog_Printf(LT_NONE, "    Delay: M=%luus\n", p_owner->_max_delay);
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
#ifdef PIF_USE_BLOCK_TIME
		// A lapse is a run that was let through although it does not fit before the release, so
		// the guarantee did not hold for it. Either the run is longer than the whole period, in
		// which case it is the task whose block time above exceeds the period printed for the
		// realtime task, or a release had been held back max_skip times in a row and was let
		// through to bound its wait. Lapses climbing with no oversized task points at the
		// latter, and raising PIF_TASK_MAX_SKIP trades that jitter back for a longer wait.
		pifLog_Printf(LT_NONE, "          Guard=%luus Lapse=%lu\n", s_task_guard,
				s_guard_lapse_count);
#endif
	}

#ifdef PIF_USE_BLOCK_TIME
	// Timer and idle callbacks run outside the task ring, so their block time is not in the list
	// above even though it delays the realtime task just as a task does.
	block_time = 0UL;
	it = pifObjArray_Begin(&s_timers);
	while (it) {
		PifTaskTimer *p_timer = (PifTaskTimer *)it->data;
		if (p_timer->_block_time._max > block_time) block_time = p_timer->_block_time._max;
		it = pifObjArray_Next(it);
	}
	if (block_time || s_idle_block_time._max) {
		pifLog_Printf(LT_NONE, "Callback block: Timer=%luus Idle=%luus\n", block_time,
				s_idle_block_time._max);
	}
#endif

	pifLog_Printf(LT_NONE, "Task Load: %u%%\n", pif_performance._task_load);
}

#endif	// PIF_NO_LOG
