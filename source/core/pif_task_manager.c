// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"

#include <string.h>

// Central task scheduler and runtime execution loop management.

#ifdef PIF_USE_TASK_REALTIME

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

// How long the margin above may hold a release back before it is let through anyway. The margin
// alone gives no bound: a run shorter than the realtime period but longer than the slack it
// happens to be offered can be refused on every visit, and nothing in the rule makes the next
// visit any more likely to succeed. By default the bound is one realtime period from the first
// refusal. In that time the realtime task has run once and left the widest slack there is, so a
// run that still did not fit never will. It is then let through and the release it delays is
// counted as a guard lapse.
// A count of passes, PifTask::max_skip and PifTaskTimer::max_skip or PIF_TASK_MAX_SKIP for all
// of them, takes its place. A pass lasts a few microseconds on one CPU and tens on another, so
// the same count bounds a different wait on each.

#endif	// PIF_USE_TASK_REALTIME

// How late a release is, against its period or its trigger, is only of use to the realtime task and
// to the statistics.
#if defined(PIF_USE_TASK_REALTIME) || defined(PIF_USE_TASK_STATISTICS)
#define TASK_RELEASE_DELAY
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
static PifTask *s_current_task = NULL;
static uint32_t s_task_load_start_time = 0UL;

static PifObjArray s_timers;

static PifEvtTaskIdle evt_task_idle = NULL;
static uint32_t s_idle_period;		// Default idle period in microseconds
static uint32_t s_idle_pretime;		// Idle timer in microseconds
#ifdef PIF_USE_TASK_STATISTICS
// The timer callbacks together, over the CPU load window: they run at every pass of the main loop,
// so how often and how long they run is what tells their share of the load.
static uint32_t s_timer_window_count;	// Calls that did work in the current window
static uint32_t s_timer_window_time;	// Their time in microseconds
static uint32_t s_timer_rate;			// Calls per second over the last window
static uint32_t s_timer_average10;		// Average call in tenths of a microsecond
static uint8_t s_timer_load;			// Share of the last window in percent

// What a PifTask keeps for itself, kept here for the idle callback. The two buckets take turns
// exactly as the ones of a task do.
static struct {
	uint32_t total_execution_time;
	uint32_t max_execution_time;
	uint32_t sum_delta_time[2];
	uint32_t sum_execution_time[2];
	uint16_t count;
	uint8_t index;
} s_idle_stat;
#endif
#ifdef PIF_USE_BLOCK_TIME
static PifBlockTime s_idle_block_time;			// Longest run of the idle callback

#ifdef PIF_USE_TASK_REALTIME
// Margin a run has to leave free before the realtime release, and how many runs were let through
// without it. Both belong to the scheduling rule in this file, and are reported only by
// pifTaskManager_Print() and pifTaskManager_GetRealtimeStatistics().
static uint32_t s_task_guard = PIF_TASK_GUARD_MIN_US;
static uint32_t s_guard_lapse_count = 0UL;
// End of the last run other than the realtime task, a task, a timer callback or the idle callback,
// and whether it came after the previous realtime release. A release is late through the margin
// only when such a run was still going at the time the release fell due.
static uint32_t s_run_end;
static BOOL s_run_since_release = FALSE;
#endif
#endif

PifTask *g_task_cutin = NULL;

#ifdef PIF_USE_ISR_STATISTICS
static PifTaskIsr *s_p_isr_first = NULL;
static uint16_t s_isr_load10 = 0;		// All of them, in tenths of a percent of the last window
#endif

#ifdef PIF_USE_TASK_STATISTICS
// End of the last run left out by pifTask_IgnoreStatistics(). The releases that fell due before it
// were held up by that run, so their lateness is not measured. The end is dropped a second later,
// when every such release has run and before the time can wrap around it.
#define TASK_IGNORE_HOLD_US		1000000UL

static BOOL s_ignore_active = FALSE;
static uint32_t s_ignore_end;
#endif

#ifdef PIF_USE_TASK_REALTIME
PifTask *g_realtime_task = NULL;
// How late the previous release of the realtime task was against its grid, 0 when the grid was
// started over. The grid point and this give back the time of that release.
uint32_t g_realtime_late = 0UL;
#endif

#ifdef PIF_DEBUG

PifActTaskSignal pif_act_task_signal = NULL;

#endif


#ifdef PIF_USE_TASK_REALTIME

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
				g_realtime_task->__trigger_time += g_realtime_task->__trigger_delay;
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
	if (g_realtime_task->_pause) return NULL;

	// Released by trigger alone, so there is no grid to test.
	if (!g_realtime_task->__period) return NULL;

	g_realtime_task->_delta_time = current - g_realtime_task->__pretime;
	if (g_realtime_task->_delta_time < g_realtime_task->__period) return NULL;

	g_realtime_task->__current_time = current;
	return g_realtime_task;
}

// Whether a run of max_block_time may start now. Timer and idle callbacks are not tasks, but
// they hold the CPU the same way, so they are judged by the same rule.
// p_skip_count and p_skip_time are where the consecutive refusals of this owner and the time of
// the first one are kept, and a NULL p_skip_count asks for no bound at all: the idle callback is
// the work to be done with the time left over, so having none left is the answer rather than a
// wait to be cut short.
// The caller must only ask about a run it would actually start, because a refusal is counted:
// asking on behalf of a task that is not due yet would spend the bound on nothing.
static BOOL _fitsInSlack(uint32_t max_block_time, uint32_t slack, uint16_t *p_skip_count, uint32_t *p_skip_time,
		uint16_t max_skip)
{
#ifdef PIF_USE_BLOCK_TIME
	uint32_t guard;

	if (!g_realtime_task) return TRUE;

	// The release is already due and nothing may make the delay worse. Not counted as a refusal:
	// the release is dispatched before the ring is reached again, so the slack the next visit is
	// offered is a full period and the wait ends on its own.
	if (!slack) return FALSE;

	// A run cannot be interrupted once it has started, so the delay it imposes on the realtime
	// task is its whole length. It may only start while that length, and the margin covering what
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
	// for as long as the bound allows has waited as long as this rule promises. Either way it is
	// let through and counted: the guarantee does not hold for that release, and
	// _guard_lapse_count is the only place that says so.
	if (max_block_time < g_realtime_task->__period) {
		if (!p_skip_count) return FALSE;
#ifdef PIF_TASK_MAX_SKIP
		if (!max_skip) max_skip = PIF_TASK_MAX_SKIP;
#endif
		if (max_skip) {
			if (++(*p_skip_count) < max_skip) return FALSE;
		}
		else {
			// The pass reading of pifTaskManager_Loop() is the time of this refusal.
			if (!*p_skip_count) {
				*p_skip_count = 1;
				*p_skip_time = pif_timer1us;
				return FALSE;
			}
			if (pif_timer1us - *p_skip_time < g_realtime_task->__period) return FALSE;
		}
		*p_skip_count = 0;
	}

	s_guard_lapse_count++;
	return TRUE;
#else
	(void)max_block_time;
	(void)slack;
	(void)p_skip_time;
	(void)max_skip;
	if (p_skip_count) *p_skip_count = 0;
	return TRUE;
#endif
}

