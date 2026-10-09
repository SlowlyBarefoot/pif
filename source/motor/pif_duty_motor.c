// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "motor/pif_duty_motor.h"


/**
 * @brief Processes runtime errors and stop-state cleanup for the duty motor.
 * @param p_owner Pointer to the duty motor instance.
 */
void pifDutyMotor_Control(PifDutyMotor* p_owner)
{
    if (p_owner->__error) {
        if (p_owner->_state < MS_BREAK) {
			pifDutyMotor_ApplyDuty(p_owner, 0);
			p_owner->_current_duty = 0;

#ifndef PIF_NO_LOG
			pifDutyMotor_SetState(p_owner, MS_BREAK, "DM");
#else
			pifDutyMotor_SetState(p_owner, MS_BREAK);
#endif
        }

        if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
    }

	if (p_owner->_state == MS_STOP) {
		p_owner->__p_task->pause = TRUE;
#ifndef PIF_NO_LOG
		pifDutyMotor_SetState(p_owner, MS_IDLE, "DM");
#else
		pifDutyMotor_SetState(p_owner, MS_IDLE);
#endif
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
}

/**
 * @brief Break timer callback that transitions to reduce state or releases break.
 * @param p_issuer Issuer pointer cast to `PifDutyMotor*`.
 */
static void _evtTimerBreakFinish(PifIssuerP p_issuer)
{
    PifDutyMotor* p_owner = (PifDutyMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
#ifndef PIF_NO_LOG
		pifDutyMotor_SetState(p_owner, MS_REDUCE, "DM");
#else
		pifDutyMotor_SetState(p_owner, MS_REDUCE);
#endif
    }
    else {
    	(*p_owner->act_operate_break)(0);
    }
}

BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty)
{
    if (!p_owner || !p_timer_manager || !max_duty) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDutyMotor));

    p_owner->_p_timer_manager = p_timer_manager;
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_max_duty = max_duty;
    return TRUE;
}

void pifDutyMotor_Clear(PifDutyMotor* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifDutyMotor_ResetCsFlag(p_owner, DM_CSF_ALL_BIT);
#endif
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	if (p_owner->__p_timer_break) {
		pifTimerManager_Remove(p_owner->__p_timer_break);
		p_owner->__p_timer_break = NULL;
	}
}

#ifndef PIF_NO_LOG

void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state, char *tag)
{
	pifLog_Printf(LT_INFO, "%s(%u) %s->%s E:%d", tag, p_owner->_id,
			kMotorState[p_owner->_state], kMotorState[state], p_owner->__error);
	p_owner->_state = state;
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_STATE_IDX], state);
#endif
}

#else

PIF_INLINE void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state)
{
	p_owner->_state = state;
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_STATE_IDX], state);
#endif
}

#endif

void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction)
{
	p_owner->_direction = direction;

	if (p_owner->act_set_direction) (*p_owner->act_set_direction)(p_owner->_direction);
}

void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty)
{
    if (duty > p_owner->_max_duty) duty = p_owner->_max_duty;
	p_owner->_current_duty = duty;

	pifDutyMotor_ApplyDuty(p_owner, p_owner->_current_duty);
}

BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time)
{
	if (!operating_time) return TRUE;

	if (!p_owner->__p_timer_break) {
		p_owner->__p_timer_break = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	}
	if (p_owner->__p_timer_break) {
		pifTimer_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
		if (pifTimer_Start(p_owner->__p_timer_break, operating_time)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty)
{
    if (!p_owner->act_set_duty || !p_owner->act_set_direction) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    (*p_owner->act_set_direction)(p_owner->_direction);

   	p_owner->_current_duty = duty;

    pifDutyMotor_ApplyDuty(p_owner, p_owner->_current_duty);
	return TRUE;
}

void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time)
{
    p_owner->_current_duty = 0;

    pifDutyMotor_ApplyDuty(p_owner, p_owner->_current_duty);

    if (break_time && p_owner->act_operate_break) {
	    if (!p_owner->__p_timer_break) {
	    	p_owner->__p_timer_break = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	    }
	    if (p_owner->__p_timer_break) {
	    	pifTimer_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
			if (pifTimer_Start(p_owner->__p_timer_break, break_time)) {
		    	(*p_owner->act_operate_break)(1);
			}
	    }
	}
}

BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = FALSE;
    return TRUE;
}

BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = TRUE;
    return TRUE;
}

void pifDutyMotor_ApplyDuty(PifDutyMotor* p_owner, uint16_t duty)
{
	(*p_owner->act_set_duty)(duty);
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_DUTY_IDX], duty);
#endif
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifDutyMotor_SetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag)
{
	if (flag & DM_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[DM_CSF_STATE_IDX], "DMS", p_owner->_id, CSVT_REG, 4,
				p_owner->_state)) return FALSE;
	}
	if (flag & DM_CSF_DUTY_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[DM_CSF_DUTY_IDX], "DMD", p_owner->_id, CSVT_INTEGER, 16,
				p_owner->_current_duty)) return FALSE;
	}
	return TRUE;
}

void pifDutyMotor_ResetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag)
{
	if (flag & DM_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DM_CSF_STATE_IDX]);
	if (flag & DM_CSF_DUTY_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DM_CSF_DUTY_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
