#include "pif_log.h"
#include "pif_step_motor_pos.h"


static void _evtTimerDelayFinish(void* p_issuer)
{
    PifStepMotor* p_parent = (PifStepMotor*)p_issuer;

	switch (p_parent->_state) {
	case MS_BREAKING:
		p_parent->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "SMP(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifStepMotor *p_parent = p_task->_p_client;
	uint16_t tmp_pps = 0;
    PifStepMotorPos* p_owner = (PifStepMotorPos*)p_parent;
    const PifStepMotorPosStage* p_stage = p_owner->__p_current_stage;
    PifMotorState state = p_parent->_state;

    tmp_pps = p_parent->_current_pps;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_pps >= p_stage->fs_high_pps) {
			tmp_pps = p_stage->fs_high_pps;
			p_parent->_state = MS_CONST;
			if (p_parent->evt_stable) (*p_parent->evt_stable)(p_parent);
		}
		else {
			tmp_pps += p_stage->gs_ctrl_pps;
		}
	}
	else if (p_parent->_state == MS_REDUCE) {
		if (!p_stage->rs_ctrl_pps) {
			if (p_stage->mode & MM_PC_MASK) {
				tmp_pps = p_stage->rs_low_pps;
				p_parent->_state = MS_LOW_CONST;
			}
			else {
				tmp_pps = 0;
				p_parent->_state = MS_BREAK;
			}
		}
		else if (tmp_pps > p_stage->rs_low_pps + p_stage->rs_ctrl_pps) {
			tmp_pps -= p_stage->rs_ctrl_pps;
		}
		else if (tmp_pps) {
			tmp_pps = p_stage->rs_low_pps;
			p_parent->_state = MS_LOW_CONST;
		}
	}

	if (p_stage->mode & MM_PC_MASK) {
		if (p_parent->_current_pulse >= p_stage->total_pulse) {
			tmp_pps = 0;
			if (p_parent->_state < MS_BREAK) {
				p_parent->_state = MS_BREAK;
			}
		}
		else if (p_parent->_current_pulse >= p_stage->fs_pulse_count) {
			if (p_parent->_state < MS_REDUCE) {
				p_parent->_state = MS_REDUCE;
			}
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
			p_parent->_state = MS_BREAKING;
    	}
    	else {
    		p_parent->_state = MS_STOPPING;
    	}
	}

    if (p_parent->_state == MS_STOPPING) {
		if (!(p_stage->mode & MM_NR_MASK)) {
			pifStepMotor_Release(p_parent);
		}
		p_parent->_state = MS_STOP;

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
		pifLog_Printf(LT_INFO, "SMP(%u) %s P/S:%u CP:%u", p_parent->_id,
				kMotorState[p_parent->_state], tmp_pps, p_parent->_current_pulse);
	}
#endif

	pifStepMotor_Control(p_parent);
	return 0;
}

static void _evtSwitchReduceChange(PifId id, uint16_t level, void* p_issuer)
{
	PifStepMotorPos* p_owner = (PifStepMotorPos*)p_issuer;

	(void)id;

	if (p_owner->parent._state >= MS_REDUCE) return;

	if (level) {
		pifStepMotorPos_Stop(p_owner);
	}
}

static void _evtSwitchStopChange(PifId id, uint16_t level, void* p_issuer)
{
	PifStepMotor* p_parent = (PifStepMotor*)p_issuer;

	(void)id;

	if (p_parent->_state >= MS_BREAK) return;

	if (level) {
		p_parent->_state = MS_BREAK;
	}
}

static void _fnStopStep(PifStepMotor* p_parent)
{
	p_parent->_current_pps = 0;
	p_parent->_state = MS_BREAK;

#ifndef __PIF_NO_LOG__
    if (pif_log_flag.bt.step_motor) {
    	pifLog_Printf(LT_INFO, "SMP(%u) CP:%lu S:%d", p_parent->_id,
    			p_parent->_current_pulse, p_parent->_state);
    }
#endif
}

