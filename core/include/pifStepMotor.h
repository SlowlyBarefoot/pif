#ifndef PIF_STEP_MOTOR_H
#define PIF_STEP_MOTOR_H


#include "pifPulse.h"
#include "pifMotor.h"


typedef enum EnPifStepMotorMethod
{
	SMM_TIMER		= 0,
	SMM_TASK		= 1
} PifStepMotorMethod;

typedef enum EnPifStepMotorOperation
{
	SMO_2P_2W			= 1,	// 2-Phase 2-Wire
	SMO_2P_4W_1S		= 2,	// 2-Phase 4-Wire : 1 Step
	SMO_2P_4W_2S		= 3,	// 2-Phase 4-Wire : 2 Step
	SMO_2P_4W_1_2S		= 4,	// 2-Phase 4-Wire : 1-2 Step
#if 0
	SMO_5P_PG			= 5		// 5-Phase Pantagon
#endif
} PifStepMotorOperation;


struct StPifStepMotor;
typedef struct StPifStepMotor PifStepMotor;

typedef void (*PifActStepMotorSetStep)(uint16_t phase);

typedef void (*PifEvtStepMotorStable)(PifStepMotor* p_owner);
typedef void (*PifEvtStepMotorStop)(PifStepMotor* p_owner);
typedef void (*PifEvtStepMotorError)(PifStepMotor* p_owner);

typedef void (*PifStepMotorControl)(PifStepMotor* p_owner);
typedef void (*PifStepMotorStopStep)(PifStepMotor* p_owner);


/**
 * @class StPifStepMotor
 * @brief
 */
struct StPifStepMotor
{
	// Public Member Variable

    // Public Event Function
    PifEvtStepMotorStable evt_stable;
    PifEvtStepMotorStop evt_stop;
    PifEvtStepMotorError evt_error;

	// Read-only Member Variable
    PifId _id;
    PifPulse* _p_timer;
    PifStepMotorMethod _method;
    PifStepMotorOperation _operation;
    uint16_t _resolution;
	uint16_t _current_pps;
    uint8_t _reduction_gear_ratio;
    uint8_t _direction;
    PifMotorState _state;
    uint32_t _current_pulse;

	// Private Member Variable
    uint8_t __error;
	uint8_t __step_size;
	uint16_t __step_period1us;
	uint16_t __control_period1ms;

	PifTask* __p_task;

	PifPulseItem* __p_timer_step;
	PifPulseItem* __p_timer_control;
	PifPulseItem* __p_timer_delay;
	PifPulseItem* __p_timer_break;

	uint8_t __current_step;
	uint32_t __target_pulse;
	const uint16_t* __p_phase_operation;

    // Private Action Function
    PifActStepMotorSetStep __act_set_step;

	// Private Member Function
    PifStepMotorControl __control;
    PifStepMotorStopStep __stop_step;
};


#ifdef __cplusplus
extern "C" {
#endif

PifStepMotor* pifStepMotor_Create(PifId id, PifPulse* p_timer, uint16_t resolution, PifStepMotorOperation operation);
void pifStepMotor_Destroy(PifStepMotor** pp_owner);

BOOL pifStepMotor_Init(PifStepMotor* p_owner, PifId id, PifPulse* p_timer, uint16_t resolution, PifStepMotorOperation operation);
void pifStepMotor_Clear(PifStepMotor* p_owner);

void pifStepMotor_AttachAction(PifStepMotor* p_owner, PifActStepMotorSetStep act_set_step);

BOOL pifStepMotor_SetDirection(PifStepMotor* p_owner, uint8_t direction);
BOOL pifStepMotor_SetMethod(PifStepMotor* p_owner, PifStepMotorMethod method);
BOOL pifStepMotor_SetOperatingTime(PifStepMotor* p_owner, uint32_t operating_time);
BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation);
BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps);
BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio);
BOOL pifStepMotor_SetRpm(PifStepMotor* p_owner, float rpm);
float pifStepMotor_GetRpm(PifStepMotor* p_owner);
BOOL pifStepMotor_SetTargetPulse(PifStepMotor* p_owner, uint32_t target_pulse);

BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse);
void pifStepMotor_Break(PifStepMotor* p_owner);
void pifStepMotor_Release(PifStepMotor* p_owner);
void pifStepMotor_BreakRelease(PifStepMotor* p_owner, uint16_t break_time);

BOOL pifStepMotor_InitControl(PifStepMotor* p_owner, uint16_t control_period1ms);
BOOL pifStepMotor_StartControl(PifStepMotor* p_owner);

// Task Function
PifTask* pifStepMotor_AttachTask(PifStepMotor* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
