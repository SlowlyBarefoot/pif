// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_duty_motor.h
 * @brief Motor driven by a PWM duty, with direction and brake, run in stages.
 *
 * The platform sets the duty, the direction and the brake through the act_ functions. pif_duty_motor_speed,
 * pif_duty_motor_speed_enc and pif_duty_motor_pos build on it.
 */
#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "core/pif_timer_manager.h"
#include "motor/pif_motor.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_DUTY_MOTOR)
	#define COLLECT_SIGNAL_DUTY_MOTOR	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_DUTY_MOTOR is too
	#include "core/pif_collect_signal.h"
#endif


struct StPifDutyMotor;
typedef struct StPifDutyMotor PifDutyMotor;

/**
 * @brief Sets the PWM duty of the motor, 0 to _max_duty.
 */
typedef void (*PifActDutyMotorSetDuty)(PifDutyMotor* p_owner, uint16_t duty);
/**
 * @brief Sets the direction of the motor, 0 for CW and 1 for CCW.
 */
typedef void (*PifActDutyMotorSetDirection)(PifDutyMotor* p_owner, uint8_t dir);
/**
 * @brief Engages (ON) or releases (OFF) the brake.
 */
typedef void (*PifActDutyMotorOperateBrake)(PifDutyMotor* p_owner, uint8_t state);

/**
 * @brief Called when the motor reaches the constant speed of its stage.
 */
typedef void (*PifEvtDutyMotorStable)(PifDutyMotor* p_owner);
/**
 * @brief Called when the motor has stopped.
 */
typedef void (*PifEvtDutyMotorStop)(PifDutyMotor* p_owner);
/**
 * @brief Called when the motor stops on an error. _error tells why.
 */
typedef void (*PifEvtDutyMotorError)(PifDutyMotor* p_owner);

/**
 * @brief Control step of a speed or position motor, run by its control timer.
 */
typedef void (*PifDutyMotorControl)(PifDutyMotor* p_owner);

#ifdef COLLECT_SIGNAL_DUTY_MOTOR

/**
 * @brief Signals of a duty motor for pifDutyMotor_SetCsFlag() and pifDutyMotor_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifDutyMotorCsFlag
{
    DM_CSF_OFF			= 0,

    DM_CSF_STATE_IDX	= 0,	///< PifMotorState
    DM_CSF_DUTY_IDX		= 1,	///< Duty given to act_set_duty

	DM_CSF_STATE_BIT	= 1,
	DM_CSF_DUTY_BIT		= 2,
	DM_CSF_ALL_BIT		= 3,

    DM_CSF_COUNT		= 2
} PifDutyMotorCsFlag;

#endif	// COLLECT_SIGNAL_DUTY_MOTOR


/**
 * @class StPifDutyMotor
 * @brief Core duty motor object that stores runtime state, timers, and callbacks.
 */
struct StPifDutyMotor
{
    // Public Action Function
    PifActDutyMotorSetDuty act_set_duty;	///< Sets the duty. Required.
    PifActDutyMotorSetDirection act_set_direction;	///< Sets the direction. May be NULL.
    PifActDutyMotorOperateBrake act_operate_brake;	///< Operates the brake. May be NULL.

    // Public Event Function
    PifEvtDutyMotorStable evt_stable;	///< May be NULL
    PifEvtDutyMotorStop evt_stop;	///< May be NULL
    PifEvtDutyMotorError evt_error;	///< May be NULL

    // Read-only Member Variable
    PifId _id;						///< Instance ID
    PifTask* _p_task;
    PifTimerManager* _p_timer_manager;	///< Timer manager of the control timers
	uint16_t _max_duty;				///< Duty that means full power
	uint16_t _current_duty;			///< Duty given last to act_set_duty
	uint8_t _direction;				///< Direction given last, 0 for CW and 1 for CCW
    PifMotorState _state;			///< Current state
    PifMotorError _error;           ///< Cause of the last error, cleared when a stage starts

	// Private Member Variable
    BOOL __error_notified;      ///< evt_error was already called for the current error

	PifTimer* __p_timer_delay;
	PifTimer* __p_timer_brake;

#ifdef COLLECT_SIGNAL_DUTY_MOTOR
	PifCollectSignalChannel __cs[DM_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a duty motor instance.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param max_duty Maximum supported duty value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty);

/**
 * @brief Releases all timers owned by the motor instance.
 * @param p_owner Pointer to the motor instance.
 */
void pifDutyMotor_Clear(PifDutyMotor* p_owner);

#ifndef PIF_NO_LOG
    /**
     * @brief Updates motor state and emits a transition log entry.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     * @param tag Log tag string.
     */
    void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state, const char *tag);
#else
    /**
     * @brief Updates motor state when logging is disabled.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     */
    void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state);
#endif

/**
 * @brief Sets output direction and forwards it through direction callback.
 * @param p_owner Pointer to the motor instance.
 * @param direction Direction value.
 */
void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction);

/**
 * @brief Sets current duty with clamping to `_max_duty`.
 * @param p_owner Pointer to the motor instance.
 * @param duty Target duty value.
 */
void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @brief Starts a one-shot timer that triggers deceleration after runtime expires.
 * @param p_owner Pointer to the motor instance.
 * @param operating_time Runtime value for the brake timer.
 * @return `TRUE` if timer setup/start succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time);

/**
 * @brief Starts motor output with current direction and requested duty.
 * @param p_owner Pointer to the motor instance.
 * @param duty Initial duty value.
 * @return `TRUE` if required callbacks exist and output is applied, otherwise `FALSE`.
 */
BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @brief Sets duty to zero and optionally engages timed brake handling.
 * @param p_owner Pointer to the motor instance.
 * @param brake_time Brake hold time; `0` skips timed brake.
 */
void pifDutyMotor_BrakeRelease(PifDutyMotor* p_owner, uint16_t brake_time);

/**
 * @brief Resumes the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is resumed, otherwise `FALSE`.
 */
BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner);

/**
 * @brief Pauses the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is paused, otherwise `FALSE`.
 */
BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner);

/**
 * @brief Performs common stop/error handling for task-driven controllers.
 * @param p_owner Pointer to the motor instance.
 */
void pifDutyMotor_Control(PifDutyMotor* p_owner);

/**
 * @brief Gives a duty to act_set_duty and records it for pifCollectSignal. Used by the motor
 *        controllers; it does not change `_current_duty`.
 * @param p_owner Pointer to the motor instance.
 * @param duty Duty value to output.
 */
void pifDutyMotor_ApplyDuty(PifDutyMotor* p_owner, uint16_t duty);

#ifdef COLLECT_SIGNAL_DUTY_MOTOR

/**
 * @brief Adds the selected signals of the motor to pifCollectSignal as channels.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDutyMotor_SetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag);

/**
 * @brief Removes the selected signals of the motor from pifCollectSignal.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifDutyMotor_ResetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag);

#endif	// COLLECT_SIGNAL_DUTY_MOTOR

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