BOOL pifStepMotorPos_Init(PifStepMotorPos* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t resolution,
		PifStepMotorOperation operation, uint16_t period1ms)
{
    if (!p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifStepMotorPos));

    PifStepMotor* p_parent = &p_owner->parent;
    if (!pifStepMotor_Init(p_parent, id, p_timer_manager, resolution, operation)) goto fail;

    p_parent->__p_timer_delay = pifTimerManager_Add(p_parent->_p_timer_manager, TT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifTimer_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__stop_step = _fnStopStep;

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
	if (!p_parent->__p_task) goto fail;
    return TRUE;

fail:
	pifStepMotorPos_Clear(p_owner);
    return FALSE;
}

void pifStepMotorPos_Clear(PifStepMotorPos* p_owner)
{
    PifStepMotor* p_parent = &p_owner->parent;

    if (p_parent->__p_task) {
		pifTaskManager_Remove(p_parent->__p_task);
		p_parent->__p_task = NULL;
	}
	pifStepMotor_Clear(p_parent);
}

BOOL pifStepMotorPos_AddStages(PifStepMotorPos* p_owner, uint8_t stage_size, const PifStepMotorPosStage* p_stages)
{
    for (int i = 0; i < stage_size; i++) {
    	if (p_stages[i].mode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    p_owner->__stage_size = stage_size;
    p_owner->__p_stages = p_stages;
    return TRUE;
}

BOOL pifStepMotorPos_Start(PifStepMotorPos* p_owner, uint8_t stage_index, uint32_t operating_time)
{
    PifStepMotor* p_parent = &p_owner->parent;
    const PifStepMotorPosStage *pstStage;
    uint8_t state;

    if (!p_parent->act_set_step) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->__p_current_stage = &p_owner->__p_stages[stage_index];
    pstStage = p_owner->__p_current_stage;

    if (pstStage->mode & MM_CIAS_MASK) {
    	state = 0;
    	if (pstStage->p_start_sensor) {
    		if (pstStage->p_start_sensor->_curr_state != ON) {
    			state |= 1;
    		}
    	}
    	if (pstStage->p_reduce_sensor) {
    		if (pstStage->p_reduce_sensor->_curr_state != OFF) {
    			state |= 2;
    		}
    	}
    	if (pstStage->p_stop_sensor) {
    		if (pstStage->p_stop_sensor->_curr_state != OFF) {
    			state |= 4;
    		}
    	}
    	if (state) {
        	pif_error = E_INVALID_STATE;
		    return FALSE;
    	}
    }

    if ((pstStage->mode & MM_RT_MASK) == MM_RT_TIME) {
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

    if (pstStage->p_reduce_sensor) {
        pifSensor_AttachEvtChange(pstStage->p_reduce_sensor, _evtSwitchReduceChange, p_owner);
    }

    if (pstStage->p_stop_sensor) {
        pifSensor_AttachEvtChange(pstStage->p_stop_sensor, _evtSwitchStopChange, p_parent);
    }

    p_parent->_direction = (pstStage->mode & MM_D_MASK) >> MM_D_SHIFT;

    if (pstStage->gs_ctrl_pps) {
    	pifStepMotor_SetPps(p_parent, pstStage->gs_start_pps);
    	p_parent->_state = MS_GAINED;
    }
    else {
    	pifStepMotor_SetPps(p_parent, pstStage->fs_high_pps);
    	p_parent->_state = MS_CONST;
    }
    p_parent->__error = 0;

    if (pstStage->mode & MM_PC_MASK) {
    	pifStepMotor_Start(p_parent, pstStage->total_pulse);
    }
    else {
    	pifStepMotor_Start(p_parent, 0);
    }
    return TRUE;
}

void pifStepMotorPos_Stop(PifStepMotorPos* p_owner)
{
    PifStepMotor* p_parent = &p_owner->parent;

    if (p_parent->_state == MS_IDLE) return;

    p_parent->_state = MS_REDUCE;
}

void pifStepMotorPos_Emergency(PifStepMotorPos* p_owner)
{
	p_owner->parent._state = MS_BREAK;
}
