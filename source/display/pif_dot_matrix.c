// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_dot_matrix.h"


static BOOL _hasPattern(PifDotMatrix* p_owner)
{
	return p_owner->__p_pattern && p_owner->__pattern_count;
}

static void _setPattern(PifDotMatrix* p_owner)
{
    PifDotMatrixPattern* p_pattern = &p_owner->__p_pattern[p_owner->__pattern_index];
    uint16_t first = p_owner->__position_x / 8;
    uint8_t shift = p_owner->__position_x & 7;
    uint8_t* p_src;
    uint8_t* p_dst;
    uint16_t col, row;
    uint8_t data;

	p_src = p_pattern->p_pattern + (uint32_t)p_owner->__position_y * p_pattern->col_bytes + first;
	p_dst = p_owner->__p_paper;
	for (row = 0; row < p_owner->__row_size; row++) {
		for (col = 0; col < p_owner->__col_bytes; col++) {
			data = p_src[col] >> shift;
			// The last byte of a pattern row has no next byte to take the upper bits from.
			if (shift && first + col + 1 < p_pattern->col_bytes) data |= p_src[col + 1] << (8 - shift);
			p_dst[col] = data;
		}
		p_src += p_pattern->col_bytes;
		p_dst += p_owner->__col_bytes;
	}
}

static void _stopShift(PifDotMatrix* p_owner)
{
	// A step the timer manager still has pending is then dropped by _evtTimerShiftFinish().
	p_owner->__shift_direction = DMSD_NONE;
	pifTimer_Stop(p_owner->__p_timer_shift);
}

static void _endShift(PifDotMatrix* p_owner)
{
	_stopShift(p_owner);
	if (p_owner->evt_shift_finish) {
		(*p_owner->evt_shift_finish)(p_owner->_id);
	}
}

static BOOL _isValidShift(PifDotMatrixShiftDir shift_direction, PifDotMatrixShiftMethod shift_method)
{
	switch (shift_direction) {
	case DMSD_LEFT:
	case DMSD_RIGHT:
		return shift_method == DMSM_ONCE || shift_method == DMSM_REPEAT_HOR || shift_method == DMSM_PING_PONG_HOR;

	case DMSD_UP:
	case DMSD_DOWN:
		return shift_method == DMSM_ONCE || shift_method == DMSM_REPEAT_VER || shift_method == DMSM_PING_PONG_VER;

	default:
		return FALSE;
	}
}

static uint32_t _doTask(PifTask* p_task)
{
	PifDotMatrix* p_owner = p_task->_p_client;
	uint8_t* p_data;

	if (p_owner->__bt.led) {
		p_data = p_owner->__p_paper + p_owner->__row_index * p_owner->__col_bytes;
	}
	else {
		p_data = p_owner->__p_paper + p_owner->__total_bytes;
	}
	(*p_owner->__act_display)(p_owner->__row_index, p_data);
	p_owner->__row_index++;
	if (p_owner->__row_index >= p_owner->__row_size) p_owner->__row_index = 0;
	return 0;
}

static void _evtTimerBlinkFinish(PifIssuerP p_issuer)
{
    PifDotMatrix* p_owner = (PifDotMatrix*)p_issuer;

    if (p_owner->__bt.blink) p_owner->__bt.led ^= 1;
}

static void _evtTimerShiftFinish(PifIssuerP p_issuer)
{
    PifDotMatrix* p_owner = (PifDotMatrix*)p_issuer;
    PifDotMatrixPattern* p_pattern;
    uint16_t max_x, max_y;

    if (p_owner->__shift_direction == DMSD_NONE) return;

    p_pattern = &p_owner->__p_pattern[p_owner->__pattern_index];
    max_x = p_pattern->col_size - p_owner->__col_size;
    max_y = p_pattern->row_size - p_owner->__row_size;

    // A ping-pong turns and moves on the same step, so that the pattern does not rest twice at an end.
    switch (p_owner->__shift_direction) {
    case DMSD_LEFT:
        if (p_owner->__position_x < max_x) {
        	p_owner->__position_x++;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_HOR) {
        	p_owner->__shift_direction = DMSD_RIGHT;
        	if (p_owner->__position_x) p_owner->__position_x--;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_HOR) {
        	p_owner->__position_x = 0;
		}
		else {
			_endShift(p_owner);
			return;
        }
    	break;

    case DMSD_RIGHT:
        if (p_owner->__position_x) {
        	p_owner->__position_x--;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_HOR) {
        	p_owner->__shift_direction = DMSD_LEFT;
        	if (p_owner->__position_x < max_x) p_owner->__position_x++;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_HOR) {
        	p_owner->__position_x = max_x;
		}
		else {
			_endShift(p_owner);
			return;
		}
    	break;

    case DMSD_UP:
        if (p_owner->__position_y < max_y) {
        	p_owner->__position_y++;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_VER) {
        	p_owner->__shift_direction = DMSD_DOWN;
        	if (p_owner->__position_y) p_owner->__position_y--;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_VER) {
        	p_owner->__position_y = 0;
		}
		else {
			_endShift(p_owner);
			return;
        }
    	break;

    case DMSD_DOWN:
        if (p_owner->__position_y) {
        	p_owner->__position_y--;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_VER) {
        	p_owner->__shift_direction = DMSD_UP;
        	if (p_owner->__position_y < max_y) p_owner->__position_y++;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_VER) {
        	p_owner->__position_y = max_y;
		}
		else {
			_endShift(p_owner);
			return;
		}
    	break;

    default:
    	return;
    }
    _setPattern(p_owner);

    if (p_owner->__shift_count) {
    	p_owner->__shift_count--;
		if (!p_owner->__shift_count) _endShift(p_owner);
    }
}

