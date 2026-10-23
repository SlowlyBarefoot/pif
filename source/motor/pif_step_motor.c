// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "motor/pif_step_motor.h"


#ifdef PIF_USE_STEP_MOTOR_2PHASE

const uint16_t kOperation_2P_2W[] = {
		0x02,		// 10 : B
		0x03,		// 11 : B A
		0x01,		// 01 :   A
		0x00		// 00 :
};

const uint16_t kOperation_2P_4W_1S[] = {
		0x01,		// 00 01 :     A
		0x04,		// 01 00 :  B
		0x02,		// 00 10 :    ~A
		0x08		// 10 00 : ~B
};

const uint16_t kOperation_2P_4W_2S[] = {
		0x05,		// 01 01 :  B  A
		0x06,		// 01 10 :  B ~A
		0x0A,		// 10 10 : ~B ~A
		0x09		// 10 01 : ~B  A
};

const uint16_t kOperation_2P_4W_1_2S[] = {
		0x01,		// 00 01 :     A
		0x05,		// 01 01 :  B  A
		0x04,		// 01 00 :  B
		0x06,		// 01 10 :  B ~A
		0x02,		// 00 10 :    ~A
		0x0A,		// 10 10 : ~B ~A
		0x08,		// 10 00 : ~B
		0x09		// 10 01 : ~B  A
};

#endif

#ifdef PIF_USE_STEP_MOTOR_5PHASE

// Each step moves one terminal, in the order C, D, E, A, B.
const uint16_t kOperation_5P_PG[] = {
		0x16,		// 1 0 1 1 0 : E   C B
		0x12,		// 1 0 0 1 0 : E     B
		0x1A,		// 1 1 0 1 0 : E D   B
		0x0A,		// 0 1 0 1 0 :   D   B
		0x0B,		// 0 1 0 1 1 :   D   B A
		0x09,		// 0 1 0 0 1 :   D     A
		0x0D,		// 0 1 1 0 1 :   D C   A
		0x05,		// 0 0 1 0 1 :     C   A
		0x15,		// 1 0 1 0 1 : E   C   A
		0x14		// 1 0 1 0 0 : E   C
};

// kOperation_5P_PG with a half step between two full steps: the terminal that moves is off in between.
const uint16_t kOperation_5P_PG_HALF[] = {
		0x16,						// E   C B
		0x12 | SM5P_OFF(SM5P_C),	// E   - B
		0x12,						// E     B
		0x12 | SM5P_OFF(SM5P_D),	// E -   B
		0x1A,						// E D   B
		0x0A | SM5P_OFF(SM5P_E),	// - D   B
		0x0A,						//   D   B
		0x0A | SM5P_OFF(SM5P_A),	//   D   B -
		0x0B,						//   D   B A
		0x09 | SM5P_OFF(SM5P_B),	//   D   - A
		0x09,						//   D     A
		0x09 | SM5P_OFF(SM5P_C),	//   D -   A
		0x0D,						//   D C   A
		0x05 | SM5P_OFF(SM5P_D),	//   - C   A
		0x05,						//     C   A
		0x05 | SM5P_OFF(SM5P_E),	// -   C   A
		0x15,						// E   C   A
		0x14 | SM5P_OFF(SM5P_A),	// E   C   -
		0x14,						// E   C
		0x14 | SM5P_OFF(SM5P_B)		// E   C -
};

#endif

#ifdef PIF_USE_STEP_MOTOR_STEP_DIR

// Index 1 raises STEP and index 0 lowers it, so a pulse ends with STEP low.
const uint16_t kOperation_StepDir[] = {
		SMSD_ENABLE,
		SMSD_ENABLE | SMSD_STEP
};

	#define _IS_STEP_DIR(p_owner)	((p_owner)->_operation == SMO_STEP_DIR)
#else
	#define _IS_STEP_DIR(p_owner)	FALSE
#endif


/**
 * @brief Returns the phase output of the current step, with DIR added in SMO_STEP_DIR.
 * @param p_owner Pointer to the step motor instance.
 */
static uint16_t _getPhase(PifStepMotor* p_owner)
{
	uint16_t phase = p_owner->__p_phase_operation[p_owner->__current_step];

	if (_IS_STEP_DIR(p_owner) && p_owner->_direction) phase |= SMSD_DIR;
	return phase;
}

/**
 * @brief Advances or reverses one commutation step and applies the phase output.
 * @param p_owner Pointer to the step motor instance.
 */
