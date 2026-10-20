// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_fnd.h"


const uint8_t kFndNumber[] = {
		0x3F, /*  0  */	0x06, /*  1  */	0x5B, /*  2  */ 0x4F, /*  3  */ 	// 0x30
		0x66, /*  4  */ 0x6D, /*  5  */ 0x7D, /*  6  */ 0x07, /*  7  */		// 0x34
		0x7F, /*  8  */ 0x6F  /*  9  */ 									// 0x38
};

const uint8_t *c_user_char;
static uint8_t s_user_char_count = 0;


static void _outputDigit(PifFnd* p_owner, uint8_t digit)
{
	uint8_t ch, seg = 0;
	BOOL point = FALSE;

	// A stopped display in a scan group keeps its turn dark, so that the others are not brightened.
	if (!p_owner->__bt.run) {
		(*p_owner->__act_display)(0, digit);
		return;
	}

	if (p_owner->__bt.blink && pifTask_IsSubPeriodElapsed(&p_owner->__blink_time_us, p_owner->__blink_period_1ms * 1000UL)) {
		p_owner->__bt.led ^= 1;
	}

	if (p_owner->__bt.led && p_owner->__bt.raw) {
		seg = p_owner->__p_string[digit];
	}
	else if (p_owner->__bt.led) {
		ch = p_owner->__p_string[digit];
		if (ch & 0x80) {
			point = TRUE;
			ch &= 0x7F;
		}
		if (ch >= '0' && ch <= '9') {
			seg = kFndNumber[ch - '0'];
		}
		else if (ch == '-') {
			seg = 0x40;
		}
		else if (ch == '_') {
			seg = 0x08;
		}
		else if (s_user_char_count && ch >= 'A' && ch < 'A' + s_user_char_count) {
			seg = c_user_char[ch - 'A'];
		}
		if (point) seg |= 0x80;
	}
	(*p_owner->__act_display)(seg, digit);
}

static uint32_t _doTask(PifTask* p_task)
{
	PifFnd* p_owner = p_task->_p_client;
	PifFnd* p_scan = p_owner->__p_scan;

	_outputDigit(p_scan, p_owner->__digit_index);
	p_owner->__digit_index++;
	if (p_owner->__digit_index >= p_scan->_digit_size) {
		p_owner->__digit_index = 0;
		p_owner->__p_scan = p_scan->__p_next ? p_scan->__p_next : p_owner;
	}
	return 0;
}

static PifFnd* _getScanner(PifFnd* p_owner)
{
	return p_owner->__p_leader ? p_owner->__p_leader : p_owner;
}

// Spreads the frame period of a leader over all digits of its group.
static BOOL _updatePeriod(PifFnd* p_leader)
{
	uint16_t digits = 0;
	PifFnd* p_fnd;

	// A member left without a task by a failed _addTask() keeps the period for nothing.
	if (!p_leader->__p_task) return TRUE;

	for (p_fnd = p_leader; p_fnd; p_fnd = p_fnd->__p_next) digits += p_fnd->_digit_size;
	return pifTask_ChangePeriod(p_leader->__p_task, p_leader->__period_per_digit_1ms * 1000UL / digits);
}

// The task of a leader runs while any display of its group runs.
static void _updatePause(PifFnd* p_leader)
{
	PifFnd* p_fnd;

	if (!p_leader->__p_task) return;

	for (p_fnd = p_leader; p_fnd; p_fnd = p_fnd->__p_next) {
		if (p_fnd->__bt.run) {
			pifTask_Resume(p_leader->__p_task);
			return;
		}
	}
	pifTask_Pause(p_leader->__p_task);
}

// Starts a new frame of a leader, after its group has changed.
static void _restartScan(PifFnd* p_leader)
{
	p_leader->__p_scan = p_leader;
	p_leader->__digit_index = 0;
	_updatePeriod(p_leader);
	_updatePause(p_leader);
}

// Gives a display a scan task of its own, paused until it runs.
static BOOL _addTask(PifFnd* p_owner)
{
	p_owner->__p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, p_owner->__period_per_digit_1ms * 1000UL / p_owner->_digit_size,
			_doTask, p_owner, FALSE);
	if (!p_owner->__p_task) return FALSE;
	p_owner->__p_task->name = "FND";
	return TRUE;
}

