#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_step_motor.h"


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

#if 0
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
#endif


static void _setStep(PifStepMotor* p_owner)
{
    if (p_owner->_direction == 0) {
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

	p_owner->_current_pulse++;

	(*p_owner->act_set_step)(p_owner->__p_phase_operation[p_owner->__current_step]);
}

static void _evtTimerStepFinish(void* p_issuer)
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
	if (p_owner->_state == MS_STOP) {
		p_owner->__p_task->pause = TRUE;
#ifndef __PIF_NO_LOG__
		pifStepMotor_SetState(p_owner, MS_IDLE, "SM");
#else
		pifStepMotor_SetState(p_owner, MS_IDLE);
#endif
		if (p_owner->evt_stop) (*p_owner->evt_stop)(p_owner);
	}
}

static void _evtTimerBreakFinish(void *p_issuer)
{
    PifStepMotor* p_owner = (PifStepMotor*)p_issuer;

    if (p_owner->_state > MS_IDLE && p_owner->_state < MS_REDUCE) {
#ifndef __PIF_NO_LOG__
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
    if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifStepMotor));

    p_owner->_p_timer_manager = p_timer_manager;
	p_owner->__p_timer_step = pifTimerManager_Add(p_timer_manager, TT_REPEAT);
    if (!p_owner->__p_timer_step) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__p_timer_step, _evtTimerStepFinish, p_owner);

	pifStepMotor_SetOperation(p_owner, operation);

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_resolution = resolution;
    p_owner->_reduction_gear_ratio = 1;
    p_owner->__step_period1us = 1000;
    return TRUE;

fail:
	pifStepMotor_Clear(p_owner);
	return FALSE;	
}

void pifStepMotor_Clear(PifStepMotor* p_owner)
{
	if (p_owner->__p_timer_step) {
		pifTimerManager_Remove(p_owner->__p_timer_step);
		p_owner->__p_timer_step = NULL;
	}
	if (p_owner->__p_timer_break) {
		pifTimerManager_Remove(p_owner->__p_timer_break);
		p_owner->__p_timer_break = NULL;
	}
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
}

#ifndef __PIF_NO_LOG__

void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state, char *tag)
{
	pifLog_Printf(LT_INFO, "%s(%u) %s->%s E:%d", tag, p_owner->_id,
			kMotorState[p_owner->_state], kMotorState[state], p_owner->__error);
	p_owner->_state = state;
}

#else

#ifdef __PIF_NO_USE_INLINE__

void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state)
{
	p_owner->_state = state;
}

#endif

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

BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation)
{
	if (p_owner->__p_timer_step->_step != TS_STOP) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	switch (operation) {
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

#if 0
	case SMO_5P_PG:
		p_owner->__step_size = 10;
		p_owner->__p_phase_operation = kOperation_5P_PG;
		break;
#endif

	default:
        pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->_operation = operation;
	return TRUE;
}

BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps)
{
    if (!pps) {
        pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	uint16_t period = 1000000.0 / (pps * p_owner->_reduction_gear_ratio) + 0.5;
	if (p_owner->_operation == SMO_2P_4W_1_2S) {
		period /= 2;
	}

	if (period < 2 * p_owner->_p_timer_manager->_period1us) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}
	p_owner->__p_timer_step->target = period / p_owner->_p_timer_manager->_period1us;

	p_owner->_current_pps = pps;
	p_owner->__step_period1us = period;

#ifndef __PIF_NO_LOG__
	if (pif_log_flag.bt.step_motor) {
		pifLog_Printf(LT_INFO, "SM(%u) %s P/S:%d SP:%uus", p_owner->_id,
				kMotorState[p_owner->_state], pps, p_owner->__step_period1us);
	}
#endif
	return TRUE;
}

BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio)
{
	if (p_owner->_state != MS_IDLE) {
        pif_error = E_INVALID_STATE;
		return FALSE;
    }

	p_owner->_reduction_gear_ratio = reduction_gear_ratio;
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
	return TRUE;
}

BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse)
{
	if (p_owner->__step_period1us < 2 * p_owner->_p_timer_manager->_period1us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!pifTimer_Start(p_owner->__p_timer_step, p_owner->__step_period1us / p_owner->_p_timer_manager->_period1us)) return FALSE;
    p_owner->__target_pulse = target_pulse;
    p_owner->_current_pulse = 0;
	return TRUE;
}

void pifStepMotor_Break(PifStepMotor* p_owner)
{
   	pifTimer_Stop(p_owner->__p_timer_step);
}

void pifStepMotor_Release(PifStepMotor* p_owner)
{
	(*p_owner->act_set_step)(0);
}

void pifStepMotor_BreakRelease(PifStepMotor* p_owner, uint16_t break_time)
{
	pifStepMotor_Break(p_owner);

	if (break_time) {
	    if (!p_owner->__p_timer_break) {
	    	p_owner->__p_timer_break = pifTimerManager_Add(p_owner->_p_timer_manager, TT_ONCE);
	    }
	    if (p_owner->__p_timer_break) {
	    	pifTimer_AttachEvtFinish(p_owner->__p_timer_break, _evtTimerBreakFinish, p_owner);
			if (pifTimer_Start(p_owner->__p_timer_break, break_time)) return;
	    }
	}

	pifStepMotor_Release(p_owner);
}

BOOL pifStepMotor_StartControl(PifStepMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = FALSE;
    return TRUE;
}

BOOL pifStepMotor_StopControl(PifStepMotor* p_owner)
{
	if (!p_owner->__p_task) {
        pif_error = E_NOT_SET_TASK;
	    return FALSE;
    }

    p_owner->__p_task->pause = TRUE;
    return TRUE;
}