BOOL pifDotMatrix_Init(PifDotMatrix* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display)
{
    if (!p_owner || !p_timer_manager || !col_size || !row_size || !act_display) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifDotMatrix));

    p_owner->__p_timer_manager = p_timer_manager;
    p_owner->__col_size = col_size;
    p_owner->__row_size = row_size;
    p_owner->__col_bytes = (p_owner->__col_size - 1) / 8 + 1;
    p_owner->__total_bytes = p_owner->__col_bytes * p_owner->__row_size;

    // One blank row more after the display data, which is output while the display is off.
    p_owner->__p_paper = calloc(p_owner->__total_bytes + p_owner->__col_bytes, sizeof(uint8_t));
    if (!p_owner->__p_paper) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__bt.led = ON;
    p_owner->__act_display = act_display;
    p_owner->__frame_period_1ms = PIF_DOT_MATRIX_FRAME_PERIOD;

	p_owner->__p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, p_owner->__frame_period_1ms * 1000UL / row_size,
			_doTask, p_owner, FALSE);
    if (!p_owner->__p_task) goto fail;
	p_owner->__p_task->name = "DotMatrix";
    return TRUE;

fail:
	pifDotMatrix_Clear(p_owner);
    return FALSE;
}

void pifDotMatrix_Clear(PifDotMatrix* p_owner)
{
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	if (p_owner->__p_paper) {
		free(p_owner->__p_paper);
		p_owner->__p_paper = NULL;
	}
	if (p_owner->__p_pattern) {
		free(p_owner->__p_pattern);
		p_owner->__p_pattern = NULL;
	}
	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
	if (p_owner->__p_timer_shift) {
		pifTimerManager_Remove(p_owner->__p_timer_shift);
		p_owner->__p_timer_shift = NULL;
	}
}

BOOL pifDotMatrix_SetPatternSize(PifDotMatrix* p_owner, uint8_t size)
{
	if (!size) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	// The shift and the display data refer to the patterns released here.
	if (p_owner->__p_timer_shift) _stopShift(p_owner);
	if (p_owner->__p_pattern) free(p_owner->__p_pattern);
	p_owner->__p_pattern = NULL;
	p_owner->__pattern_size = 0;
	p_owner->__pattern_count = 0;
	p_owner->__pattern_index = 0;
	p_owner->__position_x = 0;
	p_owner->__position_y = 0;
	memset(p_owner->__p_paper, 0, p_owner->__total_bytes);

	p_owner->__p_pattern = calloc(size, sizeof(PifDotMatrixPattern));
    if (!p_owner->__p_pattern) {
		pif_error = E_OUT_OF_HEAP;
	    return FALSE;
	}
    p_owner->__pattern_size = size;
    return TRUE;
}

BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint16_t col_size, uint16_t row_size, uint8_t* p_pattern)
{
	if (p_owner->__pattern_count >= p_owner->__pattern_size) {
        pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
    }

    if (!col_size || !row_size || col_size < p_owner->__col_size || row_size < p_owner->__row_size || !p_pattern) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    PifDotMatrixPattern* p_pattern_ = &p_owner->__p_pattern[p_owner->__pattern_count];

    p_pattern_->col_size = col_size;
    p_pattern_->col_bytes = (col_size - 1) / 8 + 1;
    p_pattern_->row_size = row_size;
    p_pattern_->p_pattern = p_pattern;

    p_owner->__pattern_count = p_owner->__pattern_count + 1;
    return TRUE;
}

uint16_t pifDotMatrix_GetFramePeriod(PifDotMatrix* p_owner)
{
	return p_owner->__frame_period_1ms;
}

BOOL pifDotMatrix_SetFramePeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_owner->__frame_period_1ms = period1ms;
   	pifTask_ChangePeriod(p_owner->__p_task, p_owner->__frame_period_1ms * 1000UL / p_owner->__row_size);
	return TRUE;
}