static void _setStep(PifStepMotor* p_owner)
{
	// In SMO_STEP_DIR the direction is on the DIR line, so the STEP table always runs forward.
    if (p_owner->_direction == 0 || _IS_STEP_DIR(p_owner)) {
		p_owner->__current_step++;
		if (p_owner->__current_step == p_owner->__step_size) {
			p_owner->__current_step = 0;
		}
	}
	else {
		if (p_owner->__current_step == 0) {
			p_owner->__current_step = p_owner->__step_size;
		}
		p_owner->__current_step--;
	}

	p_owner->__tick_count++;
	if (p_owner->__tick_count >= p_owner->__ticks_per_pulse) {
		p_owner->__tick_count = 0;
		p_owner->_current_pulse++;
	}

	(*p_owner->act_set_step)(p_owner, _getPhase(p_owner));
}

/**
 * @brief Recomputes the step timer ticks per pulse and reapplies the current speed.
 * @param p_owner Pointer to the step motor instance.
 */
static void _updateTicksPerPulse(PifStepMotor* p_owner)
{
	uint32_t ticks = p_owner->_reduction_gear_ratio;

	switch (p_owner->_operation) {
	case SMO_2P_4W_1_2S:
	case SMO_5P_PG_HALF:
		ticks *= 2;
		break;

#ifdef PIF_USE_STEP_MOTOR_STEP_DIR
	case SMO_STEP_DIR:
		ticks *= 2UL * p_owner->_microstep;
		break;
#endif

	default:
		break;
	}
	p_owner->__ticks_per_pulse = ticks;

	if (p_owner->_current_pps) pifStepMotor_SetPps(p_owner, p_owner->_current_pps);
}

/**
 * @brief Timer callback that emits one step and stops at the configured target pulse.
 * @param p_issuer Issuer pointer cast to `PifStepMotor*`.
 */
static void _evtTimerStepFinish(PifIssuerP p_issuer)
{
    PifStepMotor* p_owner = (PifStepMotor*)p_issuer;

    _setStep(p_owner);

	if (p_owner->__target_pulse) {
		if (p_owner->_current_pulse >= p_owner->__target_pulse) {
			pifTimer_Stop(p_owner->__p_timer_step);
			if (p_owner->__stop_step) (*p_owner->__stop_step)(p_owner);
			else if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
		}
	}
}

void pifStepMotor_Control(PifStepMotor* p_owner)
{
    if (p_owner->_error) {
        if (p_owner->_state < MS_BRAKE) {
#ifndef PIF_NO_LOG
			pifStepMotor_SetState(p_owner, MS_BRAKE, "SM");
#else
			pifStepMotor_SetState(p_owner, MS_BRAKE);
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
		pifStepMotor_SetState(p_owner, MS_IDLE, "SM");
#else
		pifStepMotor_SetState(p_owner, MS_IDLE);
#endif
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
}

/**
 * @brief Brake-timer callback that requests reduction or releases motor hold.
 * @param p_issuer Issuer pointer cast to `PifStepMotor*`.
 */
static void _evtTimerBrakeFinish(void *p_issuer)
{
    PifStepMotor* p_owner = (PifStepMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
#ifndef PIF_NO_LOG
		pifStepMotor_SetState(p_owner, MS_REDUCE, "SM");
#else
		pifStepMotor_SetState(p_owner, MS_REDUCE);
#endif
    }
    else {
    	pifStepMotor_Release(p_owner);
    }
}

BOOL pifStepMotor_Init(PifStepMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t resolution, PifStepMotorOperation operation)
{
    if (!p_owner || !p_timer_manager || !resolution) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifStepMotor));

    p_owner->_p_timer_manager = p_timer_manager;
	p_owner->__p_timer_step = pifTimerManager_Add(p_timer_manager, TT_REPEAT);
    if (!p_owner->__p_timer_step) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__p_timer_step, _evtTimerStepFinish, p_owner, FALSE);

    p_owner->_reduction_gear_ratio = 1;
    p_owner->_microstep = 1;
	if (!pifStepMotor_SetOperation(p_owner, operation)) goto fail;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_resolution = resolution;
    p_owner->__step_period1us = 1000;
    return TRUE;

fail:
	pifStepMotor_Clear(p_owner);
	return FALSE;	
}

