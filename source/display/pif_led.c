// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_led.h"


static uint32_t _allBits(PifLed* p_owner)
{
	return (p_owner->count == 32) ? 0xFFFFFFFFUL : (1UL << p_owner->count) - 1;
}

static uint32_t _doTask(PifTask* p_task)
{
    PifLed* p_owner = (PifLed*)p_task->_p_client;
	PifLedBlink* p_blink;
	uint8_t b;

	// A single blink is a profile with a multiple of 1, which toggles at every run.
    for (b = 0; b < p_owner->__blink_count; b++) {
    	p_blink = &p_owner->__p_blinks[b];
    	if (p_blink->count) {
    		p_blink->count--;
    		continue;
    	}
		p_blink->count = p_blink->multiple - 1;

		p_blink->state ^= 1;
		// pifLed_MBlinkOn() keeps the flags within count.
		if (p_blink->state) {
			p_owner->__state |= p_blink->flag;
		}
		else {
			p_owner->__state &= ~p_blink->flag;
		}
    }
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
	return 0;
}

// The task runs only while an LED blinks. When it resumes, every profile starts over in the on
// phase and toggles after a full multiple of the period.
static void _updatePause(PifLed* p_owner)
{
	PifTask* p_task = p_owner->_p_task;
	PifLedBlink* p_blink;
	BOOL pause = TRUE;
	uint8_t b;

	if (!p_task) return;

	for (b = 0; b < p_owner->__blink_count; b++) {
		if (p_owner->__p_blinks[b].flag) {
			pause = FALSE;
			break;
		}
	}

	if (p_task->_pause && !pause) {
		for (b = 0; b < p_owner->__blink_count; b++) {
			p_blink = &p_owner->__p_blinks[b];
			p_blink->state = ON;
			p_blink->count = p_blink->multiple - 1;
		}
		// Counts the period from now. A paused task would otherwise run at once.
		pifTask_Restart(p_task);
	}
	else if (pause) {
		pifTask_Pause(p_task);
	}
}

BOOL pifLed_Init(PifLed* p_owner, PifId id, uint8_t count, PifActLedState act_state)
{
	if (!p_owner || !count || count > 32 || !act_state) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifLed));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;
    p_owner->__act_state = act_state;
    return TRUE;
}

void pifLed_Clear(PifLed* p_owner)
{
	pifLed_DetachBlink(p_owner);
}

// Bits past count are dropped wherever the state is set, so that the callback never sees them.
void pifLed_PartOn(PifLed* p_owner, uint32_t bits)
{
	p_owner->__state |= bits & _allBits(p_owner);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartOff(PifLed* p_owner, uint32_t bits)
{
	p_owner->__state &= ~bits;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartChange(PifLed* p_owner, uint32_t bits, SWITCH state)
{
	if (state) {
		p_owner->__state |= bits & _allBits(p_owner);
	}
	else {
		p_owner->__state &= ~bits;
	}
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartToggle(PifLed* p_owner, uint32_t bits)
{
	p_owner->__state ^= bits & _allBits(p_owner);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllOn(PifLed* p_owner)
{
	p_owner->__state = _allBits(p_owner);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllOff(PifLed* p_owner)
{
	p_owner->__state = 0;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllChange(PifLed* p_owner, uint32_t state)
{
	p_owner->__state = state & _allBits(p_owner);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllToggle(PifLed* p_owner)
{
	p_owner->__state ^= _allBits(p_owner);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

BOOL pifLed_AttachSBlink(PifLed* p_owner, uint16_t period1ms)
{
	return pifLed_AttachMBlink(p_owner, period1ms, 1, 1);
}

BOOL pifLed_AttachMBlink(PifLed* p_owner, uint16_t period1ms, uint8_t count, ...)
{
	va_list ap;
	PifLedBlink* p_blinks;
	int b;

	if (!period1ms || !count) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	// The new profile is built aside, so that a failure leaves the current blink untouched.
    p_blinks = calloc(count, sizeof(PifLedBlink));
    if (!p_blinks) {
    	pif_error = E_OUT_OF_HEAP;
    	return FALSE;
    }

	va_start(ap, count);
	for (b = 0; b < count; b++) {
		p_blinks[b].multiple = va_arg(ap, int);
		if (!p_blinks[b].multiple) break;
	}
	va_end(ap);
	if (b < count) {
		free(p_blinks);
        pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->_p_task) {
		// Paused until an LED blinks.
		p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1ms * 1000UL, _doTask, p_owner, FALSE);
		if (!p_owner->_p_task) {
			free(p_blinks);
			return FALSE;
		}
	}
	else {
		// A non-zero period cannot fail.
		pifTask_ChangePeriod(p_owner->_p_task, period1ms * 1000UL);
	}

    if (p_owner->__p_blinks) free(p_owner->__p_blinks);
    p_owner->__p_blinks = p_blinks;
    p_owner->__blink_count = count;
    _updatePause(p_owner);
	return TRUE;
}

void pifLed_DetachBlink(PifLed* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}

    if (p_owner->__p_blinks) {
		free(p_owner->__p_blinks);
		p_owner->__p_blinks = NULL;
    }
	p_owner->__blink_count = 0;
}

BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->_p_task) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	return pifTask_ChangePeriod(p_owner->_p_task, period1ms * 1000UL);
}

void pifLed_SBlinkOn(PifLed* p_owner, uint32_t bits)
{
	pifLed_MBlinkOn(p_owner, bits, 0);
}

void pifLed_MBlinkOn(PifLed* p_owner, uint32_t bits, uint8_t index)
{
	uint8_t b;

    if (!p_owner->__p_blinks || index >= p_owner->__blink_count) return;

    bits &= _allBits(p_owner);
    for (b = 0; b < p_owner->__blink_count; b++) {
    	if (b == index) {
    		p_owner->__p_blinks[b].flag |= bits;
    	}
    	else {
    		p_owner->__p_blinks[b].flag &= ~bits;
    	}
    }
    _updatePause(p_owner);

    // The LEDs join the profile in its current phase instead of waiting for its next toggle.
    pifLed_PartChange(p_owner, bits, p_owner->__p_blinks[index].state);
}

void pifLed_SBlinkOff(PifLed* p_owner, uint32_t bits, SWITCH state)
{
	pifLed_MBlinkOff(p_owner, bits, 0, state);
}

void pifLed_MBlinkOff(PifLed* p_owner, uint32_t bits, uint8_t index, SWITCH state)
{
	uint32_t others = 0;
	uint8_t b;

    if (!p_owner->__p_blinks || index >= p_owner->__blink_count) return;

	p_owner->__p_blinks[index].flag &= ~bits;
	for (b = 0; b < p_owner->__blink_count; b++) {
		if (b != index) others |= p_owner->__p_blinks[b].flag;
	}
	_updatePause(p_owner);

	// An LED that blinks in another profile keeps blinking there, so its state is left to it.
	pifLed_PartChange(p_owner, bits & ~others, state);
}
