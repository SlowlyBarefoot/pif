#include "pif_dot_matrix.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


static void _setPattern(PifDotMatrix* p_owner, uint16_t position_x, uint16_t position_y, uint16_t position)
{
    PifDotMatrixPattern* p_pattern = &p_owner->__p_pattern[p_owner->__pattern_index];
    uint8_t* p_src;
    uint8_t* p_dst;
    uint8_t shift, col, row;

	shift = position_x & 7;
	p_src = p_pattern->p_pattern + position_y * p_pattern->col_bytes + position_x / 8;
	p_dst = p_owner->__p_paper + position;
    if (shift) {
    	uint8_t rest = 8 - shift;
    	uint8_t mask = ~((1 << rest) - 1);
    	for (row = 0; row < p_owner->__row_size; row++) {
    		for (col = 0; col < p_owner->__col_bytes; col++) {
    			p_dst[col] = (p_src[col] >> shift) + ((p_src[col + 1] << rest) & mask);
    		}
    		p_src += p_pattern->col_bytes;
    		p_dst += p_owner->__col_bytes;
    	}
    }
    else {
    	for (row = 0; row < p_owner->__row_size; row++) {
    		for (col = 0; col < p_owner->__col_bytes; col++) {
    			p_dst[col] = p_src[col];
    		}
    		p_src += p_pattern->col_bytes;
    		p_dst += p_owner->__col_bytes;
    	}
    }
}

static uint16_t _doTask(PifTask* p_task)
{
	PifDotMatrix* p_owner = p_task->_p_client;
	uint8_t* p_pattern;
	uint8_t off = 0;

	if (p_owner->__bt.led) {
		p_pattern = p_owner->__p_paper;
		int index = p_owner->__row_index * p_owner->__col_bytes;
		(*p_owner->__act_display)(p_owner->__row_index, p_pattern + index);
		p_pattern += p_owner->__total_bytes;
	}
	else {
		(*p_owner->__act_display)(p_owner->__row_index, &off);
	}
	p_owner->__row_index++;
	if (p_owner->__row_index >= p_owner->__row_size) p_owner->__row_index = 0;
	return 0;
}

static void _evtTimerBlinkFinish(void* p_issuer)
{
    PifDotMatrix* p_owner = (PifDotMatrix*)p_issuer;

    if (p_owner->__bt.blink) p_owner->__bt.led ^= 1;
}

