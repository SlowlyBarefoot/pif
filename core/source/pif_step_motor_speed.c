#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_step_motor_speed.h"


static void _evtTimerDelayFinish(void* p_issuer)
{
    PifStepMotor* p_parent = (PifStepMotor*)p_issuer;

	switch (p_parent->_state) {
	case MS_OVER_RUN:
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_REDUCE, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_REDUCE);
#endif
        break;

	case MS_BREAKING:
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_STOPPING, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_STOPPING);
#endif
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "SMS(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifStepMotor *p_parent = p_task->_p_client;
	uint16_t tmp_pps = 0;
    PifStepMotorSpeed* p_owner = (PifStepMotorSpeed*)p_parent;
    const PifStepMotorSpeedStage* p_stage = p_owner->__p_current_stage;
    PifMotorState state = p_parent->_state;

    tmp_pps = p_parent->_current_pps;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_pps >= p_stage->fs_fixed_pps) {
			tmp_pps = p_stage->fs_fixed_pps;
#ifndef __PIF_NO_LOG__
			pifStepMotor_SetState(p_parent, MS_CONST, "SMS");
#else
			pifStepMotor_SetState(p_parent, MS_CONST);
#endif
			if (p_parent->evt_stable) (*p_parent->evt_stable)(p_parent);
		}
		else {
			tmp_pps += p_stage->gs_ctrl_pps;
		}
	}
	else if (p_parent->_state == MS_REDUCE) {
		if (!p_stage->rs_ctrl_pps) {
			tmp_pps = 0;
#ifndef __PIF_NO_LOG__
			pifStepMotor_SetState(p_parent, MS_BREAK, "SMS");
#else
			pifStepMotor_SetState(p_parent, MS_BREAK);
#endif
		}
		else if (tmp_pps > p_stage->rs_ctrl_pps && tmp_pps > p_stage->rs_stop_pps) {
			tmp_pps -= p_stage->rs_ctrl_pps;
		}
		else if (tmp_pps) {
			tmp_pps = 0;
#ifndef __PIF_NO_LOG__
			pifStepMotor_SetState(p_parent, MS_BREAK, "SMS");
#else
			pifStepMotor_SetState(p_parent, MS_BREAK);
#endif
		}
	}

	if (tmp_pps != p_parent->_current_pps) {
		if (tmp_pps) pifStepMotor_SetPps(p_parent, tmp_pps);
		p_parent->_current_pps = tmp_pps;
	}

    if (p_parent->_state == MS_BREAK) {
    	pifStepMotor_Break(p_parent);
		if (p_stage->rs_break_time &&
				pifTimer_Start(p_parent->__p_timer_delay, p_stage->rs_break_time)) {
#ifndef __PIF_NO_LOG__
			pifStepMotor_SetState(p_parent, MS_BREAKING, "SMS");
#else
			pifStepMotor_SetState(p_parent, MS_BREAKING);
#endif
    	}
    	else {
#ifndef __PIF_NO_LOG__
			pifStepMotor_SetState(p_parent, MS_STOPPING, "SMS");
#else
			pifStepMotor_SetState(p_parent, MS_STOPPING);
#endif
    	}
	}

    if (p_parent->_state == MS_STOPPING) {
		if (!(p_stage->mode & MM_NR_MASK)) {
			pifStepMotor_Release(p_parent);
		}
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_STOP, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_STOP);
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
	if (state != p_parent->_state && pif_log_flag.bt.step_motor) {
		pifLog_Printf(LT_INFO, "SMS(%u) %s P/S:%u", p_parent->_id,
				kMotorState[p_parent->_state], tmp_pps);
	}
#endif

	pifStepMotor_Control(p_parent);
	return 0;
}

static void _evtSwitchReduceChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, void* p_issuer)
{
	PifStepMotorSpeed* p_motor = (PifStepMotorSpeed*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->parent._state >= MS_REDUCE) return;

	if (state) {
		pifStepMotorSpeed_Stop(p_motor);
	}
}

