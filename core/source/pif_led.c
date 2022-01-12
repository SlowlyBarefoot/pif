#include "pif_led.h"


static void _evtTimerSBlinkFinish(void* p_issuer)
{
    PifLed* p_owner = (PifLed*)p_issuer;
	PifLedBlink* p_blink = &p_owner->__p_blinks[0];
	uint8_t i;

	p_blink->state ^= 1;
	for (i = 0; i < p_owner->count; i++) {
		if (p_blink->flag & (1UL << i)) {
			if (p_blink->state) {
				p_owner->__state |= 1UL << i;
			}
			else {
				p_owner->__state &= ~(1UL << i);
			}
		}
	}
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

static void _evtTimerMBlinkFinish(void* p_issuer)
{
    PifLed* p_owner = (PifLed*)p_issuer;
	PifLedBlink* p_blink;
	uint8_t b, i;

    for (b = 0; b < p_owner->__blink_count; b++) {
    	p_blink = &p_owner->__p_blinks[b];
    	if (!p_blink->count) {
    		p_blink->count = p_blink->multiple - 1;

    		p_blink->state ^= 1;
			for (i = 0; i < p_owner->count; i++) {
				if (p_blink->flag & (1UL << i)) {
					if (p_blink->state) {
						p_owner->__state |= 1UL << i;
					}
					else {
						p_owner->__state &= ~(1UL << i);
					}
				}
			}
    	}
    	else {
    		p_blink->count--;
    	}
    }
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

BOOL pifLed_Init(PifLed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t count, PifActLedState act_state)
{
	if (!p_owner || !p_timer_manager || !count || count > 32 || !act_state) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifLed));

    p_owner->__p_timer_manager = p_timer_manager;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;
    p_owner->__act_state = act_state;
    return TRUE;
}

void pifLed_Clear(PifLed* p_owner)
{
	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

void pifLed_PartOn(PifLed* p_owner, uint8_t bits)
{
	p_owner->__state |= bits;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartOff(PifLed* p_owner, uint8_t bits)
{
	p_owner->__state &= ~bits;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartChange(PifLed* p_owner, uint8_t bits, SWITCH state)
{
	if (state) {
		p_owner->__state |= bits;
	}
	else {
		p_owner->__state &= ~bits;
	}
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_PartToggle(PifLed* p_owner, uint8_t bits)
{
	p_owner->__state ^= bits;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllOn(PifLed* p_owner)
{
	p_owner->__state = (1 << p_owner->count) - 1;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllOff(PifLed* p_owner)
{
	p_owner->__state = 0;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllChange(PifLed* p_owner, uint32_t state)
{
	p_owner->__state = state;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_AllToggle(PifLed* p_owner)
{
	p_owner->__state ^= (1 << p_owner->count) - 1;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

BOOL pifLed_AttachSBlink(PifLed* p_owner, uint16_t period1ms)
{
	return pifLed_AttachMBlink(p_owner, period1ms, 1, 1);
}

BOOL pifLed_AttachMBlink(PifLed* p_owner, uint16_t period1ms, uint8_t count, ...)
{
	va_list ap;
	int b;

	if (!period1ms || !count) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_blink) {
		p_owner->__p_timer_blink = pifTimerManager_Add(p_owner->__p_timer_manager, TT_REPEAT);
		if (!p_owner->__p_timer_blink) return FALSE;
	}

    pifTimer_Start(p_owner->__p_timer_blink, period1ms * 1000L / p_owner->__p_timer_manager->_period1us);

    p_owner->__p_blinks = calloc(count, sizeof(PifLedBlink));
    if (!p_owner->__p_blinks) {
    	pif_error = E_OUT_OF_HEAP;
    	return FALSE;
    }
    p_owner->__blink_count = count;

    if (count == 1) {
        pifTimer_AttachEvtFinish(p_owner->__p_timer_blink, _evtTimerSBlinkFinish, p_owner);

   		p_owner->__p_blinks[0].multiple = 1;
    }
    else {
        pifTimer_AttachEvtFinish(p_owner->__p_timer_blink, _evtTimerMBlinkFinish, p_owner);

    	va_start(ap, count);
    	for (b = 0; b < count; b++) {
    		p_owner->__p_blinks[b].multiple = va_arg(ap, int);
    		if (!p_owner->__p_blinks[b].multiple) {
    			free(p_owner->__p_blinks);
    			p_owner->__p_blinks = NULL;
    			p_owner->__blink_count = 0;
    	        pif_error = E_INVALID_PARAM;
    			return FALSE;
    		}
    	}
    	va_end(ap);
    }
	return TRUE;
}

void pifLed_DetachBlink(PifLed* p_owner)
{
    if (p_owner->__p_blinks) {
		free(p_owner->__p_blinks);
		p_owner->__p_blinks = NULL;
    }
	p_owner->__blink_count = 0;

	if (p_owner->__p_timer_blink) {
		pifTimerManager_Remove(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_blink || p_owner->__p_timer_blink->_step == TS_STOP) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer_blink->target = period1ms * 1000L / p_owner->__p_timer_manager->_period1us;
	return TRUE;
}

void pifLed_SBlinkOn(PifLed* p_owner, uint8_t bits)
{
    p_owner->__p_blinks[0].flag |= bits;
}

void pifLed_MBlinkOn(PifLed* p_owner, uint8_t bits, uint8_t index)
{
	uint8_t b;

    p_owner->__p_blinks[index].flag |= bits;
    for (b = 0; b < p_owner->__blink_count; b++) {
    	if (b != index && (p_owner->__p_blinks[b].flag & bits)) {
    		p_owner->__p_blinks[b].flag &= ~bits;
    	}
    }
}

void pifLed_SBlinkOff(PifLed* p_owner, uint8_t bits, SWITCH state)
{
	p_owner->__p_blinks[0].flag &= ~bits;
	pifLed_PartChange(p_owner, bits, state);
}

void pifLed_MBlinkOff(PifLed* p_owner, uint8_t bits, uint8_t index, SWITCH state)
{
	p_owner->__p_blinks[index].flag &= ~bits;
	pifLed_PartChange(p_owner, bits, state);
}
