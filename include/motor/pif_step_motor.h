#ifndef PIF_STEP_MOTOR_H
#define PIF_STEP_MOTOR_H


#include "core/pif_timer.h"
#include "motor/pif_motor.h"


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

    // Public Action Function
    PifActStepMotorSetStep act_set_step;

    // Public Event Function
    PifEvtStepMotorStable evt_stable;
    PifEvtStepMotorStop evt_stop;
    PifEvtStepMotorError evt_error;

	// Read-only Member Variable
    PifId _id;
    PifTimerManager* _p_timer_manager;
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

	PifTask* __p_task;

	PifTimer* __p_timer_step;
	PifTimer* __p_timer_delay;
	PifTimer* __p_timer_break;

	uint8_t __current_step;
	uint32_t __target_pulse;
	const uint16_t* __p_phase_operation;

	// Private Member Function
    PifStepMotorStopStep __stop_step;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStepMotor_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param resolution
 * @param operation
 * @return 
 */
BOOL pifStepMotor_Init(PifStepMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t resolution, PifStepMotorOperation operation);

/**
 * @fn pifStepMotor_Clear
 * @brief 
 * @param p_owner
 */
void pifStepMotor_Clear(PifStepMotor* p_owner);

#ifndef PIF_NO_LOG
	/**
	* @fn pifStepMotor_SetState
	* @brief
	* @param p_owner
	* @param state
	* @param tag
	*/
	void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state, char *tag);
#else
	/**
	 * @fn pifStepMotor_SetState
	 * @brief
	 * @param p_owner
	 * @param state
	 */
	void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state);
#endif

/**
 * @fn pifStepMotor_SetDirection
 * @brief
 * @param p_owner
 * @param direction
 * @return
 */
BOOL pifStepMotor_SetDirection(PifStepMotor* p_owner, uint8_t direction);

/**
 * @fn pifStepMotor_SetOperatingTime
 * @brief
 * @param p_owner
 * @param operating_time
 * @return
 */
BOOL pifStepMotor_SetOperatingTime(PifStepMotor* p_owner, uint32_t operating_time);

/**
 * @fn pifStepMotor_SetOperation
 * @brief
 * @param p_owner
 * @param operation
 * @return
 */
BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation);

/**
 * @fn pifStepMotor_SetPps
 * @brief
 * @param p_owner
 * @param pps
 * @return
 */
BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps);

/**
 * @fn pifStepMotor_SetReductionGearRatio
 * @brief
 * @param p_owner
 * @param reduction_gear_ratio
 * @return
 */
BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio);

/**
 * @fn pifStepMotor_SetRpm
 * @brief
 * @param p_owner
 * @param rpm
 * @return
 */
BOOL pifStepMotor_SetRpm(PifStepMotor* p_owner, float rpm);

/**
 * @fn pifStepMotor_GetRpm
 * @brief
 * @param p_owner
 * @return
 */
float pifStepMotor_GetRpm(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_SetTargetPulse
 * @brief
 * @param p_owner
 * @param target_pulse
 * @return
 */
BOOL pifStepMotor_SetTargetPulse(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @fn pifStepMotor_Start
 * @brief
 * @param p_owner
 * @param target_pulse
 * @return
 */
BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @fn pifStepMotor_Break
 * @brief
 * @param p_owner
 */
void pifStepMotor_Break(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_Release
 * @brief
 * @param p_owner
 */
void pifStepMotor_Release(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_BreakRelease
 * @brief
 * @param p_owner
 * @param break_time
 */
void pifStepMotor_BreakRelease(PifStepMotor* p_owner, uint16_t break_time);

/**
 * @fn pifStepMotor_StartControl
 * @brief 
 * @param p_owner
 * @return 
 */
BOOL pifStepMotor_StartControl(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_StopControl
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifStepMotor_StopControl(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_Control
 * @brief
 * @param p_owner
 */
void pifStepMotor_Control(PifStepMotor* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