static uint8_t _getSubDigits(PifFnd* p_owner)
{
	// Keep at least one integer digit so the decimal point stays inside the display.
	if (p_owner->sub_numeric_digits >= p_owner->_digit_size) return p_owner->_digit_size - 1;
	return p_owner->sub_numeric_digits;
}

static void _setNumber(PifFnd* p_owner, uint32_t integer, uint32_t fraction, uint8_t sub, BOOL minus)
{
	char* p_str = p_owner->__p_string;
	int last = p_owner->_digit_size - 1;
	int point = last - sub;		// Position of the last integer digit
	int p;

	p_owner->__bt.raw = FALSE;
	for (p = last; p > point; p--) {
		p_str[p] = '0' + fraction % 10;
		fraction /= 10;
	}
	for (; p >= 0; p--) {
		if (p < point) {
			if (p_owner->__bt.fill_zero) {
				if (minus && p == 0) break;
			}
			else if (!integer) break;
		}
		p_str[p] = '0' + integer % 10;
		integer /= 10;
	}
	if (sub) p_str[point] |= 0x80;
	if (minus) {
		if (p >= 0) p_str[p--] = '-';
		else integer = 1;		// No room for the sign
	}
	for (; p >= 0; p--) p_str[p] = 0x20;
	if (integer) p_str[last] = '_';
}


void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count)
{
	c_user_char = p_user_char;
	s_user_char_count = count;
}

BOOL pifFnd_Init(PifFnd* p_owner, PifId id, uint8_t digit_size, PifActFndDisplay act_display)
{
    int i;

    if (!p_owner || !digit_size || !act_display) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifFnd));

    p_owner->__p_string = calloc(digit_size, sizeof(uint8_t));
    if (!p_owner->__p_string) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}
    for (i = 0; i < digit_size; i++) p_owner->__p_string[i] = 0x20;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__bt.led = ON;
    p_owner->_digit_size = digit_size;
    p_owner->__act_display = act_display;
    p_owner->__period_per_digit_1ms = PIF_FND_PERIOD_PER_DIGIT;
    p_owner->__p_scan = p_owner;

    if (!_addTask(p_owner)) goto fail;
    return TRUE;

fail:
	pifFnd_Clear(p_owner);
    return FALSE;
}

void pifFnd_Clear(PifFnd* p_owner)
{
	PifFnd* p_fnd;
	PifFnd* p_next;

	if (p_owner->__p_leader) {
		// A member leaves the group of its leader.
		p_fnd = p_owner->__p_leader;
		while (p_fnd->__p_next != p_owner) p_fnd = p_fnd->__p_next;
		p_fnd->__p_next = p_owner->__p_next;
		_restartScan(p_owner->__p_leader);
	}
	else if (p_owner->__p_next) {
		// The members of a leader go back to scanning themselves, each with a task of its own again.
		// The task of the leader is released first, so that there is room for one of them.
		if (p_owner->__p_task) {
			pifTaskManager_Remove(p_owner->__p_task);
			p_owner->__p_task = NULL;
		}
		for (p_fnd = p_owner->__p_next; p_fnd; p_fnd = p_next) {
			p_next = p_fnd->__p_next;
			p_fnd->__p_leader = NULL;
			p_fnd->__p_next = NULL;
			// Without a task the member stays dark until it is cleared and initialized again.
			if (_addTask(p_fnd)) _restartScan(p_fnd);
		}
	}
	p_owner->__p_leader = NULL;
	p_owner->__p_next = NULL;
	p_owner->__p_scan = p_owner;

	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	if (p_owner->__p_string) {
		free(p_owner->__p_string);
		p_owner->__p_string = NULL;
	}
	p_owner->__bt.blink = FALSE;
	p_owner->__bt.run = FALSE;
}