static BOOL _fitsInRealtimeSlack(PifTask *p_owner, uint32_t slack)
{
	uint32_t block_time;

	if (p_owner == g_realtime_task) return TRUE;

	// What the task said its next run would be, when it said anything. The measurement is the
	// longest of a window of runs, which is the right answer for a task whose runs are alike and
	// the wrong one for a state machine whose states differ by an order of magnitude.
	block_time = PIF_TASK_BLOCK_TIME(p_owner);
#ifdef PIF_USE_BLOCK_TIME
	if (p_owner->__next_block_time) block_time = p_owner->__next_block_time;
#endif

	return _fitsInSlack(block_time, slack, &p_owner->__skip_count, &p_owner->__skip_time, p_owner->max_skip);
}

// Time left before the next release of the realtime task, taken from a timer reading that the
// caller already has.
static uint32_t _realtimeSlack(uint32_t current)
{
	uint32_t slack = 0xFFFFFFFFUL;
	uint32_t delta;

	if (!g_realtime_task || g_realtime_task->_pause) return 0xFFFFFFFFUL;

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

static void _markRunEnd(uint32_t end_time)
{
	s_run_end = end_time;
	s_run_since_release = TRUE;
}

// Fed with whether a run was let start that was still going when the realtime release fell due.
// That means the margin did not cover what the scheduler spends between judging the run and
// starting it, so it grows. Any other release lets it shrink back, which keeps a margin that grew
// during a busy phase from costing throughput for good. A release that is late only by the time
// the loop takes to come round to it is not counted against the margin: a larger margin keeps
// runs from starting but cannot bring the loop round any sooner, and on a slow CPU every release
// is late by that much, which would hold the margin at its maximum for nothing.
static void _updateTaskGuard(BOOL overran)
{
	if (overran) {
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

// The realtime task keeps a grid of its period. The next release is counted from when this one was
// due rather than from when the loop got to it, so the lateness of one release is not added to
// every period after it and the task runs period apart on average. A release late by a whole period
// or more starts the grid over from now instead of catching up with a burst, and a release by
// trigger starts it from now as it always has.
static void _advanceRealtimeGrid(BOOL trigger)
{
	PifTask *p_task = g_realtime_task;
	uint32_t late;

	// The grid point is no longer the time of the previous release, which is added back here.
	p_task->_delta_time = p_task->__current_time - (p_task->__pretime + g_realtime_late);

	late = p_task->__current_time - p_task->__pretime - p_task->__period;
	if (!trigger && p_task->__period && late < p_task->__period) {
		p_task->__pretime += p_task->__period;
		g_realtime_late = late;
	}
	else {
		p_task->__pretime = p_task->__current_time;
		g_realtime_late = 0UL;
	}
}

#else	// PIF_USE_TASK_REALTIME

// With no realtime release to keep time free for, every run fits. The slack is still named, so a
// caller that only passes it on has a use for it.
#define _realtimeSlack(current)										(0xFFFFFFFFUL)
#define _fitsInSlack(max_block_time, slack, p_skip_count, p_skip_time, max_skip)	((void)(slack), TRUE)
#define _fitsInRealtimeSlack(p_owner, slack)						((void)(slack), TRUE)

#endif	// PIF_USE_TASK_REALTIME

static void _processingTrigger(PifTask *p_owner)
{
	switch (p_owner->_mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
#endif
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
#ifdef TASK_RELEASE_DELAY
	uint32_t diff;
	uint32_t delay;
	uint32_t trigger_diff;
#endif
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t execute_time;
	uint32_t due;
	BOOL skip;
	BOOL resumed;
#endif

	// One reading serves both the release measurements below and the start of the run, so the
	// dispatch is timed where it happens rather than where it was decided.
	start_time = (*pif_act_timer1us)();

#ifdef TASK_RELEASE_DELAY

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
	// Neither the first release after a resume nor one that fell due before an ignored run ended
	// is measured: both are late for reasons outside the load.
	skip = p_owner->__skip_delay;
	resumed = p_owner->__skip_delay;
	p_owner->__skip_delay = FALSE;
	if (s_ignore_active) {
		if (start_time - s_ignore_end >= TASK_IGNORE_HOLD_US) {
			s_ignore_active = FALSE;
		}
		else {
			due = trigger ? p_owner->__trigger_time : p_owner->__pretime + p_owner->__period;
			if ((int32_t)(due - s_ignore_end) < 0) skip = TRUE;
		}
	}
	if (!skip && delay > p_owner->_max_delay) p_owner->_max_delay = delay;
#endif
#else
	(void)trigger;
#endif

#ifdef PIF_USE_TASK_REALTIME
	if (p_owner == g_realtime_task) {
#ifdef PIF_USE_BLOCK_TIME
		// Only a release by the period feeds the margin. A trigger with no delay is not a release
		// the scheduler could have reserved time for, so a run still going at it says nothing
		// about whether the margin is large enough and would only ratchet it to the maximum. A
		// task released by trigger alone therefore keeps the margin it was configured with.
		_updateTaskGuard(!trigger && p_owner->__period && s_run_since_release &&
				(int32_t)(s_run_end - (p_owner->__pretime + p_owner->__period)) > 0);
		s_run_since_release = FALSE;
#endif
		// A release by a trigger is late by the time since the trigger rather than by the time
		// past the period, so what is reported is whichever of the two is larger.
		if (trigger_diff > delay) delay = trigger_diff;
		if (delay > pif_performance._max_delay) pif_performance._max_delay = delay;
		if (p_owner->__period && delay >= p_owner->__period) pif_performance._miss_count++;
	}
#endif

#ifdef PIF_USE_TASK_REALTIME
	if (p_owner == g_realtime_task) _advanceRealtimeGrid(trigger);
	else p_owner->__pretime = p_owner->__current_time;
#else
	p_owner->__pretime = p_owner->__current_time;
#endif

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(TRUE);
#endif

#ifdef PIF_USE_TASK_STATISTICS
	if (trigger && !skip) {
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

	// The bound counts refusals since the last run, and a cut in dispatches without asking the
	// slack rule at all, so the count is cleared where every release ends up rather than only on
	// the path that tests it.
#ifdef PIF_USE_TASK_REALTIME
	p_owner->__skip_count = 0;
#endif

	s_current_task = p_owner;
	p_owner->_last_execute_time = start_time;
#ifdef PIF_USE_BLOCK_TIME
	// Consumed here, so that a task which declared a length once and then stopped goes back to
	// being judged by its measurement rather than keeping the old declaration for good.
	p_owner->__next_block_time = 0;
#endif
	period = (*p_owner->__evt_loop)(p_owner);
	end_time = (*pif_act_timer1us)();
	// A run cannot give the CPU up before it returns, so the whole run is the time the CPU was
	// held.
	block_time = end_time - start_time;
	pif_performance._task_time1us += block_time;
#if defined(PIF_USE_TASK_REALTIME) && defined(PIF_USE_BLOCK_TIME)
	if (p_owner != g_realtime_task) _markRunEnd(end_time);
#endif
#ifdef PIF_USE_TASK_STATISTICS
	// Counted ahead of pifTask_IgnoreStatistics(): the run took place whatever its length.
	p_owner->__window_count++;
#endif
#ifdef PIF_USE_BLOCK_TIME
	// The longest such run is the delay this task can impose on the realtime task. A run the task
	// itself declared unrepresentative is kept out of that estimate, but it was still added to the
	// load above: the CPU was held either way.
	if (!p_owner->__ignore_block) pifTask_UpdateBlockTime(&p_owner->_block_time, block_time);
	p_owner->__ignore_block = FALSE;
#endif
#ifdef PIF_USE_TASK_STATISTICS
	if (p_owner->__ignore_stats) {
		p_owner->__ignore_stats = FALSE;
		s_ignore_end = end_time;
		s_ignore_active = TRUE;
		goto stats_done;
	}
	execute_time = block_time;
	p_owner->_total_execution_time += execute_time;
	if (execute_time > p_owner->_max_execution_time) p_owner->_max_execution_time = execute_time;
	p_owner->__sum_execution_time[p_owner->__execute_index] += execute_time;
	p_owner->__execution_count++;
	if (p_owner->__execution_count == 200) {
		p_owner->__execution_count -= 100;
		p_owner->__execute_index ^= 1;
		p_owner->__sum_execution_time[p_owner->__execute_index] = 0;
	}
	else if (p_owner->__execution_count == 100) {
		p_owner->__execute_index ^= 1;
	}
	// Only the time apart of releases by the default period is kept. An external task and a release
	// by trigger run when they are triggered, a period the task returned holds for one release only,
	// and the first release after a resume or a period change follows the pause or the old period:
	// none of these says anything about how well the default period is kept. __period still holds
	// the period that caused this release here, as the one the task returned is taken in below.
	if (p_owner->_mode != TM_EXTERNAL && !trigger && !resumed && p_owner->__period == p_owner->_default_period) {
		p_owner->__total_delta_time[p_owner->__delta_index] += p_owner->_delta_time;
		p_owner->__delta_count++;
		if (p_owner->__delta_count == 200) {
			p_owner->__delta_count -= 100;
			p_owner->__delta_index ^= 1;
			p_owner->__total_delta_time[p_owner->__delta_index] = 0;
		}
		else if (p_owner->__delta_count == 100) {
			p_owner->__delta_index ^= 1;
		}
	}
stats_done:
#endif
	s_current_task = NULL;

#ifdef PIF_DEBUG
    if (pif_act_task_signal) (*pif_act_task_signal)(FALSE);
#endif

	switch (p_owner->_mode) {
#ifdef PIF_USE_TASK_REALTIME
	case TM_REALTIME:
#endif
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
	if (!_fitsInSlack(PIF_IDLE_BLOCK_TIME, slack, NULL, NULL, 0)) return;

	s_idle_pretime = current;
	(*evt_task_idle)();
	// The whole run is one block: the CPU it held, and the delay it can impose on the realtime
	// task.
	block_time = (*pif_act_timer1us)() - current;
#ifdef PIF_USE_BLOCK_TIME
	pifTask_UpdateBlockTime(&s_idle_block_time, block_time);
#ifdef PIF_USE_TASK_REALTIME
	_markRunEnd(current + block_time);
#endif
#endif
	pif_performance._task_time1us += block_time;
#ifdef PIF_USE_TASK_STATISTICS
	// delta is the time since the previous run started, which is what the delta time of a task
	// measures as well.
	s_idle_stat.total_execution_time += block_time;
	if (block_time > s_idle_stat.max_execution_time) s_idle_stat.max_execution_time = block_time;
	s_idle_stat.sum_delta_time[s_idle_stat.index] += delta;
	s_idle_stat.sum_execution_time[s_idle_stat.index] += block_time;
	s_idle_stat.count++;
	if (s_idle_stat.count == 200) {
		s_idle_stat.count -= 100;
		s_idle_stat.index ^= 1;
		s_idle_stat.sum_delta_time[s_idle_stat.index] = 0;
		s_idle_stat.sum_execution_time[s_idle_stat.index] = 0;
	}
	else if (s_idle_stat.count == 100) {
		s_idle_stat.index ^= 1;
	}
#endif
}

static void _initCpuLoad()
{
	// pifTaskManager_Clear() may run before pif_Init(), when there is no timer to read yet.
	s_task_load_start_time = pif_act_timer1us ? (*pif_act_timer1us)() : 0UL;
	pif_performance._task_time1us = 0UL;
	pif_performance._task_load = 0;
}

static void _updateCpuLoad()
{
	uint32_t current_time = (*pif_act_timer1us)();
	uint32_t elapsed_time = current_time - s_task_load_start_time;
	uint32_t task_time = pif_performance._task_time1us;
	uint32_t excess = 0UL;
#ifdef PIF_USE_TASK_STATISTICS
	PifObjArrayIterator it;
	PifTask *p_task;
#endif
#if defined(PIF_USE_TASK_STATISTICS) || defined(PIF_USE_ISR_STATISTICS)
	uint32_t elapsed_ms;
#endif
#ifdef PIF_USE_ISR_STATISTICS
	PifTaskIsr *p_isr;
	uint32_t state, count, sample_count, sample_ticks, max_ticks, isr_us10 = 0UL;
#endif

	// The window is closed at a scheduling point once at least a second has passed, so it is
	// never a partially filled window. A single long run makes it longer than a second, which is
	// why the ratio is taken against the measured length and not against a fixed second.
	if (elapsed_time < 1000000UL) return;

#if defined(PIF_USE_TASK_STATISTICS) || defined(PIF_USE_ISR_STATISTICS)
	// The window is at least 1000 ms long, so dividing it into milliseconds loses at most 0.1%.
	elapsed_ms = elapsed_time / 1000UL;
#endif
#ifdef PIF_USE_ISR_STATISTICS
	// The counts are taken and cleared together, since the interrupts go on adding to them. The
	// share is every call at the average of the sample, in tenths of a microsecond: 10 ms of interrupts
	// in a window come to 100000, against the 1000 ms of the window.
	for (p_isr = s_p_isr_first; p_isr; p_isr = p_isr->__p_next) {
		state = pif_IrqDisable();
		count = p_isr->__count;
		sample_count = p_isr->__sample_count;
		sample_ticks = p_isr->__sample_ticks;
		max_ticks = p_isr->__max_ticks;
		p_isr->__count = 0UL;
		p_isr->__sample_count = 0UL;
		p_isr->__sample_ticks = 0UL;
		p_isr->__max_ticks = 0UL;
		pif_IrqRestore(state);

		p_isr->_rate = (count * 1000UL) / elapsed_ms;
		p_isr->_average10 = sample_count ? (sample_ticks * 10UL / PIF_ISR_TICKS_PER_US) / sample_count : 0UL;
		p_isr->_max = max_ticks / PIF_ISR_TICKS_PER_US;
		p_isr->_load10 = (uint16_t)((count * p_isr->_average10) / (10UL * elapsed_ms));
		isr_us10 += count * p_isr->_average10;
	}
	s_isr_load10 = (uint16_t)(isr_us10 / (10UL * elapsed_ms));
#endif
#ifdef PIF_USE_TASK_STATISTICS
	// The product would overflow only past four million runs in the window. One division per
	// task is left, which on a CPU without a divider is most of what closing the window costs.
	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		p_task = (PifTask *)it->data;
		p_task->_execution_rate = (p_task->__window_count * 1000UL) / elapsed_ms;
		p_task->__window_count = 0UL;
		it = pifObjArray_Next(it);
	}
	s_timer_rate = (s_timer_window_count * 1000UL) / elapsed_ms;
	s_timer_average10 = s_timer_window_count ? (s_timer_window_time * 10UL) / s_timer_window_count : 0UL;
	s_timer_load = (uint8_t)(s_timer_window_time / (elapsed_time / 100UL));
	s_timer_window_count = 0UL;
	s_timer_window_time = 0UL;
#endif

	if (task_time > elapsed_time) {
		// A single run longer than the window it is accounted in leaves the window nothing to
		// measure against. The overflow is handed to the next window while it stays plausible, and
		// dropped beyond that so the load recovers instead of sticking at 100%.
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
	// The task manager reads the 1us timer from here on.
	if (pif_state < PST_TIMER) {
		pif_error = E_INVALID_STATE;
	    return FALSE;
	}

	if (!pifObjArray_Init(&s_tasks, sizeof(PifTask), max_count, NULL)) return FALSE;
	s_it_current = NULL;
#ifdef PIF_USE_TASK_REALTIME
	g_realtime_task = NULL;
#endif
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
#ifdef PIF_USE_TASK_REALTIME
	g_realtime_task = NULL;
#endif
	pifTaskManager_ResetRealtime();
	_initCpuLoad();
#ifdef PIF_USE_ISR_STATISTICS
	// The interrupts go on counting into their own objects, which are only no longer reported.
	s_p_isr_first = NULL;
	s_isr_load10 = 0;
#endif
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
#if defined(PIF_USE_BLOCK_TIME) && defined(PIF_USE_TASK_REALTIME)
	// The margin starts at its lower bound and finds its own level from the releases that follow.
	s_task_guard = PIF_TASK_GUARD_MIN_US;
	s_guard_lapse_count = 0UL;
	s_run_since_release = FALSE;
#endif
}

#ifdef PIF_USE_TASK_REALTIME

void pifTaskManager_GetRealtimeStatistics(PifTaskRealtimeStatistics *p_stat, BOOL reset)
{
	p_stat->max_delay = pif_performance._max_delay;
	p_stat->miss_count = pif_performance._miss_count;
#ifdef PIF_USE_BLOCK_TIME
	p_stat->guard = s_task_guard;
	p_stat->lapse_count = s_guard_lapse_count;
#else
	p_stat->guard = 0UL;
	p_stat->lapse_count = 0UL;
#endif

	if (reset) {
		pif_performance._max_delay = 0UL;
		pif_performance._miss_count = 0UL;
#ifdef PIF_USE_BLOCK_TIME
		s_guard_lapse_count = 0UL;
#endif
	}
}

#endif

#ifdef PIF_USE_BLOCK_TIME

// The three block time measurements, the one of each task and the ones of the timer and idle
// callbacks, are cleared here together so that each value has a single owner.
void pifTaskManager_ResetBlockTime()
{
	PifObjArrayIterator it;
	PifTaskTimer *p_timer;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		pifTask_ResetMaxBlockTime((PifTask *)it->data);
		it = pifObjArray_Next(it);
	}

	it = pifObjArray_Begin(&s_timers);
	while (it) {
		p_timer = (PifTaskTimer *)it->data;
		pifTask_ResetBlockTime(&p_timer->_block_time);
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
    p_owner->_pause = (mode != TM_EXTERNAL) ? !start : TRUE;
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
#ifdef PIF_USE_TASK_REALTIME
	if (p_task->_mode == TM_REALTIME) {
		g_realtime_task = NULL;
		pifTaskManager_ResetRealtime();
	}
#endif

	if (s_it_current && p_task == (PifTask *)s_it_current->data) s_it_current = NULL;

	pifObjArray_Remove(&s_tasks, p_task);

	if (!pifObjArray_Count(&s_tasks)) s_it_current = NULL;
	else if (!s_it_current) s_it_current = pifObjArray_Begin(&s_tasks);
}

int pifTaskManager_Count()
{
	return pifObjArray_Count(&s_tasks);
}

PifTask *pifTaskManager_CurrentTask()
{
	return s_current_task;
}

#ifdef PIF_USE_ISR_STATISTICS

BOOL pifTaskManager_AddIsr(PifTaskIsr *p_isr, const char *name, uint16_t sample)
{
	PifTaskIsr **pp_last;
	uint32_t state;

	if (!sample) sample = PIF_ISR_SAMPLE;
	if (!p_isr) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	for (pp_last = &s_p_isr_first; *pp_last; pp_last = &(*pp_last)->__p_next) {
		if (*pp_last == p_isr) {
			pif_error = E_INVALID_STATE;
			return FALSE;
		}
	}

	p_isr->name = name;
	p_isr->_rate = 0UL;
	p_isr->_average10 = 0UL;
	p_isr->_max = 0UL;
	p_isr->_load10 = 0;
	p_isr->__p_next = NULL;
	// The interrupt may be running already.
	state = pif_IrqDisable();
	p_isr->__sample = sample;
	p_isr->__skip = 0;
	p_isr->__count = 0UL;
	p_isr->__sample_count = 0UL;
	p_isr->__sample_ticks = 0UL;
	p_isr->__max_ticks = 0UL;
	pif_IrqRestore(state);
	// Last in the order they are added, which is the order of the report.
	*pp_last = p_isr;
	return TRUE;
}

#endif

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
#ifdef PIF_USE_TASK_STATISTICS
	// So do the statistics.
	memset(&s_idle_stat, 0, sizeof(s_idle_stat));
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
	BOOL worked;

	// Before pif_Init() there is no timer to read, and while its setups run the tasks are not all there yet.
	if (pif_state < PST_RUNNING) return;

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
				_fitsInSlack(PIF_TIMER_BLOCK_TIME(p_timer), slack, &p_timer->__skip_count, &p_timer->__skip_time,
					p_timer->max_skip)) {
			worked = (*p_timer->_evt_timer)(p_timer->_p_client);
			// The end of one callback is the start of the next, which keeps this to a single
			// timer reading per callback.
			now = (*pif_act_timer1us)();
			block_time = now - block_start;
			block_start = now;
#ifdef PIF_USE_BLOCK_TIME
			pifTask_UpdateBlockTime(&p_timer->_block_time, block_time);
#ifdef PIF_USE_TASK_REALTIME
			_markRunEnd(now);
#endif
#endif
			// A call that found nothing to do is the loop waiting, not load. Its time still goes into
			// the block time above, which only keeps the longest call.
			if (worked) {
				pif_performance._task_time1us += block_time;
#ifdef PIF_USE_TASK_STATISTICS
				s_timer_window_count++;
				s_timer_window_time += block_time;
#endif
			}
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
#ifdef PIF_USE_TASK_REALTIME
		if (g_realtime_task) {
			p_select = _processingRealTime(pif_timer1us, &trigger);
		}
#endif

		for (i = 0; i < count && !p_select; i++) {
			p_owner = (PifTask *)s_it_current->data;

#ifdef PIF_USE_TASK_REALTIME
			// Released by _processingRealTime() above and nowhere else. Visiting it here would
			// only repeat the same tests against the same reading, and its slack is measured
			// against itself, so the ring holds no dispatch path for it.
			if (p_owner == g_realtime_task) goto next;
#endif

			if (p_owner->__trigger) {
				if (p_owner->__trigger_delay) {
					// A negative difference means an interrupt set the trigger after the reading
					// at the top of this loop was taken, so none of the delay has elapsed yet.
					// Left unsigned it would wrap and read as long expired.
					diff = pif_timer1us - p_owner->__trigger_time;
					if ((int32_t)diff >= 0 && diff >= p_owner->__trigger_delay) {
						// The trigger is due from the end of its delay, so its lateness is counted
						// from there and not from when it was set.
						p_owner->__trigger_time += p_owner->__trigger_delay;
						p_owner->__trigger_delay = 0;
					}
				}
				if (!p_owner->__trigger_delay) {
					// A trigger is due the way a period is, and waits for the slack by the same
					// rule. It stays pending for the next pass, and this pass asks nothing else of
					// the task: the same slack would refuse its period as well, and asking twice
					// would count one wait as two refusals.
					if (!_fitsInRealtimeSlack(p_owner, slack)) goto next;
					p_owner->__trigger = FALSE;
					_processingTrigger(p_owner);
					p_select = p_owner;
					trigger = TRUE;
				}
			}
			// Due first, slack second. The slack rule counts the releases it holds back, so
			// asking it about a task that is not due yet would spend that count on a release
			// that was never wanted and the bound would never reach a task that is waiting.
			if (!p_select && !p_owner->_pause && p_owner->__processing) {
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
#ifdef PIF_USE_TASK_REALTIME
	// Closing the window is not a task and is not judged against the slack, and on a slow CPU it
	// takes hundreds of microseconds. Right after the realtime run the whole period is ahead, so
	// with a periodic realtime task the window is closed there and nowhere else. It closes up to a
	// period late, which the measured length of the window takes into account.
	if (!g_realtime_task || g_realtime_task->_pause || !g_realtime_task->__period ||
			p_select == g_realtime_task) {
		_updateCpuLoad();
	}
#else
	_updateCpuLoad();
#endif
}

void pifTaskManager_AllTask(void (*callback)(PifTask *p_task, void *p_arg), void *p_arg)
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
	 	(*callback)((PifTask *)it->data, p_arg);
		it = pifObjArray_Next(it);
	}
}

#ifdef PIF_USE_TASK_STATISTICS

BOOL pifTaskManager_GetIdleStatistics(PifTaskIdleStatistics *p_stat)
{
	uint32_t count = s_idle_stat.count;

	if (!evt_task_idle) return FALSE;

	p_stat->total_execution_time = s_idle_stat.total_execution_time;
	p_stat->max_execution_time = s_idle_stat.max_execution_time;
	if (count < PIF_TASK_AVERAGE_MIN_COUNT) {
		p_stat->average_execution_time = PIF_TASK_AVERAGE_NONE;
		p_stat->average_delta_time = PIF_TASK_AVERAGE_NONE;
	}
	else {
		p_stat->average_execution_time = (s_idle_stat.sum_execution_time[0] + s_idle_stat.sum_execution_time[1]) / count;
		p_stat->average_delta_time = (s_idle_stat.sum_delta_time[0] + s_idle_stat.sum_delta_time[1]) / count;
	}
	return TRUE;
}

void pifTaskManager_ResetIdleMaxExecutionTime()
{
	s_idle_stat.max_execution_time = 0UL;
}

void pifTaskManager_GetMaxStatistics(PifTaskMaxStatistics *p_stat, const PifTask *p_exclude)
{
	PifObjArrayIterator it;
	PifTask *p_task;
	PifTaskTimer *p_timer;
	uint32_t delay;

	p_stat->max_execution_time = 0UL;
	p_stat->max_execution_id = 0;
	p_stat->max_delay = 0UL;
	p_stat->max_delay_id = 0;
	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		p_task = (PifTask *)it->data;
		if (p_task != p_exclude) {
			if (p_task->_max_execution_time > p_stat->max_execution_time) {
				p_stat->max_execution_time = p_task->_max_execution_time;
				p_stat->max_execution_id = p_task->_id;
			}
			delay = pifTask_GetMaxDelay(p_task);
			if (delay > p_stat->max_delay) {
				p_stat->max_delay = delay;
				p_stat->max_delay_id = p_task->_id;
			}
		}
		it = pifObjArray_Next(it);
	}

	p_stat->max_timer_time = 0UL;
	it = pifObjArray_Begin(&s_timers);
	while (it) {
		p_timer = (PifTaskTimer *)it->data;
		if (p_timer->_block_time._max > p_stat->max_timer_time) {
			p_stat->max_timer_time = p_timer->_block_time._max;
		}
		it = pifObjArray_Next(it);
	}
}

void pifTaskManager_ResetMaxStatistics()
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		pifTask_ResetMaxStatistics((PifTask *)it->data);
		it = pifObjArray_Next(it);
	}
}

#endif

#ifdef PIF_USE_TASK_STATISTICS_VCD

enum {
	TV_VERSION,
	TV_TIMESCALE,
	TV_SCOPE,
	TV_TIME,
	TV_DUMPVARS,
	TV_VALUE,
	TV_DUMPVARS_END,
	TV_FOOTER,
	TV_DONE
};

// The system scope first, then one scope of the same signals for each task.
static const char * const c_vcd_system[] = {
	"load",
#ifdef PIF_USE_TASK_REALTIME
	"rt_max_delay", "rt_miss", "rt_guard", "rt_lapse",
#endif
	"idle_max", "idle_average", "idle_delta"
};

static const char * const c_vcd_task[] = {
	"PrcM", "PrcA", "Delta", "Dly", "TrgM", "Rate", "Blk"
};

#define TV_SYSTEM_SIGNALS	(sizeof(c_vcd_system) / sizeof(c_vcd_system[0]))
#define TV_TASK_SIGNALS		(sizeof(c_vcd_task) / sizeof(c_vcd_task[0]))

// The value of a signal in a record. FALSE for one that was not taken or not measured yet, which
// the file shows as x.
static BOOL _vcdValue(const PifTaskStatisticsRecord *p_record, uint16_t signal, uint32_t *p_value)
{
	const PifTaskStatistics *p_stat;
	uint32_t value;

	if (signal == 0) {
		value = p_record->load;
	}
#ifdef PIF_USE_TASK_REALTIME
	else if (signal < 5) {
		if (!p_record->p_realtime) return FALSE;
		switch (signal) {
		case 1: value = p_record->p_realtime->max_delay; break;
		case 2: value = p_record->p_realtime->miss_count; break;
		case 3: value = p_record->p_realtime->guard; break;
		default: value = p_record->p_realtime->lapse_count; break;
		}
	}
#endif
	else if (signal < TV_SYSTEM_SIGNALS) {
		if (!p_record->p_idle) return FALSE;
		switch (TV_SYSTEM_SIGNALS - signal) {
		case 3: value = p_record->p_idle->max_execution_time; break;
		case 2: value = p_record->p_idle->average_execution_time; break;
		default: value = p_record->p_idle->average_delta_time; break;
		}
	}
	else {
		signal -= TV_SYSTEM_SIGNALS;
		if (signal / TV_TASK_SIGNALS >= p_record->task_count) return FALSE;
		p_stat = &p_record->p_task[signal / TV_TASK_SIGNALS];
		switch (signal % TV_TASK_SIGNALS) {
		case 0: value = p_stat->max_execution_time; break;
		case 1: value = p_stat->average_execution_time; break;
		case 2: value = p_stat->average_delta_time; break;
		case 3: value = p_stat->max_delay; break;
		case 4: value = p_stat->max_trigger_delay; break;
		case 5: value = p_stat->execution_rate; break;
		default: value = p_stat->block_time; break;
		}
	}
	if (value == PIF_TASK_AVERAGE_NONE) return FALSE;
	*p_value = value;
	return TRUE;
}

// The identifier of a signal, of letters and digits only: '$' and '#' are allowed by the format
// but read as a keyword or a time by some viewers. One character covers 62 signals, two the rest.
static const char *_vcdCode(uint16_t signal, char *p_code)
{
	static const char c_code[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const uint16_t size = sizeof(c_code) - 1;

	if (signal < size) {
		p_code[0] = c_code[signal];
		p_code[1] = 0;
	}
	else {
		p_code[0] = c_code[signal / size - 1];
		p_code[1] = c_code[signal % size];
		p_code[2] = 0;
	}
	return p_code;
}

// The name of the task with the ID that runs now, as the records keep only the ID.
static const char *_vcdTaskName(PifId id)
{
	PifObjArrayIterator it;

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		if (((PifTask *)it->data)->_id == id) return ((PifTask *)it->data)->name;
		it = pifObjArray_Next(it);
	}
	return NULL;
}

BOOL pifTaskManager_StartVcd(PifTaskVcd *p_owner, uint16_t count, PifTaskVcdGetRecord get_record, void *p_client,
		const char *p_version, const char *p_timescale)
{
	if (!count || !get_record || !p_version || !p_timescale) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTaskVcd));
	p_owner->__get_record = get_record;
	p_owner->__p_client = p_client;
	p_owner->__p_version = p_version;
	p_owner->__p_timescale = p_timescale;
	p_owner->__count = count;
	if (!(*get_record)(0, &p_owner->__first, p_client)) {
		p_owner->__stage = TV_DONE;
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	p_owner->__stage = TV_VERSION;
	return TRUE;
}

uint16_t pifTaskManager_GetVcdLine(PifTaskVcd *p_owner, char *p_line, uint16_t size)
{
	uint16_t signal_count = TV_SYSTEM_SIGNALS + p_owner->__first.task_count * TV_TASK_SIGNALS;
	uint16_t count, signal;
	uint32_t value, previous;
	BOOL known, was_known;
	const char *p_name;
	char code[3];

	if (size < PIF_TASK_VCD_LINE_SIZE) {
		pif_error = E_INVALID_PARAM;
		return 0;
	}

	p_line[0] = 0;
	while (!p_line[0]) {
		switch (p_owner->__stage) {
		case TV_VERSION:
			pif_Printf(p_line, size, "$version %s $end\n", p_owner->__p_version);
			p_owner->__stage = TV_TIMESCALE;
			break;

		case TV_TIMESCALE:
			pif_Printf(p_line, size, "$timescale %s $end\n", p_owner->__p_timescale);
			p_owner->__stage = TV_SCOPE;
			p_owner->__scope = -1;
			p_owner->__signal = 0;
			break;

		// __signal counts the lines of a scope: the $scope line, its signals, then $upscope.
		case TV_SCOPE:
			if (p_owner->__scope >= p_owner->__first.task_count) {
				pif_Printf(p_line, size, "$enddefinitions $end\n");
				p_owner->__stage = TV_TIME;
				p_owner->__record = 0;
				break;
			}
			count = (p_owner->__scope < 0) ? TV_SYSTEM_SIGNALS : TV_TASK_SIGNALS;
			if (p_owner->__signal == 0) {
				if (p_owner->__scope < 0) {
					pif_Printf(p_line, size, "$scope module system $end\n");
				}
				else {
					p_name = _vcdTaskName(p_owner->__first.p_task[p_owner->__scope].id);
					if (p_name) {
						pif_Printf(p_line, size, "$scope module %s $end\n", p_name);
					}
					else {
						pif_Printf(p_line, size, "$scope module task%u $end\n",
								(unsigned int)p_owner->__first.p_task[p_owner->__scope].id);
					}
				}
				p_owner->__signal++;
			}
			else if (p_owner->__signal <= count) {
				signal = p_owner->__signal - 1;
				if (p_owner->__scope >= 0) {
					pif_Printf(p_line, size, "$var integer 32 %s %s $end\n",
							_vcdCode(TV_SYSTEM_SIGNALS + p_owner->__scope * TV_TASK_SIGNALS + signal, code),
							c_vcd_task[signal]);
				}
				else {
					pif_Printf(p_line, size, "$var integer 32 %s %s $end\n", _vcdCode(signal, code),
							c_vcd_system[signal]);
				}
				p_owner->__signal++;
			}
			else {
				pif_Printf(p_line, size, "$upscope $end\n");
				p_owner->__scope++;
				p_owner->__signal = 0;
			}
			break;

		case TV_TIME:
			if (p_owner->__record >= p_owner->__count) {
				p_owner->__stage = TV_FOOTER;
				break;
			}
			if (p_owner->__record) p_owner->__previous = p_owner->__current;
			if (!(*p_owner->__get_record)(p_owner->__record, &p_owner->__current, p_owner->__p_client)) {
				// The rest is gone, so the file ends with what it has.
				p_owner->__count = p_owner->__record;
				p_owner->__stage = TV_FOOTER;
				break;
			}
			pif_Printf(p_line, size, "#%lu\n", (unsigned long)p_owner->__current.time);
			p_owner->__stage = p_owner->__record ? TV_VALUE : TV_DUMPVARS;
			p_owner->__signal = 0;
			break;

		case TV_DUMPVARS:
			pif_Printf(p_line, size, "$dumpvars\n");
			p_owner->__stage = TV_VALUE;
			break;

		case TV_VALUE:
			if (p_owner->__signal >= signal_count) {
				p_owner->__stage = p_owner->__record ? TV_TIME : TV_DUMPVARS_END;
				p_owner->__record++;
				break;
			}
			signal = p_owner->__signal++;
			value = previous = 0UL;
			known = _vcdValue(&p_owner->__current, signal, &value);
			// $dumpvars gives every value, and after it only a change is written.
			if (p_owner->__record) {
				was_known = _vcdValue(&p_owner->__previous, signal, &previous);
				if (known == was_known && value == previous) break;
			}
			if (known) {
				pif_Printf(p_line, size, "b%lb %s\n", (unsigned long)value, _vcdCode(signal, code));
			}
			else {
				pif_Printf(p_line, size, "bx %s\n", _vcdCode(signal, code));
			}
			break;

		case TV_DUMPVARS_END:
			pif_Printf(p_line, size, "$end\n");
			p_owner->__stage = TV_TIME;
			break;

		case TV_FOOTER:
			// A comment is allowed among the value changes, and tells a whole file from a cut one.
			pif_Printf(p_line, size, "$comment end of %u records $end\n", (unsigned int)p_owner->__count);
			p_owner->__stage = TV_DONE;
			break;

		default:
			return 0;
		}
	}
	return (uint16_t)strlen(p_line);
}

#endif

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

// The digits of value right-aligned in width columns, or a '-' in their place for 0. p_text has room
// for 12 characters, and width is at most 10.
static const char* _dashText(char* p_text, uint32_t value, int width)
{
	int len;

	if (value) {
		pif_Printf(p_text, 12, "%lu", (unsigned long)value);
	}
	else {
		p_text[0] = '-';
		p_text[1] = 0;
	}
	len = strlen(p_text);
	if (len < width) {
		memmove(p_text + width - len, p_text, len + 1);
		memset(p_text, ' ', width - len);
	}
	return p_text;
}

// One line a task, times in microseconds:
//   M      P: TM_PERIOD, E: TM_EXTERNAL, R: TM_REALTIME
//   Pa     1 while paused
//   Period default period
//   Blk    longest run over the window that the realtime task can be delayed by
//   PrcM   longest run since the statistics were reset, PrcA its moving average
//   Delta  average time between two releases by the default period, not kept for TM_EXTERNAL
//   TrgM   longest wait of a release by trigger, Dly longest lateness against the period
//   Rate   runs per second over the last CPU load window
// A '-' is a value that has not been measured.
void pifTaskManager_Print()
{
	PifObjArrayIterator it;
	// Longer than PIF_LOG_LINE_SIZE, so the lines are made here and given to pifLog_Print().
	char line[96];
	char period[12];
	char mode;
	int n;
#ifdef PIF_USE_BLOCK_TIME
	uint32_t block_time;
#endif
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t value, average;
	char delta[12], trigger[12], delay[12], rate[12];
	// Filled in whenever it is printed, but GCC cannot always tell through the inlined call.
	PifTaskIdleStatistics stat = { 0 };
#endif

	// The load comes first, so that it is not the line lost when the log buffer runs out.
#ifdef PIF_USE_ISR_STATISTICS
	// The interrupts taken during the tasks are in both values. With the share of all the interrupts go
	// how many there are a second and the longest one, the longest any of them holds everything else up.
	{
		PifTaskIsr *p_isr;
		uint32_t isr_rate = 0UL, isr_max = 0UL;

		for (p_isr = s_p_isr_first; p_isr; p_isr = p_isr->__p_next) {
			isr_rate += p_isr->_rate;
			if (p_isr->_max > isr_max) isr_max = p_isr->_max;
		}
		pifLog_Printf(LT_NONE, "Task count: %d, Load: %u%%, ISR: %u.%u%% %lu/s M=%luus (us)\n",
				pifObjArray_Count(&s_tasks), pif_performance._task_load, s_isr_load10 / 10, s_isr_load10 % 10,
				(unsigned long)isr_rate, (unsigned long)isr_max);
	}
#else
	pifLog_Printf(LT_NONE, "Task count: %d, Load: %u%% (us)\n", pifObjArray_Count(&s_tasks), pif_performance._task_load);
#endif
	// The headings are laid out to the widths of the columns below.
	pifLog_Print(LT_NONE, "  ID Name       M Pa  Period"
#ifdef PIF_USE_BLOCK_TIME
			"    Blk"
#endif
#ifdef PIF_USE_TASK_STATISTICS
			"   PrcM  PrcA   Delta   TrgM     Dly   Rate"
#endif
			"\n");

	it = pifObjArray_Begin(&s_tasks);
	while (it) {
		PifTask *p_owner = (PifTask *)it->data;

		switch (p_owner->_mode) {
#ifdef PIF_USE_TASK_REALTIME
			case TM_REALTIME: mode = 'R'; break;
#endif
			case TM_PERIOD: mode = 'P'; break;
			case TM_EXTERNAL: mode = 'E'; break;
	        default: mode = '-'; break;
		}
		// A value wider than its column takes the room it needs, which only shifts the rest of the line.
		pif_Printf(line, sizeof(line), " %3u %10.10s %c %2u %s", (unsigned int)p_owner->_id,
				p_owner->name ? p_owner->name : "---", mode, (unsigned int)p_owner->_pause,
				_dashText(period, p_owner->_default_period, 7));
		n = strlen(line);
#ifdef PIF_USE_BLOCK_TIME
		pif_Printf(line + n, sizeof(line) - n, " %6lu", (unsigned long)p_owner->_block_time._max);
		n += strlen(line + n);
#endif
#ifdef PIF_USE_TASK_STATISTICS
		// PrcA is the moving average over the last 100 to 200 runs, not an average since boot.
		value = p_owner->__sum_execution_time[0] + p_owner->__sum_execution_time[1];
		average = p_owner->__execution_count ? value / p_owner->__execution_count : 0;
		pif_Printf(line + n, sizeof(line) - n, " %6lu %5lu", (unsigned long)p_owner->_max_execution_time,
				(unsigned long)average);
		n += strlen(line + n);
		value = p_owner->__total_delta_time[0] + p_owner->__total_delta_time[1];
		// A release by trigger is not measured against the period, so it has a column of its own.
		pif_Printf(line + n, sizeof(line) - n, " %s %s %s %s",
				_dashText(delta, p_owner->__delta_count ? value / p_owner->__delta_count : 0, 7),
				_dashText(trigger, p_owner->_max_trigger_delay, 6), _dashText(delay, p_owner->_max_delay, 7),
				_dashText(rate, p_owner->_execution_rate, 6));
		n += strlen(line + n);
#endif
		line[n++] = '\n';
		line[n] = 0;
		pifLog_Print(LT_NONE, line);
		it = pifObjArray_Next(it);
	}

#ifdef PIF_USE_TASK_REALTIME
	if (g_realtime_task) {
		pifLog_Printf(LT_NONE, "RealTime: MaxDelay=%luus Miss=%lu\n", pif_performance._max_delay,
				pif_performance._miss_count);
#ifdef PIF_USE_BLOCK_TIME
		// A lapse is a run that was let through although it does not fit before the release, so
		// the guarantee did not hold for it. Either the run is longer than the whole period, in
		// which case it is the task whose block time above exceeds the period printed for the
		// realtime task, or a release had been held back as long as its bound allows and was let
		// through. With the default bound of one period only a run that never fits gets there,
		// while a count of passes in max_skip or PIF_TASK_MAX_SKIP can run out sooner on a slow
		// CPU, and raising it trades that jitter back for a longer wait.
		pifLog_Printf(LT_NONE, "          Guard=%luus Lapse=%lu\n", s_task_guard,
				s_guard_lapse_count);
#endif
	}
#endif

#ifdef PIF_USE_ISR_STATISTICS
	// One line an interrupt handler over the last CPU load window: calls per second, average and
	// longest call of the sample, and share of the window.
	if (s_p_isr_first) {
		PifTaskIsr *p_isr;

		pifLog_Print(LT_NONE, " ISR             Rate      A      M   Load\n");
		for (p_isr = s_p_isr_first; p_isr; p_isr = p_isr->__p_next) {
			pif_Printf(line, sizeof(line), " %10.10s %9lu %4lu.%lu %6lu %3u.%u%%\n",
					p_isr->name ? p_isr->name : "---", (unsigned long)p_isr->_rate,
					(unsigned long)(p_isr->_average10 / 10), (unsigned long)(p_isr->_average10 % 10),
					(unsigned long)p_isr->_max, p_isr->_load10 / 10, p_isr->_load10 % 10);
			pifLog_Print(LT_NONE, line);
		}
	}
#endif

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
#ifdef PIF_USE_TASK_STATISTICS
		// The timer callbacks together over the last CPU load window, counting only the calls that
		// did work: average call, calls per second and share of the load.
		pifLog_Printf(LT_NONE, "Callback block: Timer=%luus Idle=%luus, Timer A=%lu.%luus Rate=%lu (%u%%)\n",
				block_time, s_idle_block_time._max, s_timer_average10 / 10, s_timer_average10 % 10,
				s_timer_rate, s_timer_load);
#else
		pifLog_Printf(LT_NONE, "Callback block: Timer=%luus Idle=%luus\n", block_time,
				s_idle_block_time._max);
#endif
	}
#endif
#ifdef PIF_USE_TASK_STATISTICS
	if (s_idle_stat.count && pifTaskManager_GetIdleStatistics(&stat)) {
		pifLog_Printf(LT_NONE, "Idle: M=%luus A=%luus Delta=%luus\n", stat.max_execution_time,
				(stat.average_execution_time == PIF_TASK_AVERAGE_NONE) ? 0UL : stat.average_execution_time,
				(stat.average_delta_time == PIF_TASK_AVERAGE_NONE) ? 0UL : stat.average_delta_time);
	}
#endif

}

#endif	// PIF_NO_LOG
