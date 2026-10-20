// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_step_motor.h
 * @brief Step motor driven phase by phase or through a STEP/DIR driver, run in stages.
 *
 * A step timer of a PifTimerManager gives the phases to act_set_step at the pulse rate. 2-phase and 5-phase
 * motors are driven by their phase sequences, and STEP/DIR drivers by pulses. pif_step_motor_speed and
 * pif_step_motor_pos build on it.
 *
 * Only the operations put in by PIF_USE_STEP_MOTOR_2PHASE (SMO_2P_*), PIF_USE_STEP_MOTOR_5PHASE (SMO_5P_*) and
 * PIF_USE_STEP_MOTOR_STEP_DIR (SMO_STEP_DIR) are built. pifStepMotor_Init() and pifStepMotor_SetOperation()
 * refuse the others with E_INVALID_PARAM, so a target that drives a step motor defines at least one of them.
 */
#ifndef PIF_STEP_MOTOR_H
#define PIF_STEP_MOTOR_H


#include "core/pif_timer_manager.h"
#include "motor/pif_motor.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_STEP_MOTOR)
	#define COLLECT_SIGNAL_STEP_MOTOR	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_STEP_MOTOR is too
	#include "core/pif_collect_signal.h"
#endif


/**
 * @brief How the motor is wired and driven.
 */
typedef enum EnPifStepMotorOperation
{
    SMO_2P_2W           = 1,    ///< 2-Phase 2-Wire
    SMO_2P_4W_1S        = 2,    ///< 2-Phase 4-Wire: 1-Step sequence
    SMO_2P_4W_2S        = 3,    ///< 2-Phase 4-Wire: 2-Step sequence
    SMO_2P_4W_1_2S      = 4,    ///< 2-Phase 4-Wire: 1-2 Step sequence
    SMO_5P_PG           = 5,    ///< 5-Phase Pentagon: full step, 10 steps per electrical cycle
    SMO_5P_PG_HALF      = 6,    ///< 5-Phase Pentagon: half step, 20 steps per electrical cycle
    SMO_STEP_DIR        = 7     ///< STEP/DIR driver (A4988, DRV8825, TMC...): phase is a SMSD_* bit field
} PifStepMotorOperation;

// In SMO_5P_PG and SMO_5P_PG_HALF the phase gives the 5 terminals of the pentagon, one half bridge per
// terminal. Bits 0 to 4 (SM5P_A to SM5P_E) are the levels: a set bit drives that terminal to the supply and
// a clear bit to ground. Each full step moves one terminal, so 3 and 2 terminals are high in turn.
// SMO_5P_PG_HALF puts a half step between two full steps, with the moving terminal turned off (both
// switches of its half bridge open), marked by SM5P_OFF() of that terminal; its level bit is clear then.
// The full steps energize 4 windings and the half steps 5. pifStepMotor_Release() gives 0, with every
// terminal at ground and no current.
/** @name 5-phase terminals
 *  Bits of the phase given to act_set_step in SMO_5P_PG and SMO_5P_PG_HALF. Bits 0 to 4 are the levels of the 5
 *  terminals of the pentagon: set drives the terminal to the supply, clear to ground. In a half step the moving
 *  terminal is turned off, marked by SM5P_OFF() of it. pifStepMotor_Release() gives 0.
 *  @{
 */
#define SM5P_A          0x01					///< Level of terminal A
#define SM5P_B          0x02					///< Level of terminal B
#define SM5P_C          0x04					///< Level of terminal C
#define SM5P_D          0x08					///< Level of terminal D
#define SM5P_E          0x10					///< Level of terminal E
#define SM5P_LEVEL_MASK 0x1F					///< Level bits of all terminals
#define SM5P_OFF(T)     ((uint16_t)(T) << 8)    ///< The terminal T is turned off (high impedance)
/** @} */

/// Bits of the phase given to act_set_step in SMO_STEP_DIR. Each pulse is one tick with SMSD_STEP set
/// and one tick without, and pifStepMotor_Release() gives 0, which clears SMSD_ENABLE.
#define SMSD_STEP       0x01    ///< STEP line level
#define SMSD_DIR        0x02    ///< DIR line level, set when _direction is not 0
#define SMSD_ENABLE     0x04    ///< Driver enable, active high (invert it in act_set_step if needed)


struct StPifStepMotor;
typedef struct StPifStepMotor PifStepMotor;

/**
 * @brief Drives the coils, or the STEP/DIR lines, with phase.
 *
 * The meaning of phase depends on _operation: the coil bits of a 2-phase motor, the SM5P_ bits of a 5-phase
 * motor, or the SMSD_ bits of a STEP/DIR driver.
 */
typedef void (*PifActStepMotorSetStep)(PifStepMotor* p_owner, uint16_t phase);
/**
 * @brief Sets the microstep pins of a STEP/DIR driver to microstep pulses per full step.
 */
typedef void (*PifActStepMotorSetMicrostep)(PifStepMotor* p_owner, uint16_t microstep);

