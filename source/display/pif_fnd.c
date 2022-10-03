#include "display/pif_fnd.h"


const uint8_t kFndNumber[] = {
		0x3F, /*  0  */	0x06, /*  1  */	0x5B, /*  2  */ 0x4F, /*  3  */ 	// 0x30
		0x66, /*  4  */ 0x6D, /*  5  */ 0x7D, /*  6  */ 0x07, /*  7  */		// 0x34
		0x7F, /*  8  */ 0x6F  /*  9  */ 									// 0x38
};

const uint8_t *c_user_char;
static uint8_t s_user_char_count = 0;


static uint16_t _doTask(PifTask* p_task)
{
	PifFnd *p_owner = p_task->_p_client;
	uint8_t ch, seg = 0;
	BOOL point = FALSE;

	if (p_owner->__bt.led) {
		ch = p_owner->__p_string[p_owner->__digit_index];
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
		else if (s_user_char_count && ch >= 'A' && ch < 'A' + s_user_char_count) {
			seg = c_user_char[ch - 'A'];
		}
		if (point) seg |= 0x80;
		(*p_owner->__act_display)(seg, p_owner->__digit_index);
	}
	else {
		(*p_owner->__act_display)(0, p_owner->__digit_index);
	}
	p_owner->__digit_index++;
	if (p_owner->__digit_index >= p_owner->_digit_size) p_owner->__digit_index = 0;
	return 0;
}

static void _evtTimerBlinkFinish(PifIssueP p_issuer)
{
    PifFnd* p_owner = (PifFnd*)p_issuer;

    if (p_owner->__bt.blink) p_owner->__bt.led ^= 1;
}

void pifFnd_SetUserChar(const uint8_t* p_user_char, uint8_t count)
{
	c_user_char = p_user_char;
	s_user_char_count = count;
}

BOOL pifFnd_Init(PifFnd* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t digit_size, PifActFndDisplay act_display)
{
    if (!p_owner || !p_timer_manager || !digit_size || !act_display) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifFnd));

    p_owner->__p_string = calloc(sizeof(uint8_t), digit_size);
    if (!p_owner->__p_string) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}
    for (int i = 0; i < digit_size; i++) p_owner->__p_string[i] = 0x20;

    p_owner->__p_timer_manager = p_timer_manager;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__bt.led = ON;
    p_owner->_digit_size = digit_size;
    p_owner->__act_display = act_display;
    p_owner->__period_per_digit_1ms = PIF_FND_PERIOD_PER_DIGIT;

    if (pif_act_timer1us) {
    	p_owner->__p_task = pifTaskManager_Add(TM_PERIOD_US, p_owner->__period_per_digit_1ms * 1000L / digit_size,
    			_doTask, p_owner, FALSE);
    }
    else {
    	p_owner->__p_task = pifTaskManager_Add(TM_PERIOD_MS, p_owner->__period_per_digit_1ms / digit_size,
    			_doTask, p_owner, FALSE);
    }
    if (!p_owner->__p_task) goto fail;
    return TRUE;

fail:
	pifFnd_Clear(p_owner);
    return FALSE;
}