static void _evtSwitchStopChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, void* p_issuer)
{
	PifStepMotor* p_motor = (PifStepMotor*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->_state >= MS_BREAK) return;

	if (state) {
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_motor, MS_BREAK, "SMS");
#else
		pifStepMotor_SetState(p_motor, MS_BREAK);
#endif
	}
}

BOOL pifStepMotorSpeed_Init(PifStepMotorSpeed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t resolution,
		PifStepMotorOperation operation, uint16_t period1ms)
{
    if (!p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifStepMotorSpeed));

    PifStepMotor* p_parent = (PifStepMotor*)&p_owner->parent;
    if (!pifStepMotor_Init(p_parent, id, p_timer_manager, resolution, operation)) goto fail;

    p_parent->__p_timer_delay = pifTimerManager_Add(p_parent->_p_timer_manager, TT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifTimer_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
	if (!p_parent->__p_task) goto fail;
    return TRUE;

fail:
	pifStepMotorSpeed_Clear(p_owner);
    return FALSE;
}

void pifStepMotorSpeed_Clear(PifStepMotorSpeed* p_owner)
{
    PifStepMotor* p_parent = (PifStepMotor*)&p_owner->parent;

    if (p_parent->__p_task) {
		pifTaskManager_Remove(p_parent->__p_task);
		p_parent->__p_task = NULL;
	}
	pifStepMotor_Clear(p_parent);
}

BOOL pifStepMotorSpeed_AddStages(PifStepMotorSpeed* p_owner, uint8_t stage_size, const PifStepMotorSpeedStage* p_stages)
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

BOOL pifStepMotorSpeed_Start(PifStepMotorSpeed* p_owner, uint8_t stage_index, uint32_t operating_time)
{
    PifStepMotor* p_parent = (PifStepMotor*)&p_owner->parent;
    const PifStepMotorSpeedStage* p_stage;
    uint8_t state;

    if (!p_parent->act_set_step) {
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
#ifndef __PIF_NO_LOG__
    		pifLog_Printf(LT_ERROR, "SMS(%u) S:%u", p_parent->_id, state);
#endif
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
    		if (!pifStepMotor_SetOperatingTime(p_parent, operating_time)) return FALSE;
    	}
    }

    if (!pifStepMotor_StartControl(p_parent)) return FALSE;

    p_owner->_stage_index = stage_index;

    if (p_stage->p_reduce_sensor) {
        pifSensor_AttachEvtChange(p_stage->p_reduce_sensor, _evtSwitchReduceChange);
    }

    if (p_stage->p_stop_sensor) {
        pifSensor_AttachEvtChange(p_stage->p_stop_sensor, _evtSwitchStopChange);
    }

    p_parent->_direction = (p_stage->mode & MM_D_MASK) >> MM_D_SHIFT;

    if (p_stage->gs_ctrl_pps) {
    	pifStepMotor_SetPps(p_parent, p_stage->gs_start_pps);
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_GAINED, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_GAINED);
#endif
    }
    else {
    	pifStepMotor_SetPps(p_parent, p_stage->fs_fixed_pps);
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_CONST, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_CONST);
#endif
    }
    p_parent->__error = 0;

    pifStepMotor_Start(p_parent, 0);
    return TRUE;
}

void pifStepMotorSpeed_Stop(PifStepMotorSpeed* p_owner)
{
    const PifStepMotorSpeedStage* p_stage = p_owner->__p_current_stage;
    PifStepMotor* p_parent = (PifStepMotor*)&p_owner->parent;

    if (p_parent->_state == MS_IDLE) return;

    if (p_stage->fs_overtime && pifTimer_Start(p_parent->__p_timer_delay, p_stage->fs_overtime)) {
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_OVER_RUN, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_OVER_RUN);
#endif
    }
    else {
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_parent, MS_REDUCE, "SMS");
#else
		pifStepMotor_SetState(p_parent, MS_REDUCE);
#endif
    }
}

void pifStepMotorSpeed_Emergency(PifStepMotorSpeed* p_owner)
{
#ifndef __PIF_NO_LOG__
	pifStepMotor_SetState(&p_owner->parent, MS_BREAK, "SMS");
#else
	pifStepMotor_SetState(&p_owner->parent, MS_BREAK);
#endif
}