/**
 * @brief Called when the motor reaches the constant speed of its stage.
 */
typedef void (*PifEvtStepMotorStable)(PifStepMotor* p_owner);
/**
 * @brief Called when the motor has stopped.
 */
typedef void (*PifEvtStepMotorStop)(PifStepMotor* p_owner);
/**
 * @brief Called when the motor stops on an error. _error tells why.
 */
typedef void (*PifEvtStepMotorError)(PifStepMotor* p_owner);

/**
 * @brief Control step of a speed or position motor, run by its control timer.
 */
typedef void (*PifStepMotorControl)(PifStepMotor* p_owner);
/**
 * @brief Called on each pulse to check whether the motor has reached its stop.
 */
typedef void (*PifStepMotorStopStep)(PifStepMotor* p_owner);

#ifdef COLLECT_SIGNAL_STEP_MOTOR

/**
 * @brief Signals of a step motor for pifStepMotor_SetCsFlag() and pifStepMotor_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifStepMotorCsFlag
{
    SM_CSF_OFF			= 0,

    SM_CSF_STATE_IDX	= 0,	///< PifMotorState
    SM_CSF_PPS_IDX		= 1,	///< Pulses per second

	SM_CSF_STATE_BIT	= 1,
	SM_CSF_PPS_BIT		= 2,
	SM_CSF_ALL_BIT		= 3,

    SM_CSF_COUNT		= 2
} PifStepMotorCsFlag;

#endif	// COLLECT_SIGNAL_STEP_MOTOR


/**
 * @class StPifStepMotor
 * @brief Core step motor object that stores runtime state, timers, and callbacks.
 */
struct StPifStepMotor
{
    // Public Action Function
    PifActStepMotorSetStep act_set_step;			///< Drives the coils or the STEP/DIR lines. Required.
    PifActStepMotorSetMicrostep act_set_microstep;  ///< Optional: sets the MS pins of a STEP/DIR driver

    // Public Event Function
    PifEvtStepMotorStable evt_stable;				///< May be NULL
    PifEvtStepMotorStop evt_stop;					///< May be NULL
    PifEvtStepMotorError evt_error;					///< May be NULL

	// Read-only Member Variable
    PifId _id;						///< Instance ID
    PifTimerManager* _p_timer_manager;	///< Timer manager of the step and control timers
    PifStepMotorOperation _operation;	///< How the motor is driven
    uint16_t _resolution;			///< Pulses per turn of the output shaft
	uint16_t _current_pps;			///< Current speed in pulses per second
    uint8_t _reduction_gear_ratio;	///< Full steps of the motor per pulse
    uint16_t _microstep;            ///< STEP pulses per full step in SMO_STEP_DIR (see pifStepMotor_SetMicrostep)
    uint8_t _direction;				///< Direction, 0 for CW and 1 for CCW
    PifMotorState _state;			///< Current state
    PifMotorError _error;           ///< Cause of the last error, cleared when a stage starts
    uint32_t _current_pulse;        ///< Pulses since start, in the same unit as pps (see pifStepMotor_SetPps)

	// Private Member Variable
    BOOL __error_notified;      ///< evt_error was already called for the current error
	uint8_t __step_size;
	uint32_t __ticks_per_pulse;	///< Step timer ticks per pulse: gear ratio x half step or STEP/DIR edges x microstep
	uint32_t __tick_count;
	uint32_t __step_period1us;	///< Step timer tick period

	PifTask* __p_task;

	PifTimer* __p_timer_step;
	PifTimer* __p_timer_delay;
	PifTimer* __p_timer_brake;

	uint8_t __current_step;
	uint32_t __target_pulse;
	const uint16_t* __p_phase_operation;
#ifdef COLLECT_SIGNAL_STEP_MOTOR
	PifCollectSignalChannel __cs[SM_CSF_COUNT];
#endif

	// Private Member Function
    PifStepMotorStopStep __stop_step;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
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
 * @brief Releases all timers owned by the motor instance.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Clear(PifStepMotor* p_owner);

#ifndef PIF_NO_LOG
    /**
     * @brief Updates motor state and emits a transition log entry.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     * @param tag Log tag string.
     */
    void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state, const char *tag);
#else
    /**
     * @brief Updates motor state when logging is disabled.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     */
    void pifStepMotor_SetState(PifStepMotor* p_owner, PifMotorState state);
#endif

/**
 * @brief Sets the rotation direction while the motor is idle.
 * @param p_owner Pointer to the motor instance.
 * @param direction Direction value used by the step sequencer.
 * @return `TRUE` if applied, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetDirection(PifStepMotor* p_owner, uint8_t direction);

/**
 * @brief Starts a one-shot timer that triggers deceleration after runtime expires.
 * @param p_owner Pointer to the motor instance.
 * @param operating_time Runtime value for the brake timer.
 * @return `TRUE` if timer setup/start succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetOperatingTime(PifStepMotor* p_owner, uint32_t operating_time);

/**
 * @brief Changes electrical phase operation mode and sequence table.
 * @param p_owner Pointer to the motor instance.
 * @param operation Step operation mode.
 * @return `TRUE` if mode is valid and motor is in valid state, otherwise `FALSE`. A mode that
 *         PIF_USE_STEP_MOTOR_2PHASE, PIF_USE_STEP_MOTOR_5PHASE or PIF_USE_STEP_MOTOR_STEP_DIR did not put in the
 *         build is refused with E_INVALID_PARAM.
 */