static void _evtTimerShiftFinish(void* p_issuer)
{
    PifDotMatrix* p_owner = (PifDotMatrix*)p_issuer;

    switch (p_owner->__shift_direction) {
    case DMSD_LEFT:
        if (p_owner->__position_x < p_owner->__p_pattern[p_owner->__pattern_index].col_size - p_owner->__col_size) {
        	p_owner->__position_x++;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_HOR) {
        	p_owner->__shift_direction = DMSD_RIGHT;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_HOR) {
        	p_owner->__position_x = 0;
		}
		else {
			pifTimer_Stop(p_owner->__p_timer_shift);
			if (p_owner->evt_shift_finish) {
				(*p_owner->evt_shift_finish)(p_owner->_id);
			}
        }
    	break;

    case DMSD_RIGHT:
        if (p_owner->__position_x) {
        	p_owner->__position_x--;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_HOR) {
        	p_owner->__shift_direction = DMSD_LEFT;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_HOR) {
        	p_owner->__position_x = p_owner->__p_pattern[p_owner->__pattern_index].col_size - p_owner->__col_size;
		}
		else {
			pifTimer_Stop(p_owner->__p_timer_shift);
			if (p_owner->evt_shift_finish) {
				(*p_owner->evt_shift_finish)(p_owner->_id);
			}
		}
    	break;

    case DMSD_UP:
        if (p_owner->__position_y < p_owner->__p_pattern[p_owner->__pattern_index].row_size - p_owner->__row_size) {
        	p_owner->__position_y++;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_VER) {
        	p_owner->__shift_direction = DMSD_DOWN;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_VER) {
        	p_owner->__position_y = 0;
		}
		else {
			pifTimer_Stop(p_owner->__p_timer_shift);
			if (p_owner->evt_shift_finish) {
				(*p_owner->evt_shift_finish)(p_owner->_id);
			}
        }
    	break;

    case DMSD_DOWN:
        if (p_owner->__position_y) {
        	p_owner->__position_y--;
        }
        else if (p_owner->__shift_method == DMSM_PING_PONG_VER) {
        	p_owner->__shift_direction = DMSD_UP;
		}
        else if (p_owner->__shift_method == DMSM_REPEAT_VER) {
        	p_owner->__position_y = p_owner->__p_pattern[p_owner->__pattern_index].row_size - p_owner->__row_size;
		}
		else {
			pifTimer_Stop(p_owner->__p_timer_shift);
			if (p_owner->evt_shift_finish) {
				(*p_owner->evt_shift_finish)(p_owner->_id);
			}
		}
    	break;

    default:
    	return;
    }
    _setPattern(p_owner, p_owner->__position_x, p_owner->__position_y, 0);

    if (p_owner->__shift_count) {
    	p_owner->__shift_count--;
		if (!p_owner->__shift_count) {
			pifTimer_Stop(p_owner->__p_timer_shift);
		}
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

    p_owner->__p_paper = calloc(sizeof(uint8_t), p_owner->__total_bytes);
    if (!p_owner->__p_paper) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__bt.led = ON;
    p_owner->__act_display = act_display;
    p_owner->__period_per_row_1ms = PIF_DOT_MATRIX_PERIOD_PER_ROW;

    if (pif_act_timer1us) {
    	p_owner->__p_task = pifTaskManager_Add(TM_PERIOD_US, p_owner->__period_per_row_1ms * 1000L / row_size,
    			_doTask, p_owner, FALSE);
    }
    else {
    	p_owner->__p_task = pifTaskManager_Add(TM_PERIOD_MS, p_owner->__period_per_row_1ms / row_size,
    			_doTask, p_owner, FALSE);
    }
    if (!p_owner->__p_task) goto fail;
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
	if (p_owner->__p_pattern) free(p_owner->__p_pattern);

	p_owner->__p_pattern = calloc(sizeof(PifDotMatrixPattern), size);
    if (!p_owner->__p_pattern) {
		pif_error = E_OUT_OF_HEAP;
	    return FALSE;
	}
    p_owner->__pattern_size = size;
    p_owner->__pattern_count = 0;
    return TRUE;
}

BOOL pifDotMatrix_AddPattern(PifDotMatrix* p_owner, uint8_t col_size, uint8_t row_size, uint8_t* p_pattern)
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

uint16_t pifDotMatrix_GetPeriodPerRow(PifDotMatrix* p_owner)
{
	return p_owner->__period_per_row_1ms;
}

BOOL pifDotMatrix_SetPeriodPerRow(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_owner->__period_per_row_1ms = period1ms;
    if (pif_act_timer1us) {
    	pifTask_SetPeriod(p_owner->__p_task, p_owner->__period_per_row_1ms * 1000L / p_owner->__row_size);
    }
    else {
    	pifTask_SetPeriod(p_owner->__p_task, p_owner->__period_per_row_1ms / p_owner->__row_size);
    }
	return TRUE;
}

void pifDotMatrix_Start(PifDotMatrix* p_owner)
{
	_setPattern(p_owner, p_owner->__position_x, p_owner->__position_y, 0);
    p_owner->__p_task->pause = FALSE;
}

void pifDotMatrix_Stop(PifDotMatrix* p_owner)
{
	uint16_t col, row;
	uint8_t off = 0;

	for (row = 0; row < p_owner->__row_size; row++) {
		for (col = 0; col < p_owner->__col_size; col += 8) {
			(*p_owner->__act_display)(row, &off);
		}
	}
	p_owner->__p_task->pause = TRUE;
    if (p_owner->__bt.blink) {
		pifTimer_Stop(p_owner->__p_timer_blink);
		p_owner->__bt.blink = FALSE;
    }
}

BOOL pifDotMatrix_SelectPattern(PifDotMatrix* p_owner, uint8_t pattern_index)
{
	if (pattern_index >= p_owner->__pattern_size) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__pattern_index = pattern_index;
	_setPattern(p_owner, p_owner->__position_x, p_owner->__position_y, 0);
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
	if (!pifTimer_Start(p_owner->__p_timer_blink, period1ms * 1000L / p_owner->__p_timer_manager->_period1us)) return FALSE;
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

void pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (p_owner->__p_timer_blink) {
		p_owner->__p_timer_blink->target = period1ms * 1000 / p_owner->__p_timer_manager->_period1us;
	}
}

BOOL pifDotMatrix_SetPosition(PifDotMatrix* p_owner, uint16_t pos_x, uint16_t pos_y)
{
	if (pos_x >= p_owner->__p_pattern[p_owner->__pattern_index].col_size - p_owner->__col_size ||
			pos_y >= p_owner->__p_pattern[p_owner->__pattern_index].row_size - p_owner->__row_size) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	p_owner->__position_x = pos_x;
	p_owner->__position_y = pos_y;
	return TRUE;
}

BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count)
{
	if (!period1ms || shift_direction == DMSD_NONE) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_shift) {
		p_owner->__p_timer_shift = pifTimerManager_Add(p_owner->__p_timer_manager, TT_REPEAT);
		if (!p_owner->__p_timer_shift) return FALSE;
		pifTimer_AttachEvtFinish(p_owner->__p_timer_shift, _evtTimerShiftFinish, p_owner);
	}
	if(!pifTimer_Start(p_owner->__p_timer_shift, period1ms * 1000L / p_owner->__p_timer_manager->_period1us)) return FALSE;
	p_owner->__shift_direction = shift_direction;
	p_owner->__shift_method = shift_method;
	p_owner->__shift_count = count;
	return TRUE;
}

void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner)
{
	if (p_owner->__p_timer_shift) {
		p_owner->__shift_direction = DMSD_NONE;
		pifTimer_Stop(p_owner->__p_timer_shift);
		p_owner->__position_x = 0;
		p_owner->__position_y = 0;
	}
}

void pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (p_owner->__p_timer_shift) {
		p_owner->__p_timer_shift->target = period1ms * 1000 / p_owner->__p_timer_manager->_period1us;
	}
}