void pifDotMatrix_Start(PifDotMatrix* p_owner)
{
	if (_hasPattern(p_owner)) _setPattern(p_owner);
    p_owner->__p_task->pause = FALSE;
}

void pifDotMatrix_Stop(PifDotMatrix* p_owner)
{
	uint16_t row;

	p_owner->__p_task->pause = TRUE;
	for (row = 0; row < p_owner->__row_size; row++) {
		(*p_owner->__act_display)(row, p_owner->__p_paper + p_owner->__total_bytes);
	}
    if (p_owner->__bt.blink) {
		pifTimer_Stop(p_owner->__p_timer_blink);
		p_owner->__bt.blink = FALSE;
    }
    // Blink may have been stopped in its off phase.
	p_owner->__bt.led = ON;
	if (p_owner->__p_timer_shift) _stopShift(p_owner);
}

BOOL pifDotMatrix_SelectPattern(PifDotMatrix* p_owner, uint8_t pattern_index)
{
	PifDotMatrixPattern* p_pattern;
	uint16_t max_x, max_y;

	if (pattern_index >= p_owner->__pattern_count) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__pattern_index = pattern_index;

	// A position taken over from a larger pattern is kept inside this one.
	p_pattern = &p_owner->__p_pattern[pattern_index];
	max_x = p_pattern->col_size - p_owner->__col_size;
	max_y = p_pattern->row_size - p_owner->__row_size;
	if (p_owner->__position_x > max_x) p_owner->__position_x = max_x;
	if (p_owner->__position_y > max_y) p_owner->__position_y = max_y;

	_setPattern(p_owner);
    return TRUE;
}

BOOL pifDotMatrix_BlinkOn(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	if (!p_owner->__p_timer_blink) {
		p_owner->__p_timer_blink = pifTimerManager_Add(p_owner->__p_timer_manager, TT_REPEAT);
		if (!p_owner->__p_timer_blink) return FALSE;
		pifTimer_AttachEvtFinish(p_owner->__p_timer_blink, _evtTimerBlinkFinish, p_owner);
	}
	if (!pifTimer_Start(p_owner->__p_timer_blink, period1ms * 1000UL / p_owner->__p_timer_manager->_period1us)) return FALSE;
	p_owner->__bt.blink = TRUE;
    return TRUE;
}

void pifDotMatrix_BlinkOff(PifDotMatrix* p_owner)
{
	p_owner->__bt.led = ON;
	p_owner->__bt.blink = FALSE;
	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

BOOL pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!p_owner->__p_timer_blink) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// A period that comes to 0 ticks is refused here.
	return pifTimer_SetTarget(p_owner->__p_timer_blink, period1ms * 1000UL / p_owner->__p_timer_manager->_period1us);
}

BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y)
{
	PifDotMatrixPattern* p_pattern;

	if (!_hasPattern(p_owner)) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_pattern = &p_owner->__p_pattern[p_owner->__pattern_index];
	if (pos_x > p_pattern->col_size - p_owner->__col_size || pos_y > p_pattern->row_size - p_owner->__row_size) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	p_owner->__position_x = pos_x;
	p_owner->__position_y = pos_y;
	_setPattern(p_owner);
	return TRUE;
}

BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count)
{
	if (!period1ms || !_isValidShift(shift_direction, shift_method)) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!_hasPattern(p_owner)) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!p_owner->__p_timer_shift) {
		p_owner->__p_timer_shift = pifTimerManager_Add(p_owner->__p_timer_manager, TT_REPEAT);
		if (!p_owner->__p_timer_shift) return FALSE;
		pifTimer_AttachEvtFinish(p_owner->__p_timer_shift, _evtTimerShiftFinish, p_owner);
	}

	// pifTimer_Start() runs a step left pending by the previous shift, which then does nothing.
	p_owner->__shift_direction = DMSD_NONE;
	if (!pifTimer_Start(p_owner->__p_timer_shift, period1ms * 1000UL / p_owner->__p_timer_manager->_period1us)) return FALSE;
	p_owner->__shift_direction = shift_direction;
	p_owner->__shift_method = shift_method;
	p_owner->__shift_count = count;
	return TRUE;
}

void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner)
{
	if (p_owner->__p_timer_shift) {
		_stopShift(p_owner);
		p_owner->__position_x = 0;
		p_owner->__position_y = 0;
		if (_hasPattern(p_owner)) _setPattern(p_owner);
	}
}

BOOL pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!p_owner->__p_timer_shift) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// A period that comes to 0 ticks is refused here.
	return pifTimer_SetTarget(p_owner->__p_timer_shift, period1ms * 1000UL / p_owner->__p_timer_manager->_period1us);
}
