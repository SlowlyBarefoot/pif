#include "pifDotMatrix.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
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
			pifPulse_StopItem(p_owner->__p_timer_shift);
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
			pifPulse_StopItem(p_owner->__p_timer_shift);
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
			pifPulse_StopItem(p_owner->__p_timer_shift);
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
			pifPulse_StopItem(p_owner->__p_timer_shift);
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
			pifPulse_StopItem(p_owner->__p_timer_shift);
		}
    }
}

/**
 * @fn pifDotMatrix_Create
 * @brief
 * @param id
 * @param p_timer
 * @param col_size
 * @param row_size
 * @param act_display
 * @return
 */
PifDotMatrix* pifDotMatrix_Create(PifId id, PifPulse* p_timer, uint16_t col_size, uint16_t row_size,
		PifActDotMatrixDisplay act_display)
{
	PifDotMatrix* p_owner = NULL;

    if (!p_timer || !col_size || !row_size || !act_display) {
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    p_owner = calloc(sizeof(PifDotMatrix), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    p_owner->__p_timer = p_timer;
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
    p_owner->__p_timer_blink = NULL;
    p_owner->__p_timer_shift = NULL;
    return p_owner;

fail:
	if (p_owner) free(p_owner);
    return NULL;
}

/**
 * @fn pifDotMatrix_Destroy
 * @brief
 * @param pp_owner
 */
void pifDotMatrix_Destroy(PifDotMatrix** pp_owner)
{
	if (*pp_owner) {
		PifDotMatrix* p_owner = *pp_owner;
		if (p_owner->__p_paper) {
			free(p_owner->__p_paper);
			p_owner->__p_paper = NULL;
		}
		if (p_owner->__p_pattern) {
			free(p_owner->__p_pattern);
			p_owner->__p_pattern = NULL;
		}
		if (p_owner->__p_timer_blink) {
			pifPulse_RemoveItem(p_owner->__p_timer_blink);
		}
		if (p_owner->__p_timer_shift) {
			pifPulse_RemoveItem(p_owner->__p_timer_shift);
		}
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifDotMatrix_SetPatternSize
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
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

/**
 * @fn pifDotMatrix_AddPattern
 * @brief
 * @param p_owner
 * @param col_size
 * @param row_size
 * @param p_pattern
 * @return
 */
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

/**
 * @fn pifDotMatrix_Start
 * @brief
 * @param p_owner
 */
void pifDotMatrix_Start(PifDotMatrix* p_owner)
{
	_setPattern(p_owner, p_owner->__position_x, p_owner->__position_y, 0);
    p_owner->__bt.run = TRUE;
}

/**
 * @fn pifDotMatrix_Stop
 * @brief
 * @param p_owner
 */
void pifDotMatrix_Stop(PifDotMatrix* p_owner)
{
	uint16_t col, row;
	uint8_t off = 0;

	for (row = 0; row < p_owner->__row_size; row++) {
		for (col = 0; col < p_owner->__col_size; col += 8) {
			(*p_owner->__act_display)(row, &off);
		}
	}
	p_owner->__bt.run = FALSE;
    if (p_owner->__bt.blink) {
		pifPulse_StopItem(p_owner->__p_timer_blink);
		p_owner->__bt.blink = FALSE;
    }
}

/**
 * @fn pifDotMatrix_SelectPattern
 * @brief
 * @param p_owner
 * @param pattern_index
 * @return
 */
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

/**
 * @fn pifDotMatrix_BlinkOn
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifDotMatrix_BlinkOn(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	if (!p_owner->__p_timer_blink) {
		p_owner->__p_timer_blink = pifPulse_AddItem(p_owner->__p_timer, PT_REPEAT);
		if (!p_owner->__p_timer_blink) return FALSE;
		pifPulse_AttachEvtFinish(p_owner->__p_timer_blink, _evtTimerBlinkFinish, p_owner);
	}
	if (!pifPulse_StartItem(p_owner->__p_timer_blink, period1ms * 1000L / p_owner->__p_timer->_period1us)) return FALSE;
	p_owner->__bt.blink = TRUE;
    return TRUE;
}

/**
 * @fn pifDotMatrix_BlinkOff
 * @brief
 * @param p_owner
 */
void pifDotMatrix_BlinkOff(PifDotMatrix* p_owner)
{
	p_owner->__bt.led = ON;
	p_owner->__bt.blink = FALSE;
	if (p_owner->__p_timer_blink) {
		pifPulse_RemoveItem(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

/**
 * @fn pifDotMatrix_ChangeBlinkPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 */
void pifDotMatrix_ChangeBlinkPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (p_owner->__p_timer_blink) {
		p_owner->__p_timer_blink->target = period1ms * 1000 / p_owner->__p_timer->_period1us;
	}
}

/**
 * @fn pifDotMatrix_SetPosition
 * @brief
 * @param p_owner
 * @param pos_x
 * @param pos_y
 * @return
 */
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

/**
 * @fn pifDotMatrix_ShiftOn
 * @brief
 * @param p_owner
 * @param shift_direction
 * @param shift_method
 * @param period1ms
 * @param count
 * @return
 */
BOOL pifDotMatrix_ShiftOn(PifDotMatrix* p_owner, PifDotMatrixShiftDir shift_direction,
		PifDotMatrixShiftMethod shift_method, uint16_t period1ms, uint16_t count)
{
	if (!period1ms || shift_direction == DMSD_NONE) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_shift) {
		p_owner->__p_timer_shift = pifPulse_AddItem(p_owner->__p_timer, PT_REPEAT);
		if (!p_owner->__p_timer_shift) return FALSE;
		pifPulse_AttachEvtFinish(p_owner->__p_timer_shift, _evtTimerShiftFinish, p_owner);
	}
	if(!pifPulse_StartItem(p_owner->__p_timer_shift, period1ms * 1000L / p_owner->__p_timer->_period1us)) return FALSE;
	p_owner->__shift_direction = shift_direction;
	p_owner->__shift_method = shift_method;
	p_owner->__shift_count = count;
	return TRUE;
}

/**
 * @fn pifDotMatrix_ShiftOff
 * @brief
 * @param p_owner
 */
void pifDotMatrix_ShiftOff(PifDotMatrix* p_owner)
{
	if (p_owner->__p_timer_shift) {
		p_owner->__shift_direction = DMSD_NONE;
		pifPulse_StopItem(p_owner->__p_timer_shift);
		p_owner->__position_x = 0;
		p_owner->__position_y = 0;
	}
}

/**
 * @fn pifDotMatrix_ChnageShiftPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 */
void pifDotMatrix_ChangeShiftPeriod(PifDotMatrix* p_owner, uint16_t period1ms)
{
	if (p_owner->__p_timer_shift) {
		p_owner->__p_timer_shift->target = period1ms * 1000 / p_owner->__p_timer->_period1us;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifDotMatrix* p_owner = p_task->_p_client;
	uint8_t* p_pattern;
	uint8_t off = 0;

	if (!p_owner->__bt.run) return 0;

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

/**
 * @fn pifDotMatrix_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifDotMatrix_AttachTask(PifDotMatrix* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_owner, start);
}