BOOL pifStepMotor_SetOperation(PifStepMotor* p_owner, PifStepMotorOperation operation);

/**
 * @brief Sets speed in pulses-per-second and updates step timer period.
 *
 * A pulse is one full step of the output shaft: `_resolution` pulses turn it once. The motor itself
 * makes `_reduction_gear_ratio` full steps per pulse, two step timer ticks for each of them in
 * SMO_2P_4W_1_2S and SMO_5P_PG_HALF. In SMO_STEP_DIR a full step is `_microstep` STEP pulses of two
 * ticks each. `_current_pulse` and the target pulse use the same unit.
 * @param p_owner Pointer to the motor instance.
 * @param pps Target pulses-per-second.
 * @return `TRUE` if speed is valid and applied, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetPps(PifStepMotor* p_owner, uint16_t pps);

/**
 * @brief Sets the gear ratio: motor full steps per pulse of the output shaft.
 * @param p_owner Pointer to the motor instance.
 * @param reduction_gear_ratio Reduction gear ratio, 1 or more.
 * @return `TRUE` if applied in idle state, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetReductionGearRatio(PifStepMotor* p_owner, uint8_t reduction_gear_ratio);

/**
 * @brief Sets the microstep division of a STEP/DIR driver: STEP pulses per full step.
 *
 * Give the division the driver is set to, and set `act_set_microstep` to have it set the MS pins of the
 * driver too. The unit of pps and pulses stays one full step, so the speed is kept. The division is used
 * in SMO_STEP_DIR only.
 * @param p_owner Pointer to the motor instance.
 * @param microstep 1 (full step), 2, 4, 8 and so on up to 256.
 * @return `TRUE` if applied while the motor does not step, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetMicrostep(PifStepMotor* p_owner, uint16_t microstep);

/**
 * @brief Sets speed in revolutions-per-minute.
 * @param p_owner Pointer to the motor instance.
 * @param rpm Target RPM.
 * @return `TRUE` if conversion and update succeed, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetRpm(PifStepMotor* p_owner, float rpm);

/**
 * @brief Returns current speed in revolutions-per-minute.
 * @param p_owner Pointer to the motor instance.
 * @return Current speed in RPM.
 */
float pifStepMotor_GetRpm(PifStepMotor* p_owner);

/**
 * @brief Sets finite move length in pulses.
 * @param p_owner Pointer to the motor instance.
 * @param target_pulse Number of pulses to generate before stop.
 * @return `TRUE` if accepted, otherwise `FALSE`.
 */
BOOL pifStepMotor_SetTargetPulse(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @brief Starts stepping with optional finite target pulse.
 * @param p_owner Pointer to the motor instance.
 * @param target_pulse Number of pulses to generate; `0` for continuous run.
 * @return `TRUE` if timer start succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotor_Start(PifStepMotor* p_owner, uint32_t target_pulse);

/**
 * @brief Stops step generation immediately.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Brake(PifStepMotor* p_owner);

/**
 * @brief De-energizes all motor phases (in SMO_STEP_DIR, disables the driver).
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Release(PifStepMotor* p_owner);

/**
 * @brief Stops stepping and optionally holds brake before releasing phases.
 * @param p_owner Pointer to the motor instance.
 * @param brake_time Brake hold time; `0` releases immediately.
 */
void pifStepMotor_BrakeRelease(PifStepMotor* p_owner, uint16_t brake_time);

/**
 * @brief Resumes the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is resumed, otherwise `FALSE`.
 */
BOOL pifStepMotor_StartControl(PifStepMotor* p_owner);

/**
 * @brief Pauses the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is paused, otherwise `FALSE`.
 */
BOOL pifStepMotor_StopControl(PifStepMotor* p_owner);

/**
 * @brief Performs common stop-state handling for task-driven controllers.
 * @param p_owner Pointer to the motor instance.
 */
void pifStepMotor_Control(PifStepMotor* p_owner);

#ifdef COLLECT_SIGNAL_STEP_MOTOR

/**
 * @brief Adds the selected signals of the motor to pifCollectSignal as channels.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifStepMotor_SetCsFlag(PifStepMotor* p_owner, PifStepMotorCsFlag flag);

/**
 * @brief Removes the selected signals of the motor from pifCollectSignal.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifStepMotor_ResetCsFlag(PifStepMotor* p_owner, PifStepMotorCsFlag flag);

#endif	// COLLECT_SIGNAL_STEP_MOTOR

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
