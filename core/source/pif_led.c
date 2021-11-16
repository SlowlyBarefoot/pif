#include "pif_led.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


static void _evtTimerBlinkFinish(void* p_issuer)
{
	BOOL bBlink = FALSE;

    PifLed* p_owner = (PifLed*)p_issuer;
    p_owner->__blink ^= 1;
    for (uint8_t i = 0; i < p_owner->count; i++) {
    	if (p_owner->__blink_flag & (1 << i)) {
    		if (p_owner->__blink) {
    			p_owner->__state |= 1 << i;
    		}
    		else {
    			p_owner->__state &= ~(1 << i);
    		}
    		bBlink = TRUE;
    	}
    }
    if (bBlink) (*p_owner->__act_state)(p_owner->_id, p_owner->__state);
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

void pifLed_EachOn(PifLed* p_owner, uint8_t index)
{
	p_owner->__state |= 1 << index;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_EachOff(PifLed* p_owner, uint8_t index)
{
	p_owner->__state &= ~(1 << index);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_EachChange(PifLed* p_owner, uint8_t index, SWITCH state)
{
	if (state) {
		p_owner->__state |= 1 << index;
	}
	else {
		p_owner->__state &= ~(1 << index);
	}
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

void pifLed_EachToggle(PifLed* p_owner, uint8_t index)
{
	p_owner->__state ^= 1 << index;
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

BOOL pifLed_AttachBlink(PifLed* p_owner, uint16_t period1ms)
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

	p_owner->__blink_flag = 0L;
    pifTimer_Start(p_owner->__p_timer_blink, period1ms * 1000L / p_owner->__p_timer_manager->_period1us);
	return TRUE;
}

void pifLed_DetachBlink(PifLed* p_owner)
{
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

void pifLed_BlinkOn(PifLed* p_owner, uint8_t index)
{
    p_owner->__blink_flag |= 1 << index;
}

void pifLed_BlinkOff(PifLed* p_owner, uint8_t index)
{
	p_owner->__blink_flag &= ~(1 << index);
	pifLed_EachOff(p_owner, index);
}
