#include "pifLed.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
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

/**
 * @fn pifLed_Create
 * @brief
 * @param id
 * @param p_timer
 * @param count
 * @param act_state
 * @return
 */
PifLed* pifLed_Create(PifId id, PifPulse* p_timer, uint8_t count, PifActLedState act_state)
{
	PifLed* p_owner = malloc(sizeof(PifLed));
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (!pifLed_Init(p_owner, id, p_timer, count, act_state)) {
		pifLed_Destroy(&p_owner);
	    return NULL;
	}
    return p_owner;
}

/**
 * @fn pifLed_Destroy
 * @brief
 * @param pp_owner
 */
void pifLed_Destroy(PifLed** pp_owner)
{
	if (*pp_owner) {
		pifLed_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifLed_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer
 * @param count
 * @param act_state
 * @return
 */
BOOL pifLed_Init(PifLed* p_owner, PifId id, PifPulse* p_timer, uint8_t count, PifActLedState act_state)
{
	if (!p_owner || !p_timer || !count || count > 32 || !act_state) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifLed));

    p_owner->__p_timer = p_timer;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;
    p_owner->__act_state = act_state;
    return TRUE;
}

/**
 * @fn pifLed_Clear
 * @brief
 * @param p_owner
 */
void pifLed_Clear(PifLed* p_owner)
{
	if (p_owner->__p_timer_blink) {
		pifPulse_RemoveItem(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

/**
 * @fn pifLed_EachOn
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachOn(PifLed* p_owner, uint8_t index)
{
	p_owner->__state |= 1 << index;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_EachOff
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachOff(PifLed* p_owner, uint8_t index)
{
	p_owner->__state &= ~(1 << index);
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_EachChange
 * @brief
 * @param p_owner
 * @param index
 * @param state
 */
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

/**
 * @fn pifLed_EachToggle
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_EachToggle(PifLed* p_owner, uint8_t index)
{
	p_owner->__state ^= 1 << index;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_AllOn
 * @brief
 * @param p_owner
 */
void pifLed_AllOn(PifLed* p_owner)
{
	p_owner->__state = (1 << p_owner->count) - 1;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_AllOff
 * @brief
 * @param p_owner
 */
void pifLed_AllOff(PifLed* p_owner)
{
	p_owner->__state = 0;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_AllChange
 * @brief
 * @param p_owner
 * @param state
 */
void pifLed_AllChange(PifLed* p_owner, uint32_t state)
{
	p_owner->__state = state;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_AllToggle
 * @brief
 * @param p_owner
 */
void pifLed_AllToggle(PifLed* p_owner)
{
	p_owner->__state ^= (1 << p_owner->count) - 1;
	(*p_owner->__act_state)(p_owner->_id, p_owner->__state);
}

/**
 * @fn pifLed_AttachBlink
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifLed_AttachBlink(PifLed* p_owner, uint16_t period1ms)
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

	p_owner->__blink_flag = 0L;
    pifPulse_StartItem(p_owner->__p_timer_blink, period1ms * 1000L / p_owner->__p_timer->_period1us);
	return TRUE;
}

/**
 * @fn pifLed_DetachBlink
 * @brief
 * @param p_owner
 */
void pifLed_DetachBlink(PifLed* p_owner)
{
	if (p_owner->__p_timer_blink) {
		pifPulse_RemoveItem(p_owner->__p_timer_blink);
		p_owner->__p_timer_blink = NULL;
	}
}

/**
 * @fn pifLed_ChangeBlinkPeriod
 * @brief
 * @param p_owner
 * @param period1ms
 * @return
 */
BOOL pifLed_ChangeBlinkPeriod(PifLed* p_owner, uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	if (!p_owner->__p_timer_blink || p_owner->__p_timer_blink->_step == PS_STOP) {
        pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer_blink->target = period1ms * 1000L / p_owner->__p_timer->_period1us;
	return TRUE;
}

/**
 * @fn pifLed_BlinkOn
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_BlinkOn(PifLed* p_owner, uint8_t index)
{
    p_owner->__blink_flag |= 1 << index;
}

/**
 * @fn pifLed_BlinkOff
 * @brief
 * @param p_owner
 * @param index
 */
void pifLed_BlinkOff(PifLed* p_owner, uint8_t index)
{
	p_owner->__blink_flag &= ~(1 << index);
	pifLed_EachOff(p_owner, index);
}
