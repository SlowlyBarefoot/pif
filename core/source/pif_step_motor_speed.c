#include "pif_log.h"
#include "pif_step_motor_speed.h"


static void _evtTimerDelayFinish(void* p_issuer)
{
    PifStepMotor* p_parent = (PifStepMotor*)p_issuer;

	switch (p_parent->_state) {
	case MS_OVER_RUN:
        p_parent->_state = MS_REDUCE;
        break;

	case MS_BREAKING:
        p_parent->_state = MS_STOPPING;
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
			p_parent->_state = MS_CONST;
			if (p_parent->evt_stable) (*p_parent->evt_stable)(p_parent);
		}
		else {
			tmp_pps += p_stage->gs_ctrl_pps;
		}
	}
	else if (p_parent->_state == MS_REDUCE) {
		if (!p_stage->rs_ctrl_pps) {
			tmp_pps = 0;
			p_parent->_state = MS_BREAK;
		}
		else if (tmp_pps > p_stage->rs_ctrl_pps && tmp_pps > p_stage->rs_stop_pps) {
			tmp_pps -= p_stage->rs_ctrl_pps;
		}
		else if (tmp_pps) {
			tmp_pps = 0;
			p_parent->_state = MS_BREAK;
		}
	}

	if (tmp_pps != p_parent->_current_pps) {
		if (tmp_pps) pifStepMotor_SetPps(p_parent, tmp_pps);
		p_parent->_current_pps = tmp_pps;
	}

    if (p_parent->_state == MS_BREAK) {
    	pifStepMotor_Break(p_parent);
		if (p_stage->rs_break_time &&
				pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->rs_break_time)) {
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
	    	if (*p_stage->pp_stop_sensor) {
	    		if ((*p_stage->pp_stop_sensor)->_curr_state == OFF) {
	    			p_parent->__error = 1;
	    		}
	    	}
	    }

		if (*p_stage->pp_reduce_sensor) {
			pifSensor_DetachEvtChange(*p_stage->pp_reduce_sensor);
		}
		if (*p_stage->pp_stop_sensor) {
			pifSensor_DetachEvtChange(*p_stage->pp_stop_sensor);
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

static void _evtSwitchReduceChange(PifId id, uint16_t level, void* p_issuer)
{
	PifStepMotor* p_parent = (PifStepMotor*)p_issuer;

	(void)id;

	if (p_parent->_state >= MS_REDUCE) return;

	if (level) {
		pifStepMotorSpeed_Stop(p_parent);
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

PifStepMotor* pifStepMotorSpeed_Create(PifId id, PifPulse* p_timer, uint8_t resolution, PifStepMotorOperation operation, uint16_t period1ms)
{
	PifStepMotorSpeed* p_owner = NULL;

    if (!p_timer) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	p_owner = calloc(sizeof(PifStepMotorSpeed), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
		return NULL;
    }

    PifStepMotor* p_parent = (PifStepMotor*)&p_owner->parent;
    if (!pifStepMotor_Init(p_parent, id, p_timer, resolution, operation)) goto fail;

    p_parent->__p_timer_delay = pifPulse_AddItem(p_parent->_p_timer, PT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
	if (!p_parent->__p_task) goto fail;
    return p_parent;

fail:
	pifStepMotorSpeed_Destroy((PifStepMotor**)&p_owner);
    return NULL;
}

void pifStepMotorSpeed_Destroy(PifStepMotor** pp_parent)
{
    if (*pp_parent) {
		if ((*pp_parent)->__p_task) {
			pifTaskManager_Remove((*pp_parent)->__p_task);
			(*pp_parent)->__p_task = NULL;
		}
    	pifStepMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

BOOL pifStepMotorSpeed_AddStages(PifStepMotor* p_parent, uint8_t stage_size, const PifStepMotorSpeedStage* p_stages)
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

    PifStepMotorSpeed* pstSpeed = (PifStepMotorSpeed*)p_parent;
    pstSpeed->__stage_size = stage_size;
    pstSpeed->__p_stages = p_stages;
    return TRUE;
}

BOOL pifStepMotorSpeed_Start(PifStepMotor* p_parent, uint8_t stage_index, uint32_t operating_time)
{
    PifStepMotorSpeed* p_owner = (PifStepMotorSpeed*)p_parent;
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
    	if (*p_stage->pp_start_sensor) {
    		if ((*p_stage->pp_start_sensor)->_curr_state != ON) {
    			state |= 1;
    		}
    	}
    	if (*p_stage->pp_reduce_sensor) {
    		if ((*p_stage->pp_reduce_sensor)->_curr_state != OFF) {
    			state |= 2;
    		}
    	}
    	if (*p_stage->pp_stop_sensor) {
    		if ((*p_stage->pp_stop_sensor)->_curr_state != OFF) {
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

    if (*p_stage->pp_reduce_sensor) {
        pifSensor_AttachEvtChange(*p_stage->pp_reduce_sensor, _evtSwitchReduceChange, p_parent);
    }

    if (*p_stage->pp_stop_sensor) {
        pifSensor_AttachEvtChange(*p_stage->pp_stop_sensor, _evtSwitchStopChange, p_parent);
    }

    p_parent->_direction = (p_stage->mode & MM_D_MASK) >> MM_D_SHIFT;

    if (p_stage->gs_ctrl_pps) {
    	pifStepMotor_SetPps(p_parent, p_stage->gs_start_pps);
        p_parent->_state = MS_GAINED;
    }
    else {
    	pifStepMotor_SetPps(p_parent, p_stage->fs_fixed_pps);
        p_parent->_state = MS_CONST;
    }
    p_parent->__error = 0;

    pifStepMotor_Start(p_parent, 0);
    return TRUE;
}

void pifStepMotorSpeed_Stop(PifStepMotor* p_parent)
{
    const PifStepMotorSpeedStage* p_stage = ((PifStepMotorSpeed*)p_parent)->__p_current_stage;

    if (p_parent->_state == MS_IDLE) return;

    if (p_stage->fs_overtime && pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->fs_overtime)) {
        p_parent->_state = MS_OVER_RUN;
    }
    else {
        p_parent->_state = MS_REDUCE;
    }
}

void pifStepMotorSpeed_Emergency(PifStepMotor* p_parent)
{
    p_parent->_state = MS_BREAK;
}