void pifStepMotor_Clear(PifStepMotor* p_owner)
{
#ifdef COLLECT_SIGNAL_STEP_MOTOR
	pifStepMotor_ResetCsFlag(p_owner, SM_CSF_ALL_BIT);
#endif
	if (p_owner->__p_timer_step) {
		pifTimerManager_Remove(p_owner->__p_timer_step);
		p_owner->__p_timer_step = NULL;
	}
	if (p_owner->__p_timer_brake) {
		pifTimerManager_Remove(p_owner->__p_timer_brake);
		p_owner->__p_timer_brake = NULL;
	}
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
}

#ifndef PIF_NO_LOG

void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state, const char *tag)
{
	pifLog_Printf(LT_INFO, "%s(%u) %s->%s E:%d", tag, p_owner->_id,
			kMotorState[p_owner->_state], kMotorState[state], p_owner->_error);
	p_owner->_state = state;
#ifdef COLLECT_SIGNAL_STEP_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[SM_CSF_STATE_IDX], state);
#endif
}

#else

void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state)
{
	p_owner->_state = state;
#ifdef COLLECT_SIGNAL_STEP_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[SM_CSF_STATE_IDX], state);
#endif
}

#endif

BOOL pifStepMotor_SetDirection(PifStepMotor* p_owner, uint8_t direction)
{
	if (p_owner->_state != MS_IDLE) {
        pif_error = E_INVALID_STATE;
		return FALSE;
    }

	p_owner->_direction = direction;
	return TRUE;
}

BOOL pifStepMotor_SetOperatingTime(PifStepMotor* p_owner, uint32_t operating_time)
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

BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation)
{
	if (p_owner->__p_timer_step->_step != TS_STOP) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// An operation not put in the build falls to the default and is refused.
	switch (operation) {
#ifdef PIF_USE_STEP_MOTOR_2PHASE
	case SMO_2P_2W:
		p_owner->__step_size = 4;
		p_owner->__p_phase_operation = kOperation_2P_2W;
		break;

	case SMO_2P_4W_1S:
		p_owner->__step_size = 4;
		p_owner->__p_phase_operation = kOperation_2P_4W_1S;
		break;

	case SMO_2P_4W_2S:
		p_owner->__step_size = 4;
		p_owner->__p_phase_operation = kOperation_2P_4W_2S;
		break;

	case SMO_2P_4W_1_2S:
		p_owner->__step_size = 8;
		p_owner->__p_phase_operation = kOperation_2P_4W_1_2S;
		break;
#endif

#ifdef PIF_USE_STEP_MOTOR_5PHASE
	case SMO_5P_PG:
		p_owner->__step_size = 10;
		p_owner->__p_phase_operation = kOperation_5P_PG;
		break;

	case SMO_5P_PG_HALF:
		p_owner->__step_size = 20;
		p_owner->__p_phase_operation = kOperation_5P_PG_HALF;
		break;
#endif

#ifdef PIF_USE_STEP_MOTOR_STEP_DIR
	case SMO_STEP_DIR:
		p_owner->__step_size = 2;
		p_owner->__p_phase_operation = kOperation_StepDir;
		break;
#endif

	default:
        pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->_operation = operation;
	// The previous table may have been longer.
	if (p_owner->__current_step >= p_owner->__step_size) p_owner->__current_step = 0;
	_updateTicksPerPulse(p_owner);
	return TRUE;
}

BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps)
{
    if (!pps) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	uint32_t period = 1000000.0 / ((double)pps * p_owner->__ticks_per_pulse) + 0.5;

	if (period < 2 * p_owner->_p_timer_manager->_period1us) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	pifTimer_SetTarget(p_owner->__p_timer_step, period / p_owner->_p_timer_manager->_period1us);

	p_owner->_current_pps = pps;
#ifdef COLLECT_SIGNAL_STEP_MOTOR
	pifCollectSignal_Put(&p_owner->__cs[SM_CSF_PPS_IDX], pps);
#endif
	p_owner->__step_period1us = period;

#ifndef PIF_NO_LOG
	if (pif_log_flag.bt.step_motor) {
		pifLog_Printf(LT_INFO, "SM(%u) %s P/S:%d SP:%luus", p_owner->_id,
				kMotorState[p_owner->_state], pps, (unsigned long)p_owner->__step_period1us);
	}
#endif
	return TRUE;
}

BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio)
{
    if (!reduction_gear_ratio) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }
	if (p_owner->_state != MS_IDLE) {
        pif_error = E_INVALID_STATE;
		return FALSE;
    }

	p_owner->_reduction_gear_ratio = reduction_gear_ratio;
	_updateTicksPerPulse(p_owner);
	return TRUE;
}

