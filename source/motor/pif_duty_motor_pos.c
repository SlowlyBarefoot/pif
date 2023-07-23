#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "motor/pif_duty_motor_pos.h"


static void _evtTimerDelayFinish(PifIssuerP p_issuer)
{
    PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	switch (p_parent->_state) {
	case MS_BREAKING:
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_STOPPING, "SMP");
#else
		pifDutyMotor_SetState(p_parent, MS_STOPPING);
#endif
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMP(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifDutyMotor* p_parent = p_task->_p_client;
	uint16_t tmp_duty = 0;
	PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_parent;
    const PifDutyMotorPosStage* p_stage = p_owner->__p_current_stage;
    uint16_t pre_duty = p_parent->_current_duty;

    tmp_duty = p_parent->_current_duty;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_duty >= p_stage->fs_high_duty) {
			tmp_duty = p_stage->fs_high_duty;
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_CONST, "SMP");
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
			if (p_stage->mode & MM_PC_MASK) {
				tmp_duty = p_stage->rs_low_duty;
#ifndef __PIF_NO_LOG__
				pifDutyMotor_SetState(p_parent, MS_LOW_CONST, "SMP");
#else
				pifDutyMotor_SetState(p_parent, MS_LOW_CONST);
#endif
			}
			else {
				tmp_duty = 0;
#ifndef __PIF_NO_LOG__
				pifDutyMotor_SetState(p_parent, MS_BREAK, "SMP");
#else
				pifDutyMotor_SetState(p_parent, MS_BREAK);
#endif
			}
		}
		else if (tmp_duty > p_stage->rs_low_duty + p_stage->rs_ctrl_duty) {
			tmp_duty -= p_stage->rs_ctrl_duty;
		}
		else if (tmp_duty) {
			tmp_duty = p_stage->rs_low_duty;
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_LOW_CONST, "SMP");
#else
			pifDutyMotor_SetState(p_parent, MS_LOW_CONST);
#endif
		}
	}

	if (p_stage->mode & MM_PC_MASK) {
		if (p_owner->__p_encoder->falling_count >= p_stage->total_pulse) {
			tmp_duty = 0;
			if (p_parent->_state < MS_BREAK) {
#ifndef __PIF_NO_LOG__
				pifDutyMotor_SetState(p_parent, MS_BREAK, "SMP");
#else
				pifDutyMotor_SetState(p_parent, MS_BREAK);
#endif
			}
		}
		else if (p_owner->__p_encoder->falling_count >= p_stage->fs_pulse_count) {
			if (p_parent->_state < MS_REDUCE) {
#ifndef __PIF_NO_LOG__
				pifDutyMotor_SetState(p_parent, MS_REDUCE, "SMP");
#else
				pifDutyMotor_SetState(p_parent, MS_REDUCE);
#endif
			}
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
			pifDutyMotor_SetState(p_parent, MS_BREAKING, "SMP");
#else
			pifDutyMotor_SetState(p_parent, MS_BREAKING);
#endif
    	}
    	else {
			if (p_parent->act_operate_break) (*p_parent->act_operate_break)(1);
#ifndef __PIF_NO_LOG__
			pifDutyMotor_SetState(p_parent, MS_STOPPING, "SMP");
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
		pifDutyMotor_SetState(p_parent, MS_STOP, "SMP");
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
			p_stage->p_reduce_sensor->evt_change = NULL;
		}
		if (p_stage->p_stop_sensor) {
			p_stage->p_stop_sensor->evt_change = NULL;
		}
    }

#ifndef __PIF_NO_LOG__
	if (pif_log_flag.bt.duty_motor && pre_duty != tmp_duty) {
		if (p_parent->_state == MS_CONST) {
			pifLog_Printf(LT_INFO, "DMP(%u) %s D:%u->%u(%u%%) CP:%u E:%d P:%luus", p_parent->_id, kMotorState[p_parent->_state],
					pre_duty, tmp_duty, (uint16_t)(100 * tmp_duty / p_parent->_max_duty), p_owner->__p_encoder->falling_count, p_parent->__error,
					pifPulse_GetPeriod(p_owner->__p_encoder));
		}
		else {
			pifLog_Printf(LT_INFO, "DMP(%u) %s D:%u->%u(%u%%) CP:%u E:%d", p_parent->_id, kMotorState[p_parent->_state],
					pre_duty, tmp_duty, (uint16_t)(100 * tmp_duty / p_parent->_max_duty), p_owner->__p_encoder->falling_count, p_parent->__error);
		}
	}
#endif

	pifDutyMotor_Control(p_parent);
	return 0;
}

static void _evtSwitchReduceChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer)
{
	PifDutyMotorPos* p_motor = (PifDutyMotorPos*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->parent._state >= MS_REDUCE) return;

	if (state) {
		pifDutyMotorPos_Stop(p_motor);
	}
}

static void _evtSwitchStopChange(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer)
{
	PifDutyMotor* p_motor = (PifDutyMotor*)p_issuer;

	(void)p_owner;
	(void)p_value;

	if (p_motor->_state >= MS_BREAK) return;

	if (state) {
		p_motor->_current_duty = 0;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_motor, MS_BREAK, "SMP");
#else
		pifDutyMotor_SetState(p_motor, MS_BREAK);
#endif
	}
}

