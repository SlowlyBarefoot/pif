#include <string.h>

#include "pif_log.h"
#include "pif_duty_motor_speed_enc.h"


static void _evtTimerDelayFinish(void* p_issuer)
{
	PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	switch (p_parent->_state) {
    case MS_GAINED:
    case MS_STABLE:
        p_parent->__error = 1;
        break;

	case MS_OVER_RUN:
        p_parent->_state = MS_REDUCE;
        break;

	case MS_BREAKING:
        p_parent->_state = MS_STOPPING;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_WARN, "DMSE(%u) S:%u", p_parent->_id, p_parent->_state);
#endif
		break;
	}
}

static uint16_t _doTask(PifTask* p_task)
{
	PifDutyMotor* p_parent = p_task->_p_client;
	float ctrl_duty = 0;
	uint16_t tmp_enc = 0;
	uint16_t tmp_duty = 0;
	uint16_t enc_average = 0;
    PifDutyMotorSpeedEnc* p_owner = (PifDutyMotorSpeedEnc*)p_parent;
	const PifDutyMotorSpeedEncStage* p_stage = p_owner->__p_current_stage;
	PifMotorState state = p_parent->_state;

	tmp_enc = p_owner->__measure_enc;
	p_owner->__measure_enc = 0;
	tmp_duty = p_parent->_current_duty;

	p_owner->__enc_sample_sum -= p_owner->__enc_sample[p_owner->__enc_sample_idx];
	p_owner->__enc_sample_sum += tmp_enc;
	p_owner->__enc_sample[p_owner->__enc_sample_idx] = tmp_enc;
	p_owner->__enc_sample_idx = (p_owner->__enc_sample_idx + 1) % MAX_STABLE_CNT;

	if (p_parent->_state == MS_GAINED) {
		if (tmp_enc >= p_owner->__arrive_ppr) {
			p_parent->_state = MS_STABLE;
            if (!pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->fs_stable_timeout)) {
                p_parent->__error = 1;
            }
		}
		else {
			tmp_duty += p_stage->gs_ctrl_duty;
			if (tmp_duty >= p_stage->fs_high_duty) tmp_duty = p_stage->fs_high_duty;
		}
	}
	else if (p_parent->_state == MS_STABLE) {
		enc_average = p_owner->__enc_sample_sum / MAX_STABLE_CNT;

        if (enc_average >= p_owner->__err_low_ppr && enc_average <= p_owner->__err_high_ppr) {
            pifPulse_StopItem(p_parent->__p_timer_delay);
            p_parent->_state = MS_CONST;
        }
    }
	else if (p_parent->_state == MS_REDUCE) {
		if (!p_stage->rs_ctrl_duty) {
			tmp_duty = 0;
			p_parent->_state = MS_BREAK;
		}
		else if (tmp_duty > p_stage->rs_ctrl_duty && tmp_duty > p_stage->rs_low_duty) {
			tmp_duty -= p_stage->rs_ctrl_duty;
		}
		else if (tmp_duty) {
			tmp_duty = 0;
			p_parent->_state = MS_BREAK;
		}
	}

	if (p_stage->mode & MM_SC_MASK) {
		if ((p_parent->_state == MS_STABLE) || (p_parent->_state == MS_CONST)) {
			ctrl_duty = pifPidControl_Calcurate(&p_owner->__pid_control, p_stage->fs_pulses_per_range - tmp_enc);

			if (ctrl_duty > 0) {
				if (tmp_duty + ctrl_duty < p_stage->fs_high_duty) {
					tmp_duty += ctrl_duty;
				}
				else {
					tmp_duty += (p_stage->fs_high_duty - tmp_duty) / 2;
					if (tmp_duty > p_stage->fs_high_duty) {
						tmp_duty = p_stage->fs_high_duty;
					}
				}
			}
			else if (ctrl_duty < 0) {
				if (tmp_duty + ctrl_duty >= 0) {
					tmp_duty += ctrl_duty;
				}
				else {
					tmp_duty /= 2;
				}
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
		pifLog_Printf(LT_INFO, "DMSE(%u) %s D:%u(%u%%) E:%u", p_parent->_id,
				kMotorState[p_parent->_state], tmp_duty, (uint16_t)(100 * tmp_duty / p_parent->_max_duty), tmp_enc);
	}
#endif

	pifDutyMotor_Control(p_parent);
	return 0;
}

static void _evtSwitchReduceChange(PifId id, uint16_t level, void* p_issuer)
{
	PifDutyMotor* p_parent = (PifDutyMotor*)p_issuer;

	(void)id;

	if (p_parent->_state >= MS_REDUCE) return;

	if (level) {
		pifDutyMotorSpeedEnc_Stop(p_parent);
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

PifDutyMotor* pifDutyMotorSpeedEnc_Create(PifId id, PifPulse* p_timer, uint16_t max_duty, uint16_t period1ms)
{
	PifDutyMotorSpeedEnc* p_owner = NULL;

    if (!p_timer || !max_duty) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

	p_owner = calloc(sizeof(PifDutyMotorSpeedEnc), 1);
    if (!p_owner) {
        pif_error = E_OUT_OF_HEAP;
	    return NULL;
    }

    PifDutyMotor *p_parent = &p_owner->parent;
    if (!pifDutyMotor_Init(p_parent, id, p_timer, max_duty)) goto fail;

    p_parent->__p_timer_delay = pifPulse_AddItem(p_parent->_p_timer, PT_ONCE);
    if (!p_parent->__p_timer_delay) goto fail;
    pifPulse_AttachEvtFinish(p_parent->__p_timer_delay, _evtTimerDelayFinish, p_parent);

    p_parent->__p_task = pifTaskManager_Add(TM_PERIOD_MS, period1ms, _doTask, p_owner, FALSE);
    return p_parent;

fail:
	pifDutyMotorSpeedEnc_Destroy((PifDutyMotor**)&p_owner);
    return NULL;
}

void pifDutyMotorSpeedEnc_Destroy(PifDutyMotor** pp_parent)
{
    if (*pp_parent) {
    	pifDutyMotor_Clear(*pp_parent);
        free(*pp_parent);
        *pp_parent = NULL;
    }
}

BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotor* p_parent, uint8_t stage_size, const PifDutyMotorSpeedEncStage* p_stages)
{
    for (int i = 0; i < stage_size; i++) {
    	if (p_stages[i].mode & MM_PC_MASK) {
            pif_error = E_INVALID_PARAM;
		    return FALSE;
    	}
    }

    PifDutyMotorSpeedEnc* pstSpeedEnc = (PifDutyMotorSpeedEnc*)p_parent;
    pstSpeedEnc->__stage_size = stage_size;
    pstSpeedEnc->__p_stages = p_stages;
    return TRUE;
}

PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotor* p_parent)
{
	return &((PifDutyMotorSpeedEnc*)p_parent)->__pid_control;
}

BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotor* p_parent, uint8_t stage_index, uint32_t operating_time)
{
    PifDutyMotorSpeedEnc* p_owner = (PifDutyMotorSpeedEnc*)p_parent;
    const PifDutyMotorSpeedEncStage* p_stage;
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
		if (!pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->gs_arrive_timeout)) return FALSE;

		p_owner->__arrive_ppr = p_stage->fs_pulses_per_range * p_stage->gs_arrive_ratio / 100;
		p_parent->_current_duty = p_stage->gs_start_duty;
		p_parent->_state = MS_GAINED;
    }
    else {
    	p_parent->_current_duty = p_stage->fs_high_duty;
        p_parent->_state = MS_CONST;
    }
    p_owner->__err_low_ppr = p_stage->fs_pulses_per_range * p_stage->fs_stable_err_low / 100;
    p_owner->__err_high_ppr = p_stage->fs_pulses_per_range * p_stage->fs_stable_err_high / 100;
    p_owner->__enc_sample_idx = 0;
    memset(p_owner->__enc_sample, 0, sizeof(p_owner->__enc_sample));
    p_owner->__enc_sample_sum = 0;
    p_owner->__measure_enc = 0;
    p_parent->__error = 0;

    (*p_parent->__act_set_duty)(p_parent->_current_duty);
    return TRUE;
}

void pifDutyMotorSpeedEnc_Stop(PifDutyMotor* p_parent)
{
    const PifDutyMotorSpeedEncStage* p_stage = ((PifDutyMotorSpeedEnc*)p_parent)->__p_current_stage;

    if (p_parent->_state == MS_IDLE) return;

    if (p_stage->fs_overtime && pifPulse_StartItem(p_parent->__p_timer_delay, p_stage->fs_overtime)) {
        p_parent->_state = MS_OVER_RUN;
    }
    else {
        p_parent->_state = MS_REDUCE;
    }
}

void pifDutyMotorSpeedEnc_Emergency(PifDutyMotor* p_parent)
{
    p_parent->_current_duty = 0;
    p_parent->_state = MS_BREAK;
}

void pifDutyMotorSpeedEnc_sigEncoder(PifDutyMotor* p_parent)
{
    ((PifDutyMotorSpeedEnc*)p_parent)->__measure_enc++;
}
