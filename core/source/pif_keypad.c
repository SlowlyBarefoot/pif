#include "pif_keypad.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


static void _checkKeyState(PifKeypad* p_owner, int idx, BOOL button)
{
	uint32_t time;
	PifKey* p_key = &p_owner->__p_key[idx];

	switch (p_key->state) {
	case KS_IDLE:
		if (button == ON) {
			p_key->pressed = FALSE;
			p_key->long_released = FALSE;
			p_key->pressed_time = pif_timer1sec * 1000 + pif_timer1ms;
			if (p_owner->evt_double_pressed) {
				if (!p_key->first_time) {
					p_key->first_time = p_key->pressed_time;
					p_key->double_pressed = FALSE;
				}
				else {
					if (p_key->pressed_time - p_key->first_time < p_owner->_double_time1ms) {
						(*p_owner->evt_double_pressed)(p_owner->__p_user_keymap[idx]);
						p_key->double_pressed = TRUE;
					}
				}
			}
			p_key->state = KS_PRESSED;
		}
		else if (p_key->short_clicked) {
			time = pif_timer1sec * 1000 + pif_timer1ms - p_key->pressed_time;
			if (time >= p_owner->_double_time1ms) {
				if (p_owner->evt_pressed)  {
					(*p_owner->evt_pressed)(p_owner->__p_user_keymap[idx]);
				}
				if (p_owner->evt_released)  {
					(*p_owner->evt_released)(p_owner->__p_user_keymap[idx], time);
				}
				p_key->short_clicked = FALSE;
				p_key->first_time = 0L;
			}
		}
		break;

	case KS_PRESSED:
		if (button == OFF) {
			if (p_key->double_pressed)	{
				p_key->first_time = 0L;
				p_key->state = KS_RELEASED;
			}
			else p_key->state = KS_IDLE;
		}
		else {
			time = pif_timer1sec * 1000 + pif_timer1ms - p_key->pressed_time;
			if (time >= p_owner->_hold_time1ms) {
				p_key->state = KS_HOLD;
			}
		}
		break;

	case KS_HOLD:
		if (!p_key->double_pressed) {
			time = pif_timer1sec * 1000 + pif_timer1ms - p_key->pressed_time;
			if (time >= p_owner->_long_time1ms) {
				if (!p_key->long_released) {
					if (p_owner->evt_long_released)  {
						(*p_owner->evt_long_released)(p_owner->__p_user_keymap[idx], time);
					}
					p_key->long_released = TRUE;
				}
			}
			else if (time >= p_owner->_double_time1ms) {
				if (!p_key->pressed) {
					if (p_owner->evt_pressed)  {
						(*p_owner->evt_pressed)(p_owner->__p_user_keymap[idx]);
					}
					p_key->pressed = TRUE;
				}
			}
			if (button == OFF) {
				if (!p_key->pressed) {
					p_key->short_clicked = TRUE;
					p_key->state = KS_IDLE;
				}
				else {
					p_key->state = KS_RELEASED;
				}
			}
		}
		else {
			if (button == OFF) {
				if (p_key->double_pressed) {
					p_key->short_clicked = FALSE;
					p_key->first_time = 0L;
				}
				p_key->state = KS_RELEASED;
			}
		}
		break;

	case KS_RELEASED:
		if (!p_key->long_released && p_owner->evt_released)  {
			time = pif_timer1sec * 1000 + pif_timer1ms - p_key->pressed_time;
			(*p_owner->evt_released)(p_owner->__p_user_keymap[idx], time);
		}
		p_key->state = KS_IDLE;
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	int idx, r, c;

	PifKeypad* p_owner = p_task->_p_client;

	if (!p_owner->__p_key || !p_owner->__act_acquire) return 0;

	(*p_owner->__act_acquire)(p_owner->__p_state);

	for (idx = 0, r = 0; r < p_owner->__num_rows; r++) {
		for (c = 0; c < p_owner->__num_cols; c++, idx++) {
			_checkKeyState(p_owner, idx, (p_owner->__p_state[r] >> c) & 1);
		}
	}
	return 0;
}

PifKeypad* pifKeypad_Create(PifId id, uint8_t num_rows, uint8_t num_cols, const char* p_user_keymap)
{
	PifKeypad* p_owner = malloc(sizeof(PifKeypad));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifKeypad_Init(p_owner, id, num_rows, num_cols, p_user_keymap)) {
		pifKeypad_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifKeypad_Destroy(PifKeypad** pp_owner)
{
	if (*pp_owner) {
		pifKeypad_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifKeypad_Init(PifKeypad* p_owner, PifId id, uint8_t num_rows, uint8_t num_cols, const char* p_user_keymap)
{
	if (!p_owner || !num_rows || !num_cols || !p_user_keymap) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifKeypad));

	p_owner->__p_key = calloc(sizeof(PifKey), num_rows * num_cols);
	if (!p_owner->__p_key) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	p_owner->__p_state = calloc(sizeof(uint16_t), num_rows);
	if (!p_owner->__p_state) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__p_user_keymap = p_user_keymap;
    p_owner->__num_rows = num_rows;
    p_owner->__num_cols = num_cols;
    p_owner->_hold_time1ms = PIF_KEYPAD_DEFAULT_HOLD_TIME;
    p_owner->_long_time1ms = PIF_KEYPAD_DEFAULT_LONG_TIME;
    p_owner->_double_time1ms = PIF_KEYPAD_DEFAULT_DOUBLE_TIME;
    p_owner->__control_period_1ms = PIF_KEYPAD_CONTROL_PERIOD;

	if (!pifTaskManager_Add(TM_PERIOD_MS, p_owner->__control_period_1ms, _doTask, p_owner, TRUE)) goto fail;
    return TRUE;

fail:
	pifKeypad_Clear(p_owner);
	return FALSE;
}

void pifKeypad_Clear(PifKeypad* p_owner)
{
	if (p_owner->__p_state) {
		free(p_owner->__p_state);
		p_owner->__p_state = NULL;
	}
	if (p_owner->__p_key) {
		free(p_owner->__p_key);
		p_owner->__p_key = NULL;
	}
}

uint16_t pifKeypad_GetControlPeriod(PifKeypad* p_owner)
{
	return p_owner->__control_period_1ms;
}

BOOL pifKeypad_SetControlPeriod(PifKeypad* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_owner->__control_period_1ms = period1ms;
   	pifTask_SetPeriod(p_owner->__p_task, p_owner->__control_period_1ms);
	return TRUE;
}

void pifKeypad_AttachAction(PifKeypad* p_owner, PifActKeypadAcquire act_acquire)
{
	p_owner->__act_acquire = act_acquire;
}

BOOL pifKeypad_SetHoldTime(PifKeypad* p_owner, uint16_t hold_time1ms)
{
	if (hold_time1ms >= p_owner->_double_time1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_hold_time1ms = hold_time1ms;
	return TRUE;
}

BOOL pifKeypad_SetLongTime(PifKeypad* p_owner, uint16_t long_time1ms)
{
	if (long_time1ms <= p_owner->_double_time1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_long_time1ms = long_time1ms;
	return TRUE;
}

BOOL pifKeypad_SetDoubleTime(PifKeypad* p_owner, uint16_t double_time1ms)
{
	if (double_time1ms <= p_owner->_hold_time1ms || double_time1ms >= p_owner->_long_time1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_double_time1ms = double_time1ms;
	return TRUE;
}