static void _evtPulseEdge(PifPulseState state, PifIssuerP p_issuer)
{
	PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_issuer;
	PifDutyMotor* p_parent = &p_owner->parent;

	if (state != PS_FALLING_EDGE) return;

	if (p_owner->__p_current_stage->mode & MM_PC_MASK) {
		if (p_parent->_state && p_parent->_state < MS_STOP) {
			if (p_parent->_current_duty && p_owner->__p_encoder->falling_count >= p_owner->__p_current_stage->total_pulse) {
				p_parent->_current_duty = 0;
				(*p_parent->act_set_duty)(p_parent->_current_duty);
			}
		}
	}
}

BOOL pifDutyMotorPos_Init(PifDutyMotorPos* p_owner, PifId id, PifTimerManager* p_timer_manager,
		uint16_t max_duty, uint16_t period1ms, PifPulse* p_encoder)
{
    if (!p_timer_manager || !max_duty || !period1ms || !p_encoder) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDutyMotorPos));

    PifDutyMotor* p_parent = &p_owner->parent;
    if (!pifDutyMotor_Init(p_parent, id, p_timer_manager, max_duty)) goto fail;

    p_parent->__p_timer_delay = pifTimerManager_Add(p_parent->_p_timer_manager, TT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifTimer_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
	if (!p_parent->__p_task) goto fail;
	p_parent->__p_task->name = "DutyMotorPos";

	p_owner->__p_encoder = p_encoder;
#ifndef __PIF_NO_LOG__
    pifPulse_SetMeasureMode(p_encoder, PIF_PMM_COUNT | PIF_PMM_PERIOD);
#else
    pifPulse_SetMeasureMode(p_encoder, PIF_PMM_FALLING_COUNT);
#endif
    pifPulse_AttachEvtEdge(p_encoder, _evtPulseEdge, p_owner);
    return TRUE;

fail:
	pifDutyMotorPos_Clear(p_owner);
    return FALSE;
}

void pifDutyMotorPos_Clear(PifDutyMotorPos* p_owner)
{
    PifDutyMotor* p_parent = &p_owner->parent;

    if (p_parent->__p_task) {
		pifTaskManager_Remove(p_parent->__p_task);
		p_parent->__p_task = NULL;
	}
	pifDutyMotor_Clear(p_parent);
}

BOOL pifDutyMotorPos_AddStages(PifDutyMotorPos* p_owner, uint8_t stage_size, const PifDutyMotorPosStage* p_stages)
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

BOOL pifDutyMotorPos_Start(PifDutyMotorPos* p_owner, uint8_t stage_index, uint32_t operating_time)
{
    PifDutyMotor* p_parent = &p_owner->parent;
    const PifDutyMotorPosStage* p_stage;
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
        p_stage->p_reduce_sensor->evt_change = _evtSwitchReduceChange;
    }

    if (p_stage->p_stop_sensor) {
        p_stage->p_stop_sensor->evt_change = _evtSwitchStopChange;
    }

    if (p_parent->act_set_direction) (*p_parent->act_set_direction)((p_stage->mode & MM_D_MASK) >> MM_D_SHIFT);

    if (p_stage->gs_ctrl_duty) {
    	p_parent->_current_duty = p_stage->gs_start_duty;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_GAINED, "SMP");
#else
		pifDutyMotor_SetState(p_parent, MS_GAINED);
#endif
    }
    else {
    	p_parent->_current_duty = p_stage->fs_high_duty;
#ifndef __PIF_NO_LOG__
		pifDutyMotor_SetState(p_parent, MS_CONST, "SMP");
#else
		pifDutyMotor_SetState(p_parent, MS_CONST);
#endif
    }
    p_owner->__p_encoder->falling_count = 0UL;
    p_parent->__error = 0;

    (*p_parent->act_set_duty)(p_parent->_current_duty);
    return TRUE;
}

void pifDutyMotorPos_Stop(PifDutyMotorPos* p_owner)
{
    PifDutyMotor* p_parent = &p_owner->parent;

    if (p_parent->_state == MS_IDLE) return;

#ifndef __PIF_NO_LOG__
	pifDutyMotor_SetState(p_parent, MS_REDUCE, "SMP");
#else
	pifDutyMotor_SetState(p_parent, MS_REDUCE);
#endif
}

#ifdef __PIF_NO_USE_INLINE__

uint32_t pifDutyMotorPos_GetCurrentPulse(PifDutyMotorPos* p_owner)
{
	return p_owner->__p_encoder->falling_count;
}

#endif

void pifDutyMotorPos_Emergency(PifDutyMotorPos* p_owner)
{
    PifDutyMotor* p_parent = &p_owner->parent;

	p_parent->_current_duty = 0;
#ifndef __PIF_NO_LOG__
	pifDutyMotor_SetState(p_parent, MS_REDUCE, "SMP");
#else
	pifDutyMotor_SetState(p_parent, MS_REDUCE);
#endif
}
