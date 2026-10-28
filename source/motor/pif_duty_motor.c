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
    if (p_owner->_error) {
        if (p_owner->_state < MS_BRAKE) {
			pifDutyMotor_ApplyDuty(p_owner, 0);
			p_owner->_current_duty = 0;

#ifndef PIF_NO_LOG
			pifDutyMotor_SetState(p_owner, MS_BRAKE, "DM");
#else
			pifDutyMotor_SetState(p_owner, MS_BRAKE);
#endif
        }

        if (!p_owner->__error_notified) {
            p_owner->__error_notified = TRUE;
            if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
        }
    }

	if (p_owner->_state == MS_STOP) {
		// Cancel a pending operating time so it does not cut into the next run.
		if (p_owner->__p_timer_brake) pifTimer_Stop(p_owner->__p_timer_brake);
		pifTask_Pause(p_owner->_p_task);
#ifndef PIF_NO_LOG
		pifDutyMotor_SetState(p_owner, MS_IDLE, "DM");
#else
		pifDutyMotor_SetState(p_owner, MS_IDLE);
#endif
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
}

/**
 * @brief Brake timer callback that transitions to reduce state or releases brake.
 * @param p_issuer Issuer pointer cast to `PifDutyMotor*`.
 */
static void _evtTimerBrakeFinish(PifIssuerP p_issuer)
{
    PifDutyMotor* p_owner = (PifDutyMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
#ifndef PIF_NO_LOG
		pifDutyMotor_SetState(p_owner, MS_REDUCE, "DM");
#else
		pifDutyMotor_SetState(p_owner, MS_REDUCE);
#endif
    }
    else if (p_owner->act_operate_brake) {
    	(*p_owner->act_operate_brake)(p_owner, 0);
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
#ifdef COLLECT_SIGNAL_DUTY_MOTOR
	pifDutyMotor_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, DM_CSF_ALL_BIT);
#endif
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	if (p_owner->__p_timer_brake) {
		pifTimerManager_Remove(p_owner->__p_timer_brake);
		p_owner->__p_timer_brake = NULL;
	}
}

#ifndef PIF_NO_LOG

void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state, const char *tag)
{
	pifLog_Printf(LT_INFO, "%s(%u) %s->%s E:%d", tag, p_owner->_id,
			kMotorState[p_owner->_state], kMotorState[state], p_owner->_error);
	p_owner->_state = state;
#ifdef COLLECT_SIGNAL_DUTY_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_STATE_IDX], state);
#endif
}

#else

void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state)
{
	p_owner->_state = state;
#ifdef COLLECT_SIGNAL_DUTY_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_STATE_IDX], state);
#endif
}

#endif

void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction)
{
	p_owner->_direction = direction;

	if (p_owner->act_set_direction) (*p_owner->act_set_direction)(p_owner, p_owner->_direction);
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

	if (!p_owner->__p_timer_brake) {
		p_owner->__p_timer_brake = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	}
	if (p_owner->__p_timer_brake) {
		pifTimer_AttachEvtFinish(p_owner->__p_timer_brake, _evtTimerBrakeFinish, p_owner, FALSE);
		if (pifTimer_Start(p_owner->__p_timer_brake, operating_time)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty)
{
    if (!p_owner->act_set_duty) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    // act_set_direction is optional, for motors that turn one way only.
    if (p_owner->act_set_direction) (*p_owner->act_set_direction)(p_owner, p_owner->_direction);

    if (duty > p_owner->_max_duty) duty = p_owner->_max_duty;
   	p_owner->_current_duty = duty;

    pifDutyMotor_ApplyDuty(p_owner, p_owner->_current_duty);
	return TRUE;
}

void pifDutyMotor_BrakeRelease(PifDutyMotor* p_owner, uint16_t brake_time)
{
    p_owner->_current_duty = 0;

    pifDutyMotor_ApplyDuty(p_owner, p_owner->_current_duty);

    if (brake_time && p_owner->act_operate_brake) {
	    if (!p_owner->__p_timer_brake) {
	    	p_owner->__p_timer_brake = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	    }
	    if (p_owner->__p_timer_brake) {
	    	pifTimer_AttachEvtFinish(p_owner->__p_timer_brake, _evtTimerBrakeFinish, p_owner, FALSE);
			if (pifTimer_Start(p_owner->__p_timer_brake, brake_time)) {
		    	(*p_owner->act_operate_brake)(p_owner, 1);
			}
	    }
	}
}

BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner)
{
	if (!p_owner->_p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__error_notified = FALSE;
    pifTask_Resume(p_owner->_p_task);
    return TRUE;
}

BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner)
{
	if (!p_owner->_p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    pifTask_Pause(p_owner->_p_task);
    return TRUE;
}

void pifDutyMotor_ApplyDuty(PifDutyMotor* p_owner, uint16_t duty)
{
	if (p_owner->act_set_duty) (*p_owner->act_set_duty)(p_owner, duty);
#ifdef COLLECT_SIGNAL_DUTY_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[DM_CSF_DUTY_IDX], duty);
#endif
}

#ifdef COLLECT_SIGNAL_DUTY_MOTOR

BOOL pifDutyMotor_SetCsFlag(PifDutyMotor* p_owner, uint8_t index, PifDutyMotorCsFlag flag)
{
	(void)index;
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

void pifDutyMotor_ResetCsFlag(PifDutyMotor* p_owner, uint8_t index, PifDutyMotorCsFlag flag)
{
	(void)index;
	if (flag & DM_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DM_CSF_STATE_IDX]);
	if (flag & DM_CSF_DUTY_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DM_CSF_DUTY_IDX]);
}

#endif	// COLLECT_SIGNAL_DUTY_MOTOR
