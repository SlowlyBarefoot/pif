#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "motor/pif_duty_motor_speed.h"


static void _evtTimerDelayFinish(PifIssueP p_issuer)
{
    PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	switch (p_parent->_state) {
	case MS_OVER_RUN:
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_REDUCE, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_REDUCE);
#endif
        break;

	case MS_BREAKING:
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_STOPPING, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_STOPPING);
#endif
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMS(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifDutyMotor* p_parent = p_task->_p_client;
	uint16_t tmp_duty = 0;
    PifDutyMotorSpeed* p_owner = (PifDutyMotorSpeed*)p_parent;
    const PifDutyMotorSpeedStage* p_stage = p_owner->__p_current_stage;
    uint16_t pre_duty = p_parent->_current_duty;

    tmp_duty = p_parent->_current_duty;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_duty >= p_stage->fs_high_duty) {
			tmp_duty = p_stage->fs_high_duty;
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_CONST, "DMS");
#else
			pifDutyMotor_SetState(p_parent, MS_CONST);
#endif
			if (p_parent->evt_stable) (*p_parent->evt_stable)(p_parent);
		}
		else {
			tmp_duty += p_stage->gs_ctrl_duty;
		}
	}
	else if (p_parent->_state == MS_REDUCE) {
		if (!p_stage->rs_ctrl_duty) {
			tmp_duty = 0;
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_BREAK, "DMS");
#else
			pifDutyMotor_SetState(p_parent, MS_BREAK);
#endif
		}
		else if (tmp_duty > p_stage->rs_ctrl_duty && tmp_duty > p_stage->rs_low_duty) {
			tmp_duty -= p_stage->rs_ctrl_duty;
		}
		else if (tmp_duty) {
			tmp_duty = 0;
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_BREAK, "DMS");
#else
			pifDutyMotor_SetState(p_parent, MS_BREAK);
#endif
		}
	}

	if (tmp_duty != p_parent->_current_duty) {
		(*p_parent->act_set_duty)(tmp_duty);
		p_parent->_current_duty = tmp_duty;
	}

    if (p_parent->_state == MS_BREAK) {
    	if (p_parent->act_operate_break && p_stage->rs_break_time &&
    			pifTimer_Start(p_parent->__p_timer_delay, p_stage->rs_break_time)) {
			(*p_parent->act_operate_break)(1);
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_BREAKING, "DMS");
#else
			pifDutyMotor_SetState(p_parent, MS_BREAKING);
#endif
    	}
    	else {
			if (p_parent->act_operate_break) (*p_parent->act_operate_break)(1);
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_STOPPING, "DMS");
#else
			pifDutyMotor_SetState(p_parent, MS_STOPPING);
#endif
    	}
	}

    if (p_parent->_state == MS_STOPPING) {
		if (!(p_stage->mode & MM_NR_MASK)) {
			if (p_parent->act_operate_break) (*p_parent->act_operate_break)(0);
		}
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_STOP, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_STOP);
#endif

		if (p_stage->mode & MM_CFPS_MASK) {
	    	if (p_stage->p_stop_sensor) {
	    		if (p_stage->p_stop_sensor->_curr_state == OFF) {
	    			p_parent->__error = 1;
	    		}
	    	}
	    }

		if (p_stage->p_reduce_sensor) {
			pifSensor_DetachEvtChange(p_stage->p_reduce_sensor);
		}
		if (p_stage->p_stop_sensor) {
			pifSensor_DetachEvtChange(p_stage->p_stop_sensor);
		}
    }

#ifndef __PIF_NO_LOG__
	if (pif_log_flag.bt.duty_motor && pre_duty != tmp_duty) {
		pifLog_Printf(LT_INFO, "DMS(%u) %s D:%u->%u(%u%%) E:%d", p_parent->_id, kMotorState[p_parent->_state],
				pre_duty, tmp_duty, (uint16_t)(100 * tmp_duty / p_parent->_max_duty), p_parent->__error);
	}
#endif

	pifDutyMotor_Control(p_parent);
	return 0;
}

static void _evtSwitchReduceChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssueP p_issuer)
{
	PifDutyMotorSpeed* p_motor = (PifDutyMotorSpeed*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->parent._state >= MS_REDUCE) return;

	if (state) {
		pifDutyMotorSpeed_Stop(p_motor);
	}
}

static void _evtSwitchStopChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, void *p_issuer)
{
	PifDutyMotor* p_motor = (PifDutyMotor*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->_state >= MS_BREAK) return;

	if (state) {
		p_motor->_current_duty = 0;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_motor, MS_BREAK, "DMS");
#else
		pifDutyMotor_SetState(p_motor, MS_BREAK);
#endif
	}
}

