// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "core/pif_timer_manager.h"
#include "motor/pif_motor.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


struct StPifDutyMotor;
typedef struct StPifDutyMotor PifDutyMotor;

typedef void (*PifActDutyMotorSetDuty)(uint16_t duty);
typedef void (*PifActDutyMotorSetDirection)(uint8_t dir);
typedef void (*PifActDutyMotorOperateBreak)(uint8_t state);

typedef void (*PifEvtDutyMotorStable)(PifDutyMotor* p_owner);
typedef void (*PifEvtDutyMotorStop)(PifDutyMotor* p_owner);
typedef void (*PifEvtDutyMotorError)(PifDutyMotor* p_owner);

typedef void (*PifDutyMotorControl)(PifDutyMotor* p_owner);

#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifDutyMotorCsFlag
{
    DM_CSF_OFF			= 0,

    DM_CSF_STATE_IDX	= 0,	// PifMotorState
    DM_CSF_DUTY_IDX		= 1,	// Duty given to act_set_duty

	DM_CSF_STATE_BIT	= 1,
	DM_CSF_DUTY_BIT		= 2,
	DM_CSF_ALL_BIT		= 3,

    DM_CSF_COUNT		= 2
} PifDutyMotorCsFlag;

#endif	// PIF_COLLECT_SIGNAL


/**
 * @class StPifDutyMotor
 * @brief Core duty motor object that stores runtime state, timers, and callbacks.
 */
struct StPifDutyMotor
{
    // Public Action Function
    PifActDutyMotorSetDuty act_set_duty;
    PifActDutyMotorSetDirection act_set_direction;
    PifActDutyMotorOperateBreak act_operate_break;

    // Public Event Function
    PifEvtDutyMotorStable evt_stable;
    PifEvtDutyMotorStop evt_stop;
    PifEvtDutyMotorError evt_error;

    // Read-only Member Variable
    PifId _id;
    PifTimerManager* _p_timer_manager;
	uint16_t _max_duty;
	uint16_t _current_duty;
	uint8_t _direction;
    PifMotorState _state;

	// Private Member Variable
    uint8_t __error;

	PifTimer* __p_timer_delay;
	PifTimer* __p_timer_break;

    PifTask* __p_task;
#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[DM_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDutyMotor_Init
 * @brief Initializes a duty motor instance.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param max_duty Maximum supported duty value.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty);

/**
 * @fn pifDutyMotor_Clear
 * @brief Releases all timers owned by the motor instance.
 * @param p_owner Pointer to the motor instance.
 */
void pifDutyMotor_Clear(PifDutyMotor* p_owner);

#ifndef PIF_NO_LOG
    /**
     * @fn pifDutyMotor_SetState
     * @brief Updates motor state and emits a transition log entry.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     * @param tag Log tag string.
     */
    void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state, char *tag);
#else
    /**
     * @fn pifDutyMotor_SetState
     * @brief Updates motor state when logging is disabled.
     * @param p_owner Pointer to the motor instance.
     * @param state New motor state.
     */
    void pifDutyMotor_SetState(PifDutyMotor* p_owner, PifMotorState state);
#endif

/**
 * @fn pifDutyMotor_SetDirection
 * @brief Sets output direction and forwards it through direction callback.
 * @param p_owner Pointer to the motor instance.
 * @param direction Direction value.
 */
void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction);

/**
 * @fn pifDutyMotor_SetDuty
 * @brief Sets current duty with clamping to `_max_duty`.
 * @param p_owner Pointer to the motor instance.
 * @param duty Target duty value.
 */
void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @fn pifDutyMotor_SetOperatingTime
 * @brief Starts a one-shot timer that triggers deceleration after runtime expires.
 * @param p_owner Pointer to the motor instance.
 * @param operating_time Runtime value for the break timer.
 * @return `TRUE` if timer setup/start succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time);

/**
 * @fn pifDutyMotor_Start
 * @brief Starts motor output with current direction and requested duty.
 * @param p_owner Pointer to the motor instance.
 * @param duty Initial duty value.
 * @return `TRUE` if required callbacks exist and output is applied, otherwise `FALSE`.
 */
BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @fn pifDutyMotor_BreakRelease
 * @brief Sets duty to zero and optionally engages timed break handling.
 * @param p_owner Pointer to the motor instance.
 * @param break_time Break hold time; `0` skips timed break.
 */
void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time);

/**
 * @fn pifDutyMotor_StartControl
 * @brief Resumes the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is resumed, otherwise `FALSE`.
 */
BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner);

/**
 * @fn pifDutyMotor_StopControl
 * @brief Pauses the periodic control task.
 * @param p_owner Pointer to the motor instance.
 * @return `TRUE` if task exists and is paused, otherwise `FALSE`.
 */
BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner);

/**
 * @fn pifDutyMotor_Control
 * @brief Performs common stop/error handling for task-driven controllers.
 * @param p_owner Pointer to the motor instance.
 */
void pifDutyMotor_Control(PifDutyMotor* p_owner);

/**
 * @fn pifDutyMotor_ApplyDuty
 * @brief Gives a duty to act_set_duty and records it for pifCollectSignal. Used by the motor
 *        controllers; it does not change `_current_duty`.
 * @param p_owner Pointer to the motor instance.
 * @param duty Duty value to output.
 */
void pifDutyMotor_ApplyDuty(PifDutyMotor* p_owner, uint16_t duty);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifDutyMotor_SetCsFlag
 * @brief Adds the selected signals of the motor to pifCollectSignal as channels.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDutyMotor_SetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag);

/**
 * @fn pifDutyMotor_ResetCsFlag
 * @brief Removes the selected signals of the motor from pifCollectSignal.
 * @param p_owner Pointer to the motor instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifDutyMotor_ResetCsFlag(PifDutyMotor* p_owner, PifDutyMotorCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
