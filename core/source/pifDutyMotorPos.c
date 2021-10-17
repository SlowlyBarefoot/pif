#include "pifLog.h"
#include "pifDutyMotorPos.h"


static void _evtTimerDelayFinish(void* p_issuer)
{
    PifDutyMotor *p_parent = (PifDutyMotor *)p_issuer;

	switch (p_parent->_state) {
	case MS_BREAKING:
		p_parent->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMP(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static void _fnControlPos(PifDutyMotor* p_parent)
{
	uint16_t tmp_duty = 0;
	PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_parent;
    const PifDutyMotorPosStage* p_stage = p_owner->__p_current_stage;
    PifMotorState state = p_parent->_state;

    tmp_duty = p_parent->_current_duty;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_duty >= p_stage->fs_high_duty) {
			tmp_duty = p_stage->fs_high_duty;
			p_parent->_state = MS_CONST;
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
				p_parent->_state = MS_LOW_CONST;
			}
			else {
				tmp_duty = 0;
				p_parent->_state = MS_BREAK;
			}
		}
		else if (tmp_duty > p_stage->rs_low_duty + p_stage->rs_ctrl_duty) {
			tmp_duty -= p_stage->rs_ctrl_duty;
		}
		else if (tmp_duty) {
			tmp_duty = p_stage->rs_low_duty;
			p_parent->_state = MS_LOW_CONST;
		}
	}

	if (p_stage->mode & MM_PC_MASK) {
		if (p_owner->_current_pulse >= p_stage->total_pulse) {
			tmp_duty = 0;
			if (p_parent->_state < MS_BREAK) {
				p_parent->_state = MS_BREAK;
			}
		}
		else if (p_owner->_current_pulse >= p_stage->fs_pulse_count) {
			if (p_parent->_state < MS_REDUCE) {
				p_parent->_state = MS_REDUCE;
			}
		}
	}

	if (tmp_duty != p_parent->_current_duty) {
		(*p_parent->__act_set_duty)(tmp_duty);
		p_parent->_current_duty = tmp_duty;
	}

    if (p_parent->_state == MS_BREAK) {
    	if (p_parent->__act_operate_break && p_stage->rs_break_time &&
    			pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->rs_break_time)) {
			(*p_parent->__act_operate_break)(1);
			p_parent->_state = MS_BREAKING;
    	}
    	else {
			if (p_parent->__act_operate_break) (*p_parent->__act_operate_break)(1);
			p_parent->_state = MS_STOPPING;
    	}
	}

    if (p_parent->_state == MS_STOPPING) {
		if (!(p_stage->mode & MM_NR_MASK)) {
			if (p_parent->__act_operate_break) (*p_parent->__act_operate_break)(0);
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
	if (state != p_parent->_state && pif_log_flag.bt.duty_motor) {
		pifLog_Printf(LT_INFO, "DMP(%u) %s D:%u(%u%%) CP:%u", p_parent->_id,
				kMotorState[p_parent->_state], tmp_duty, (uint16_t)(100 * tmp_duty / p_parent->_max_duty),
				p_owner->_current_pulse);
	}
#endif
}

static void _evtSwitchReduceChange(PifId id, uint16_t level, void* p_issuer)
{
	PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	(void)id;

	if (p_parent->_state >= MS_REDUCE) return;

	if (level) {
		pifDutyMotorPos_Stop(p_parent);
	}
}

static void _evtSwitchStopChange(PifId id, uint16_t level, void* p_issuer)
{
	PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	(void)id;

	if (p_parent->_state >= MS_BREAK) return;

	if (level) {
		p_parent->_current_duty = 0;
		p_parent->_state = MS_BREAK;
	}
}

/**
 * @fn pifDutyMotorPos_Create
 * @brief 
 * @param id
 * @param p_timer
 * @param max_duty
 * @param control_period
 * @return 
 */
PifDutyMotor* pifDutyMotorPos_Create(PifId id, PifPulse* p_timer, uint16_t max_duty, uint16_t control_period)
{
    PifDutyMotorPos* p_owner = NULL;

    p_owner = calloc(sizeof(PifDutyMotorPos), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
        goto fail;
    }

    PifDutyMotor* p_parent = &p_owner->parent;
    if (!pifDutyMotor_Init(p_parent, id, p_timer, max_duty)) goto fail;

    if (!pifDutyMotor_InitControl(p_parent, control_period)) goto fail;

    p_parent->__p_timer_delay = pifPulse_AddItem(p_parent->_p_timer, PT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__control = _fnControlPos;
    return p_parent;

fail:
	pifDutyMotorPos_Destroy((PifDutyMotor**)&p_owner);
    return NULL;
}

/**
 * @fn pifDutyMotorPos_Destroy
 * @brief
 * @param pp_parent
 */
void pifDutyMotorPos_Destroy(PifDutyMotor** pp_parent)
{
    if (*pp_parent) {
    	pifDutyMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

/**
 * @fn pifDutyMotorPos_AddStages
 * @brief 
 * @param p_parent
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifDutyMotorPos_AddStages(PifDutyMotor* p_parent, uint8_t stage_size, const PifDutyMotorPosStage* p_stages)
{
    for (int i = 0; i < stage_size; i++) {
    	if (p_stages[i].mode & MM_SC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_parent;
    p_owner->__stage_size = stage_size;
    p_owner->__p_stages = p_stages;
    return TRUE;
}

/**
 * @fn pifDutyMotorPos_Start
 * @brief 
 * @param p_parent
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifDutyMotorPos_Start(PifDutyMotor* p_parent, uint8_t stage_index, uint32_t operating_time)
{
    PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_parent;
    const PifDutyMotorPosStage* p_stage;
    uint8_t state;

    if (!p_parent->__act_set_duty || !p_parent->__act_set_direction) {
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

    if (*p_stage->pp_reduce_sensor) {
        pifSensor_AttachEvtChange(*p_stage->pp_reduce_sensor, _evtSwitchReduceChange, p_parent);
    }

    if (*p_stage->pp_stop_sensor) {
        pifSensor_AttachEvtChange(*p_stage->pp_stop_sensor, _evtSwitchStopChange, p_parent);
    }

    if (p_parent->__act_set_direction) (*p_parent->__act_set_direction)((p_stage->mode & MM_D_MASK) >> MM_D_SHIFT);

    if (p_stage->gs_ctrl_duty) {
    	p_parent->_current_duty = p_stage->gs_start_duty;
    	p_parent->_state = MS_GAINED;
    }
    else {
    	p_parent->_current_duty = p_stage->fs_high_duty;
    	p_parent->_state = MS_CONST;
    }
    p_owner->_current_pulse = 0;
    p_parent->__error = 0;

    (*p_parent->__act_set_duty)(p_parent->_current_duty);
    return TRUE;
}

/**
 * @fn pifDutyMotorPos_Stop
 * @brief 
 * @param p_parent
 */
void pifDutyMotorPos_Stop(PifDutyMotor* p_parent)
{
    if (p_parent->_state == MS_IDLE) return;

    p_parent->_state = MS_REDUCE;
}

/**
 * @fn pifDutyMotorPos_Emergency
 * @brief
 * @param p_parent
 */
void pifDutyMotorPos_Emergency(PifDutyMotor* p_parent)
{
	p_parent->_current_duty = 0;
	p_parent->_state = MS_REDUCE;
}

/**
 * @fn pifDutyMotorPos_sigEncoder
 * @brief Interrupt Function에서 호출할 것
 * @param p_parent
 */
void pifDutyMotorPos_sigEncoder(PifDutyMotor* p_parent)
{
    PifDutyMotorPos* p_owner = (PifDutyMotorPos*)p_parent;

    p_owner->_current_pulse++;
	if (p_owner->__p_current_stage->mode & MM_PC_MASK) {
		if (p_parent->_state && p_parent->_state < MS_STOP) {
			if (p_parent->_current_duty && p_owner->_current_pulse >= p_owner->__p_current_stage->total_pulse) {
				p_parent->_current_duty = 0;
				(*p_parent->__act_set_duty)(p_parent->_current_duty);
			}
		}
	}
}