BOOL pifFnd_ShareScan(PifFnd* p_owner, PifFnd* p_leader)
{
	PifFnd* p_fnd;

	if (!p_owner || !p_leader) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_leader = _getScanner(p_leader);
	if (p_owner == p_leader || p_owner->__p_leader || p_owner->__p_next || !p_leader->__p_task) {
        pif_error = E_INVALID_STATE;
        return FALSE;
	}

	for (p_fnd = p_leader; p_fnd->__p_next; p_fnd = p_fnd->__p_next);
	p_fnd->__p_next = p_owner;
	p_owner->__p_leader = p_leader;
	// The leader scans it from now on, so its own task is released. A member left without a task
	// by the clear of its leader has none to release.
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	_restartScan(p_leader);
	return TRUE;
}

uint16_t pifFnd_GetPeriodPerDigit(PifFnd* p_owner)
{
	return _getScanner(p_owner)->__period_per_digit_1ms;
}

BOOL pifFnd_SetPeriodPerDigit(PifFnd* p_owner, uint16_t period1ms)
{
	PifFnd* p_leader = _getScanner(p_owner);

	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_leader->__period_per_digit_1ms = period1ms;
	return _updatePeriod(p_leader);
}

void pifFnd_Start(PifFnd* p_owner)
{
	p_owner->__bt.run = TRUE;
	_updatePause(_getScanner(p_owner));
}

void pifFnd_Stop(PifFnd* p_owner)
{
	int i;

	for (i = 0; i < p_owner->_digit_size; i++) {
		(*p_owner->__act_display)(0, i);
	}
	p_owner->__bt.run = FALSE;
	_updatePause(_getScanner(p_owner));
	p_owner->__bt.blink = FALSE;
    // Blink may have been stopped in its off phase.
	p_owner->__bt.led = ON;
}

BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__blink_period_1ms = period1ms;
	p_owner->__blink_time_us = (*pif_act_timer1us)();
	p_owner->__bt.blink = TRUE;
    return TRUE;
}

void pifFnd_BlinkOff(PifFnd* p_owner)
{
	p_owner->__bt.led = ON;
	p_owner->__bt.blink = FALSE;
}

BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__bt.blink) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__blink_period_1ms = period1ms;
	return TRUE;
}

void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero)
{
    p_owner->__bt.fill_zero = fill_zero;
}

void pifFnd_SetFloat(PifFnd* p_owner, double value)
{
	BOOL minus = FALSE;
	uint8_t sub = _getSubDigits(p_owner);
	int i;
	uint32_t integer, fraction, scale = 1;

	if (sub > 9) sub = 9;		// 10^sub must fit in uint32_t
	for (i = 0; i < sub; i++) scale *= 10;

	if (value < 0.0) {
		minus = TRUE;
		value = -value;
	}
	if (!(value < 4294967295.0)) {		// Also catches NaN
		integer = 0xFFFFFFFFUL;
		fraction = 0;
	}
	else {
		integer = (uint32_t)value;
		fraction = (uint32_t)((value - integer) * scale + 0.5);
		if (fraction >= scale) {
			fraction -= scale;
			integer++;
		}
	}
	if (!integer && !fraction) minus = FALSE;
	_setNumber(p_owner, integer, fraction, sub, minus);
}

void pifFnd_SetInteger(PifFnd* p_owner, int32_t value)
{
	uint32_t integer = value < 0 ? 0UL - (uint32_t)value : (uint32_t)value;

	_setNumber(p_owner, integer, 0, _getSubDigits(p_owner), value < 0);
}

void pifFnd_SetString(PifFnd* p_owner, char* p_string)
{
    int p, src = 0;
    BOOL last = FALSE;

    p_owner->__bt.raw = FALSE;
    for (p = 0; p < p_owner->_digit_size; p++) {
    	if (!last && p_string[src]) {
    		p_owner->__p_string[p] = p_string[src];
    	}
    	else {
    		p_owner->__p_string[p] = 0x20;
    		last = TRUE;
    	}
    	src++;
    }
}

void pifFnd_SetSegments(PifFnd* p_owner, const uint8_t* p_segment)
{
	memcpy(p_owner->__p_string, p_segment, p_owner->_digit_size);
	p_owner->__bt.raw = TRUE;
}