BOOL pifStepMotor_SetMicrostep(PifStepMotor* p_owner, uint16_t microstep)
{
	// A power of 2 from 1 to 256, as the drivers take.
	if (!microstep || microstep > 256 || (microstep & (microstep - 1))) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }
	if (p_owner->__p_timer_step->_step != TS_STOP) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->_microstep = microstep;
	if (p_owner->act_set_microstep) (*p_owner->act_set_microstep)(p_owner, microstep);
	_updateTicksPerPulse(p_owner);
	return TRUE;
}

BOOL pifStepMotor_SetRpm(PifStepMotor* p_owner, float rpm)
{
	return pifStepMotor_SetPps(p_owner, rpm * p_owner->_resolution / 60 + 0.5);
}

float pifStepMotor_GetRpm(PifStepMotor* p_owner)
{
	return 60.0 * p_owner->_current_pps / p_owner->_resolution;
}

BOOL pifStepMotor_SetTargetPulse(PifStepMotor* p_owner, uint32_t target_pulse)
{
	if (p_owner->_state != MS_IDLE) {
        pif_error = E_INVALID_STATE;
		return FALSE;
    }

    p_owner->__target_pulse = target_pulse;
    p_owner->_current_pulse = 0;
    p_owner->__tick_count = 0;
	return TRUE;
}

BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse)
{
	if (!p_owner->act_set_step || p_owner->__step_period1us < 2 * p_owner->_p_timer_manager->_period1us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	// Set before the step timer runs, since its callback compares against them.
    p_owner->__target_pulse = target_pulse;
    p_owner->_current_pulse = 0;
    p_owner->__tick_count = 0;
	if (_IS_STEP_DIR(p_owner)) {
		// Enable the driver and set DIR ahead of the first STEP edge.
		p_owner->__current_step = 0;
		(*p_owner->act_set_step)(p_owner, _getPhase(p_owner));
	}
	return pifTimer_Start(p_owner->__p_timer_step, p_owner->__step_period1us / p_owner->_p_timer_manager->_period1us);
}

void pifStepMotor_Brake(PifStepMotor* p_owner)
{
   	pifTimer_Stop(p_owner->__p_timer_step);
}

void pifStepMotor_Release(PifStepMotor* p_owner)
{
	if (p_owner->act_set_step) (*p_owner->act_set_step)(p_owner, 0);
}

void pifStepMotor_BrakeRelease(PifStepMotor* p_owner, uint16_t brake_time)
{
	pifStepMotor_Brake(p_owner);

	if (brake_time) {
	    if (!p_owner->__p_timer_brake) {
	    	p_owner->__p_timer_brake = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	    }
	    if (p_owner->__p_timer_brake) {
	    	pifTimer_AttachEvtFinish(p_owner->__p_timer_brake, _evtTimerBrakeFinish, p_owner, FALSE);
			if (pifTimer_Start(p_owner->__p_timer_brake, brake_time)) return;
	    }
	}

	pifStepMotor_Release(p_owner);
}

BOOL pifStepMotor_StartControl(PifStepMotor* p_owner)
{
	if (!p_owner->_p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__error_notified = FALSE;
    pifTask_Resume(p_owner->_p_task);
    return TRUE;
}

BOOL pifStepMotor_StopControl(PifStepMotor* p_owner)
{
	if (!p_owner->_p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    pifTask_Pause(p_owner->_p_task);
    return TRUE;
}

#ifdef COLLECT_SIGNAL_STEP_MOTOR

BOOL pifStepMotor_SetCsFlag(PifStepMotor* p_owner, PifStepMotorCsFlag flag)
{
	if (flag & SM_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[SM_CSF_STATE_IDX], "SMS", p_owner->_id, CSVT_REG, 4,
				p_owner->_state)) return FALSE;
	}
	if (flag & SM_CSF_PPS_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[SM_CSF_PPS_IDX], "SMP", p_owner->_id, CSVT_INTEGER, 16,
				p_owner->_current_pps)) return FALSE;
	}
	return TRUE;
}

void pifStepMotor_ResetCsFlag(PifStepMotor* p_owner, PifStepMotorCsFlag flag)
{
	if (flag & SM_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[SM_CSF_STATE_IDX]);
	if (flag & SM_CSF_PPS_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[SM_CSF_PPS_IDX]);
}

#endif	// COLLECT_SIGNAL_STEP_MOTOR