BOOL pifDutyMotorSpeed_Init(PifDutyMotorSpeed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty, uint16_t period1ms)
{
    if (!p_timer_manager || !max_duty) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDutyMotorSpeed));

    PifDutyMotor *p_parent = &p_owner->parent;
    if (!pifDutyMotor_Init(p_parent, id, p_timer_manager, max_duty)) goto fail;

    p_parent->__p_timer_delay = pifTimerManager_Add(p_parent->_p_timer_manager, TT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifTimer_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
	if (!p_parent->__p_task) goto fail;
    return TRUE;

fail:
	pifDutyMotorSpeed_Clear(p_owner);
    return FALSE;
}

void pifDutyMotorSpeed_Clear(PifDutyMotorSpeed* p_owner)
{
    PifDutyMotor *p_parent = &p_owner->parent;

    if (p_parent->__p_task) {
		pifTaskManager_Remove(p_parent->__p_task);
		p_parent->__p_task = NULL;
	}
	pifDutyMotor_Clear(p_parent);
}

BOOL pifDutyMotorSpeed_AddStages(PifDutyMotorSpeed* p_owner, uint8_t stage_size, const PifDutyMotorSpeedStage* p_stages)
{
    for (int i = 0; i < stage_size; i++) {
    	if (p_stages[i].mode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	if (p_stages[i].mode & MM_PC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    p_owner->__stage_size = stage_size;
    p_owner->__p_stages = p_stages;
    return TRUE;
}

BOOL pifDutyMotorSpeed_Start(PifDutyMotorSpeed* p_owner, uint8_t stage_index, uint32_t operating_time)
{
    PifDutyMotor* p_parent = &p_owner->parent;
    const PifDutyMotorSpeedStage* p_stage;
    uint8_t state;

    if (!p_parent->act_set_duty || !p_parent->act_set_direction) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->__p_current_stage = &p_owner->__p_stages[stage_index];
    p_stage = p_owner->__p_current_stage;

    if (p_stage->mode & MM_CIAS_MASK) {
    	state = 0;
    	if (p_stage->p_start_sensor) {
    		if (p_stage->p_start_sensor->_curr_state != ON) {
    			state |= 1;
    		}
    	}
    	if (p_stage->p_reduce_sensor) {
    		if (p_stage->p_reduce_sensor->_curr_state != OFF) {
    			state |= 2;
    		}
    	}
    	if (p_stage->p_stop_sensor) {
    		if (p_stage->p_stop_sensor->_curr_state != OFF) {
    			state |= 4;
    		}
    	}
    	if (state) {
        	pif_error = E_INVALID_STATE;
		    return FALSE;
    	}
    }

    if ((p_stage->mode & MM_RT_MASK) == MM_RT_TIME) {
    	if (!operating_time) {
        	pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    	else {
    		if (!pifDutyMotor_SetOperatingTime(p_parent, operating_time)) return FALSE;
    	}
    }

    if (!pifDutyMotor_StartControl(p_parent)) return FALSE;

    p_owner->_stage_index = stage_index;

    if (p_stage->p_reduce_sensor) {
        pifSensor_AttachEvtChange(p_stage->p_reduce_sensor, _evtSwitchReduceChange);
    }

    if (p_stage->p_stop_sensor) {
        pifSensor_AttachEvtChange(p_stage->p_stop_sensor, _evtSwitchStopChange);
    }

    if (p_parent->act_set_direction) (*p_parent->act_set_direction)((p_stage->mode & MM_D_MASK) >> MM_D_SHIFT);

    if (p_stage->gs_ctrl_duty) {
    	p_parent->_current_duty = p_stage->gs_start_duty;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_GAINED, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_GAINED);
#endif
    }
    else {
    	p_parent->_current_duty = p_stage->fs_high_duty;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_CONST, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_CONST);
#endif
    }
    p_parent->__error = 0;

    (*p_parent->act_set_duty)(p_parent->_current_duty);
    return TRUE;
}

void pifDutyMotorSpeed_Stop(PifDutyMotorSpeed* p_owner)
{
    const PifDutyMotorSpeedStage* p_stage = p_owner->__p_current_stage;
    PifDutyMotor* p_parent = &p_owner->parent;

    if (p_parent->_state == MS_IDLE) return;

    if (p_stage->fs_overtime && pifTimer_Start(p_parent->__p_timer_delay, p_stage->fs_overtime)) {
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_OVER_RUN, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_OVER_RUN);
#endif
    }
    else {
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_REDUCE, "DMS");
#else
		pifDutyMotor_SetState(p_parent, MS_REDUCE);
#endif
    }
}

void pifDutyMotorSpeed_Emergency(PifDutyMotorSpeed* p_owner)
{
    PifDutyMotor* p_parent = &p_owner->parent;

    p_parent->_current_duty = 0;
#ifndef __PIF_NO_LOG__
	pifDutyMotor_SetState(p_parent, MS_BREAK, "DMS");
#else
	pifDutyMotor_SetState(p_parent, MS_BREAK);
#endif
}
