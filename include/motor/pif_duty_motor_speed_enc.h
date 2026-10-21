// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_duty_motor_speed_enc.h
 * @brief Duty motor that keeps the speed of its stage from encoder feedback, with a PID controller.
 *
 * The encoder pulses go to the PifPulse given to pifDutyMotorSpeedEnc_Init(), and the control task adjusts the
 * duty to the speed of the stage, measured one of two ways:
 *
 * - Count control: the pulses counted in each control period, against fs_pulses_per_period. It needs an encoder
 *   that gives several pulses in a period.
 * - Period control, when fs_period_us is set: the time between the pulses, averaged by the PifPulse, against
 *   fs_period_us. It suits an encoder that gives few pulses or less than one in a period. The speed is then per
 *   mille of the target (PIF_DMSE_PERIOD_SCALE at fs_period_us), and the duty is adjusted only after a new pulse,
 *   or after the timeout of the PifPulse has passed with none, which reads as speed 0. Give the PifPulse a
 *   timeout with pifPulse_SetTimeout(), or a stalled motor keeps the speed it had last.
 */
#ifndef PIF_DUTY_MOTOR_SPEED_ENC_H
#define PIF_DUTY_MOTOR_SPEED_ENC_H


#include "core/pif_pid_control.h"
#include "sensor/pif_pulse.h"
#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


#define MAX_STABLE_CNT				(10)	///< Encoder samples averaged to check that the speed is stable
#define PIF_DMSE_PERIOD_SCALE		1000	///< Speed at the target in period control: the speeds are per mille


/**
 * @class StPifDutyMotorSpeedEncStage
 * @brief Stage profile for encoder-feedback speed control.
 */
typedef struct StPifDutyMotorSpeedEncStage
{
    uint8_t mode;                   ///< PifMotorMode bit field

    /// Optional stage sensors
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;		///< Sensor that starts the deceleration, or NULL
    PifSensor* p_stop_sensor;		///< Sensor that stops the motor, or NULL

    /// Acceleration stage
    uint8_t gs_arrive_ratio;        ///< Speed that ends the acceleration, in percent of the target (0-100)
    uint16_t gs_start_duty;         ///< Initial duty at start
    uint16_t gs_ctrl_duty;          ///< Duty increment per control period (0 starts at the constant speed)
    uint16_t gs_arrive_timeout;     ///< Timeout while waiting to reach arrival ratio, 0 for none

    /// Constant-speed stage
    float fs_pulses_per_period;     ///< Target encoder pulses per control period, for count control
    uint32_t fs_period_us;          ///< Target time between encoder pulses in us for period control, up to 4294967.
                                    ///< 0 selects count control.
    uint16_t fs_high_duty;          ///< High duty limit
    uint16_t fs_overtime;           ///< Delay before deceleration after stop request
    uint16_t fs_stable_timeout;     ///< Timeout while waiting for stable feedback, 0 for none
    uint8_t fs_stable_err_low;      ///< Lowest stable speed, in percent of the target
    uint8_t fs_stable_err_high;     ///< Highest stable speed, in percent of the target
    uint16_t fs_dead_band;          ///< Speed error, in per mille of the target, that the PID leaves alone. 0 for none.
    uint16_t fs_low_duty;           ///< Lowest duty the PID lowers the duty to. 0 for none.

    /// Deceleration stage
    uint16_t rs_low_duty;           ///< Low duty used near stop threshold
    uint16_t rs_ctrl_duty;          ///< Duty decrement per control period
    uint16_t rs_brake_time;         ///< Brake hold time for mechanical/electrical brake
} PifDutyMotorSpeedEncStage;

/**
 * @class StPifDutyMotorSpeedEnc
 * @brief Encoder-feedback speed-control extension of `PifDutyMotor`.
 */
typedef struct StPifDutyMotorSpeedEnc
{
	/// The parent variable must be at the beginning of this structure.
	PifDutyMotor parent;

	// Read-only Member Variable
    uint8_t _stage_index;	///< Index of the current stage

	// Private Member Variable
    uint8_t __stage_size;
    const PifDutyMotorSpeedEncStage* __p_stages;
    const PifDutyMotorSpeedEncStage* __p_current_stage;

	PifPulse* __p_encoder;
    PifPidControl __pid_control;
	float __target;					///< Speed of the stage: fs_pulses_per_period, or PIF_DMSE_PERIOD_SCALE

	uint16_t __arrive_ppr;
	uint16_t __err_low_ppr;
	uint16_t __err_high_ppr;
	uint8_t __enc_sample_idx;
	uint16_t __enc_sample[MAX_STABLE_CNT];
	uint32_t __enc_sample_sum;
} PifDutyMotorSpeedEnc;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an encoder-feedback speed-control duty motor instance. The encoder counts its falling
 *        edges and measures their period from then on, which both ways of control use.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param max_duty Maximum supported duty value.
 * @param period1ms Control task period in milliseconds.
 * @param p_encoder Pulse encoder used for feedback.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeedEnc_Init(PifDutyMotorSpeedEnc* p_owner, PifId id, PifTimerManager* p_timer_manager,
		uint16_t max_duty, uint16_t period1ms, PifPulse* p_encoder);

/**
 * @brief Releases all resources owned by encoder-feedback speed control.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Clear(PifDutyMotorSpeedEnc* p_owner);

/**
 * @brief Registers stage table for encoder-feedback speed control.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_size, const PifDutyMotorSpeedEncStage* p_stages);

/**
 * @brief Returns the PID controller used for feedback compensation.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @return Pointer to internal `PifPidControl`.
 */
PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotorSpeedEnc* p_owner);

/**
 * @brief Starts operation using the selected encoder stage profile. act_set_duty is required, act_set_direction
 *        is not. In period control the edges measured before are discarded.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when time-based stop mode is selected.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Stop(PifDutyMotorSpeedEnc* p_owner);

/**
 * @brief Forces immediate emergency brake state.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotorSpeedEnc* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_SPEED_ENC_H