void pifFnd_Clear(PifFnd* p_owner)
{
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	if (p_owner->__p_string) {
		free(p_owner->__p_string);
		p_owner->__p_string = NULL;
	}
	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

uint16_t pifFnd_GetPeriodPerDigit(PifFnd* p_owner)
{
	return p_owner->__period_per_digit_1ms;
}

BOOL pifFnd_SetPeriodPerDigit(PifFnd* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_owner->__period_per_digit_1ms = period1ms;
    if (pif_act_timer1us) {
    	pifTask_ChangePeriod(p_owner->__p_task, p_owner->__period_per_digit_1ms * 1000L / p_owner->_digit_size);
    }
    else {
    	pifTask_ChangePeriod(p_owner->__p_task, p_owner->__period_per_digit_1ms / p_owner->_digit_size);
    }
	return TRUE;
}

void pifFnd_Start(PifFnd* p_owner)
{
	p_owner->__p_task->pause = FALSE;
}

void pifFnd_Stop(PifFnd* p_owner)
{
	int i;

	for (i = 0; i < p_owner->_digit_size; i++) {
		(*p_owner->__act_display)(0, 1 << i);
	}
	p_owner->__p_task->pause = TRUE;
    if (p_owner->__bt.blink) {
		pifTimer_Stop(p_owner->__p_timer_blink);
		p_owner->__bt.blink = FALSE;
    }
}

BOOL pifFnd_BlinkOn(PifFnd* p_owner, uint16_t period1ms)
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

void pifFnd_BlinkOff(PifFnd* p_owner)
{
	p_owner->__bt.led = ON;
	p_owner->__bt.blink = FALSE;
	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

BOOL pifFnd_ChangeBlinkPeriod(PifFnd* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_blink || p_owner->__p_timer_blink->_step == TS_STOP) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer_blink->target = period1ms * 1000 / p_owner->__p_timer_manager->_period1us;
	return TRUE;
}

void pifFnd_SetFillZero(PifFnd* p_owner, BOOL fill_zero)
{
    p_owner->__bt.fill_zero = fill_zero;
}

void pifFnd_SetFloat(PifFnd* p_owner, double value)
{
    BOOL minus = FALSE;

    if (value < 0.0) {
    	minus = TRUE;
    	value *= -1.0;
    }
    uint32_t num = (uint32_t)value;
    int sp = p_owner->_digit_size;
    if (p_owner->sub_numeric_digits) {
    	value -= num;
    	for (int p = sp - p_owner->sub_numeric_digits; p < sp; p++) {
    		value *= 10;
    		uint32_t sd = (uint32_t)value;
    		p_owner->__p_string[p] = '0' + sd;
    		value -= sd;
    	}
    	sp -= p_owner->sub_numeric_digits;
    }
    sp--;
	BOOL first = TRUE;
	for (int p = sp; p >= 0; p--) {
		if (!first && !num) {
			if (minus) {
				p_owner->__p_string[p] = '-';
				minus = FALSE;
			}
			else {
				p_owner->__p_string[p] = 0x20;
			}
		}
		else {
			uint8_t digit = num % 10;
			p_owner->__p_string[p] = '0' + digit;
			if (digit) first = FALSE;
		}
		num = num / 10;
	}
	p_owner->__p_string[p_owner->_digit_size - 1 - p_owner->sub_numeric_digits] |= 0x80;
	if (num || minus) {
		p_owner->__p_string[p_owner->_digit_size - 1] = '_';
	}
}

void pifFnd_SetInterger(PifFnd* p_owner, int32_t value)
{
    BOOL minus = FALSE;

    if (value < 0) {
    	minus = TRUE;
    	value *= -1;
    }
    int sp = p_owner->_digit_size - 1;
    if (p_owner->sub_numeric_digits) {
    	for (int p = sp; p >= sp - p_owner->sub_numeric_digits; p--) {
    		p_owner->__p_string[p] = '0';
    	}
    	sp -= p_owner->sub_numeric_digits;
    }
    if (p_owner->__bt.fill_zero) {
        for (int p = sp; p >= minus; p--) {
			uint8_t digit = value % 10;
			p_owner->__p_string[p] = '0' + digit;
			value = value / 10;
        }
        if (minus) {
        	p_owner->__p_string[0] = '-';
        }
        if (value) {
        	p_owner->__p_string[p_owner->_digit_size - 1] = '_';
        }
    }
    else {
        BOOL first = TRUE;
        for (int p = sp; p >= 0; p--) {
        	if (!first && !value) {
        		if (minus) {
        			p_owner->__p_string[p] = '-';
            		minus = FALSE;
        		}
        		else {
        			p_owner->__p_string[p] = 0x20;
        		}
        	}
        	else {
            	uint8_t digit = value % 10;
            	p_owner->__p_string[p] = '0' + digit;
            	if (digit) first = FALSE;
        	}
        	value = value / 10;
        }
        if (value || minus) {
        	p_owner->__p_string[p_owner->_digit_size - 1] = '_';
        }
    }
}

void pifFnd_SetString(PifFnd* p_owner, char* p_string)
{
    int src = 0;
    BOOL last = FALSE;

    for (int p = 0; p < p_owner->_digit_size; p++) {
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
