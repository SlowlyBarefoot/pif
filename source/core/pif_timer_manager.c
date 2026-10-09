// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_timer_manager.h"

// Timer manager task and timer object lifecycle handling.
//
// The list of timers is changed only here in the main context: pifTimerManager_Add() links a
// timer and the task unlinks the ones marked for removal. The tick handler never follows the list.
// It walks the fixed storage of the array instead, where a slot that is not in the list holds a
// stopped timer, so neither side has to lock the other out.

static void _finish(PifTimerManager *p_manager, PifTimer *p_timer)
{
	if (p_timer->__event_into_int) {
		if (p_timer->__evt_finish) (*p_timer->__evt_finish)(p_timer->__p_finish_issuer);
	}
	else if (!p_timer->__event) {
		p_timer->__event = TRUE;
		p_manager->__pending = TRUE;
	}
}

static void _doTask(void *p_client)
{
	PifTimerManager *p_manager = (PifTimerManager *)p_client;
	PifObjArrayIterator it, it_next;

	if (!p_manager->__pending) return;
	p_manager->__pending = FALSE;

	it = pifObjArray_Begin(&p_manager->__timers);
	while (it) {
		PifTimer *p_timer = (PifTimer *)it->data;

		// A callback can only mark timers for removal, and only this loop unlinks them, so the
		// next node is still in the list when it is reached.
		it_next = pifObjArray_Next(it);

		if (p_timer->_step == TS_REMOVE) {
			// Leaves the slot stopped, so that the tick handler passes over it until it is reused.
			p_timer->_step = TS_STOP;
			pifObjArray_Remove(&p_manager->__timers, p_timer);
		}
		else if (p_timer->__event) {
			p_timer->__event = FALSE;

			if (p_timer->__evt_finish) (*p_timer->__evt_finish)(p_timer->__p_finish_issuer);
		}

		it = it_next;
	}
}

BOOL pifTimerManager_Init(PifTimerManager *p_manager, PifId id, uint32_t period1us, int max_count)
{
    if (!p_manager || !period1us) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_manager, 0, sizeof(PifTimerManager));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_manager->_id = id;
    if (!pifObjArray_Init(&p_manager->__timers, sizeof(PifTimer), max_count, NULL)) goto fail;
    p_manager->_period1us = period1us;

    p_manager->__p_task = pifTaskManager_AddTimer(_doTask, p_manager);
    if (!p_manager->__p_task) goto fail;
    p_manager->__ready = TRUE;
    return TRUE;

fail:
	pifTimerManager_Clear(p_manager);
    return FALSE;
}

void pifTimerManager_Clear(PifTimerManager *p_manager)
{
	PifObjArrayIterator it;

	// Stops every timer, including those marked for removal, so that a PWM output is left off.
	it = pifObjArray_Begin(&p_manager->__timers);
	while (it) {
		PifTimer *p_timer = (PifTimer *)it->data;

		p_timer->_step = TS_STOP;
		pifTimer_Stop(p_timer);
		p_timer->__event = FALSE;
		it = pifObjArray_Next(it);
	}

	// From here on the tick handler does not touch the storage, so it can be freed. A handler that
	// had already started finished before this line, since it cannot be interrupted by this context.
	p_manager->__ready = FALSE;

	if (p_manager->__p_task) {
		pifTaskManager_RemoveTimer(p_manager->__p_task);
		p_manager->__p_task = NULL;
	}
	pifObjArray_Clear(&p_manager->__timers);
}

PifTimer *pifTimerManager_Add(PifTimerManager *p_manager, PifTimerType type)
{
	// The slot comes back cleared, and a cleared timer is a stopped one.
	PifObjArrayIterator it = pifObjArray_Add(&p_manager->__timers);
    if (!it) return NULL;

	PifTimer *p_timer = (PifTimer *)it->data;
    p_timer->_type = type;
    return p_timer;
}

void pifTimerManager_Remove(PifTimer *p_timer)
{
	p_timer->_step = TS_REMOVE;
}

int pifTimerManager_Count(PifTimerManager *p_manager)
{
	return pifObjArray_Count(&p_manager->__timers);
}

void pifTimerManager_sigTick(PifTimerManager *p_manager)
{
	char *p_slot;
	int i, max_count, stride;
	uint32_t current, target, duty;

	if (!p_manager || !p_manager->__ready) return;

	// Each slot is laid out as pifObjArray_Init() does it: the two links of the node, then the data.
	p_slot = (char *)p_manager->__timers._p_node->data;
	stride = 2 * sizeof(PifObjArrayIterator) + p_manager->__timers._size;
	max_count = p_manager->__timers._max_count;
	for (i = 0; i < max_count; i++, p_slot += stride) {
		PifTimer *p_timer = (PifTimer *)p_slot;

		if (p_timer->_step != TS_RUNNING) {
			// The task frees the slot.
			if (p_timer->_step == TS_REMOVE) p_manager->__pending = TRUE;
			continue;
		}

		current = p_timer->__current;
		if (!current) continue;
		current--;

		// __current is stored before any callback, since one may restart or stop the timer.
		switch (p_timer->_type) {
		case TT_ONCE:
			p_timer->__current = current;
			if (!current) {
				p_timer->_step = TS_STOP;
				_finish(p_manager, p_timer);
			}
			break;

		case TT_REPEAT:
			if (!current) {
				p_timer->__current = p_timer->_target;
				_finish(p_manager, p_timer);
			}
			else {
				p_timer->__current = current;
			}
			break;

		case TT_PWM:
			target = p_timer->_target;
			duty = p_timer->__pwm_duty;
			if (!current) {
				current = target;
				if (duty != target && p_timer->act_pwm) (*p_timer->act_pwm)(OFF);
			}
			p_timer->__current = current;
			if (duty != target && current == duty && p_timer->act_pwm) (*p_timer->act_pwm)(ON);
			break;
		}
	}
}
