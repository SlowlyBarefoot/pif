#ifndef PIF_STEP_MOTOR_H
#define PIF_STEP_MOTOR_H


#include "core/pif_timer_manager.h"
#include "motor/pif_motor.h"


typedef enum EnPifStepMotorOperation
{
    SMO_2P_2W           = 1,    // 2-Phase 2-Wire
    SMO_2P_4W_1S        = 2,    // 2-Phase 4-Wire: 1-Step sequence
    SMO_2P_4W_2S        = 3,    // 2-Phase 4-Wire: 2-Step sequence
    SMO_2P_4W_1_2S      = 4,    // 2-Phase 4-Wire: 1-2 Step sequence
#if 0
    SMO_5P_PG           = 5     // 5-Phase Pentagon
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
 * @brief Core step motor object that stores runtime state, timers, and callbacks.
 */
struct StPifStepMotor
{
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
 * @brief Initializes a step motor instance and allocates its core step timer.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param resolution Motor resolution in pulses-per-revolution.
 * @param operation Electrical step operation mode.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStepMotor_Init(PifStepMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t resolution, PifStepMotorOperation operation);

/**
 * @fn pifStepMotor_Clear
 * @brief Releases all timers owned by the motor instance.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Clear(PifStepMotor* p_owner);

#ifndef PIF_NO_LOG
    /**
     * @fn pifStepMotor_SetState
     * @brief Updates motor state and emits a transition log entry.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     * @param tag Log tag string.
     */
    void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state, char *tag);
#else
    /**
     * @fn pifStepMotor_SetState
     * @brief Updates motor state when logging is disabled.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     */
    void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state);
#endif

/**
 * @fn pifStepMotor_SetDirection
 * @brief Sets the rotation direction while the motor is idle.
 * @param p_owner Pointer to the motor instance.
 * @param direction Direction value used by the step sequencer.
 * @return `TRUE` if applied, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetDirection(PifStepMotor* p_owner, uint8_t direction);

/**
 * @fn pifStepMotor_SetOperatingTime
 * @brief Starts a one-shot timer that triggers deceleration after runtime expires.
 * @param p_owner Pointer to the motor instance.
 * @param operating_time Runtime value for the break timer.
 * @return `TRUE` if timer setup/start succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetOperatingTime(PifStepMotor* p_owner, uint32_t operating_time);

/**
 * @fn pifStepMotor_SetOperation
 * @brief Changes electrical phase operation mode and sequence table.
 * @param p_owner Pointer to the motor instance.
 * @param operation Step operation mode.
 * @return `TRUE` if mode is valid and motor is in valid state, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation);

/**
 * @fn pifStepMotor_SetPps
 * @brief Sets speed in pulses-per-second and updates step timer period.
 * @param p_owner Pointer to the motor instance.
 * @param pps Target pulses-per-second.
 * @return `TRUE` if speed is valid and applied, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps);

/**
 * @fn pifStepMotor_SetReductionGearRatio
 * @brief Sets gear ratio used by speed conversion.
 * @param p_owner Pointer to the motor instance.
 * @param reduction_gear_ratio Reduction gear ratio.
 * @return `TRUE` if applied in idle state, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio);

/**
 * @fn pifStepMotor_SetRpm
 * @brief Sets speed in revolutions-per-minute.
 * @param p_owner Pointer to the motor instance.
 * @param rpm Target RPM.
 * @return `TRUE` if conversion and update succeed, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetRpm(PifStepMotor* p_owner, float rpm);

/**
 * @fn pifStepMotor_GetRpm
 * @brief Returns current speed in revolutions-per-minute.
 * @param p_owner Pointer to the motor instance.
 * @return Current speed in RPM.
 */
float pifStepMotor_GetRpm(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_SetTargetPulse
 * @brief Sets finite move length in pulses.
 * @param p_owner Pointer to the motor instance.
 * @param target_pulse Number of pulses to generate before stop.
 * @return `TRUE` if accepted, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetTargetPulse(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @fn pifStepMotor_Start
 * @brief Starts stepping with optional finite target pulse.
 * @param p_owner Pointer to the motor instance.
 * @param target_pulse Number of pulses to generate; `0` for continuous run.
 * @return `TRUE` if timer start succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @fn pifStepMotor_Break
 * @brief Stops step generation immediately.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Break(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_Release
 * @brief De-energizes all motor phases.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Release(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_BreakRelease
 * @brief Stops stepping and optionally holds break before releasing phases.
 * @param p_owner Pointer to the motor instance.
 * @param break_time Break hold time; `0` releases immediately.
 */
void pifStepMotor_BreakRelease(PifStepMotor* p_owner, uint16_t break_time);

/**
 * @fn pifStepMotor_StartControl
 * @brief Resumes the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is resumed, otherwise `FALSE`.
 */
BOOL pifStepMotor_StartControl(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_StopControl
 * @brief Pauses the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is paused, otherwise `FALSE`.
 */
BOOL pifStepMotor_StopControl(PifStepMotor* p_owner);

/**
 * @fn pifStepMotor_Control
 * @brief Performs common stop-state handling for task-driven controllers.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Control(PifStepMotor* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
