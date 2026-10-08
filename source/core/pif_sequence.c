// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_sequence.h"

// Step-based sequence runner on a task of its own.


// How long the pending step leaves before it is due, or 0 when the sequence is idle.
static uint32_t _nextPoll(PifSequence *p_owner)
{
	uint32_t elapsed, remain;

	switch (p_owner->__state) {
	case SQS_RUN:
		return PIF_SEQUENCE_NEXT_US;

	case SQS_DELAY:
		elapsed = pif_cumulative_timer1ms - p_owner->__start_ms;
		if (elapsed >= p_owner->__wait_ms) return PIF_SEQUENCE_NEXT_US;
		return (p_owner->__wait_ms - elapsed) * 1000UL;

	case SQS_WAIT:
		// A signal raised since the wait began, even by an interrupt during the step that began
		// it, is picked up at once rather than at the next look.
		if (p_owner->__event) return PIF_SEQUENCE_NEXT_US;

		// Without a timeout only the signal is looked for. With one, whichever comes first.
		remain = PIF_SEQUENCE_WAIT_POLL_US;
		if (p_owner->__wait_ms) {
			elapsed = pif_cumulative_timer1ms - p_owner->__start_ms;
			if (elapsed >= p_owner->__wait_ms) return PIF_SEQUENCE_NEXT_US;
			if ((p_owner->__wait_ms - elapsed) * 1000UL < remain) remain = (p_owner->__wait_ms - elapsed) * 1000UL;
		}
		return remain;

	default:
		return 0;
	}
}

// Runs the pending step if it is due.
static void _poll(PifSequence *p_owner)
{
	// The subtraction stays correct across a wrap of the counter.
	uint32_t elapsed = pif_cumulative_timer1ms - p_owner->__start_ms;
	PifSequenceStep step = NULL;

	switch (p_owner->__state) {
	case SQS_RUN:
		step = p_owner->__step;
		break;

	case SQS_DELAY:
		if (elapsed >= p_owner->__wait_ms) step = p_owner->__step;
		break;

	case SQS_WAIT:
		// The signal is only looked at here, so one raised during a delay cannot cut it short.
		if (p_owner->__event) {
			p_owner->__event = FALSE;
			step = p_owner->__step;
		}
		else if (p_owner->__wait_ms && elapsed >= p_owner->__wait_ms) {
			step = p_owner->__on_timeout;
			if (!step) {
				pif_error = E_TIMEOUT;
				p_owner->__state = SQS_IDLE;
			}
		}
		break;
	}

	if (step) {
		// A step that picks no successor leaves the sequence idle, which ends it.
		p_owner->__state = SQS_IDLE;
		(*step)(p_owner);
	}
}

static uint32_t _doTask(PifTask *p_task)
{
	PifSequence *p_owner = (PifSequence *)p_task->_p_client;
	uint32_t delay;

	_poll(p_owner);
	delay = _nextPoll(p_owner);
	// Nothing to run until the sequence is started again.
	if (!delay) p_task->pause = TRUE;
	return delay;
}

BOOL pifSequence_Init(PifSequence *p_owner, PifId id, void *p_param)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSequence));
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->p_param = p_param;

	// Kept until pifSequence_Clear() and paused in between, rather than removed when the
	// sequence ends: the end is reached inside this task's own run, and the task manager still
	// uses the task after the run returns.
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, PIF_SEQUENCE_WAIT_POLL_US, _doTask, p_owner, FALSE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "Sequence";
	return TRUE;
}

void pifSequence_Clear(PifSequence *p_owner)
{
	p_owner->__state = SQS_IDLE;
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifSequence_Start(PifSequence *p_owner, PifSequenceStep step)
{
	if (!step) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->__state != SQS_IDLE || !p_owner->_p_task) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	pifSequence_Next(p_owner, step);

	// The period may still be a long delay left from before a stop, so it is set back to the
	// shortest one the task keeps.
	pifTask_ChangePeriod(p_owner->_p_task, PIF_SEQUENCE_WAIT_POLL_US);
	p_owner->_p_task->pause = FALSE;
	return TRUE;
}

void pifSequence_Stop(PifSequence *p_owner)
{
	p_owner->__state = SQS_IDLE;
	if (p_owner->_p_task) p_owner->_p_task->pause = TRUE;
}

BOOL pifSequence_IsRunning(PifSequence *p_owner)
{
	return p_owner->__state != SQS_IDLE;
}

void pifSequence_Next(PifSequence *p_owner, PifSequenceStep next)
{
	p_owner->__step = next;
	p_owner->__state = SQS_RUN;
}

void pifSequence_Delay(PifSequence *p_owner, PifSequenceStep next, uint16_t delay1ms)
{
	p_owner->__step = next;
	p_owner->__start_ms = pif_cumulative_timer1ms;
	p_owner->__wait_ms = delay1ms;
	p_owner->__state = SQS_DELAY;
}

void pifSequence_Wait(PifSequence *p_owner, PifSequenceStep next, uint16_t timeout1ms, PifSequenceStep on_timeout)
{
	p_owner->__step = next;
	p_owner->__on_timeout = on_timeout;
	p_owner->__start_ms = pif_cumulative_timer1ms;
	p_owner->__wait_ms = timeout1ms;
	p_owner->__event = FALSE;
	p_owner->__state = SQS_WAIT;
}

void pifSequence_Signal(PifSequence *p_owner)
{
	p_owner->__event = TRUE;
}
