#ifndef PIF_MOTOR_ENCODER_H
#define PIF_MOTOR_ENCODER_H


#include "core/pif_pid_control.h"
#include "core/pif_pulse.h"
#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


#define MAX_STABLE_CNT				(10)


/**
 * @class StPifDutyMotorSpeedEncStage
 * @brief Stage profile for encoder-feedback speed control.
 */
typedef struct StPifDutyMotorSpeedEncStage
{
    uint8_t mode;                   // PifMotorMode bit field

    // Optional stage sensors
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;
    PifSensor* p_stop_sensor;

    // Acceleration stage
    uint8_t gs_arrive_ratio;        // Arrival ratio percentage (0-100)
    uint16_t gs_start_duty;         // Initial duty at start
    uint16_t gs_ctrl_duty;          // Duty increment per control period
    uint16_t gs_arrive_timeout;     // Timeout while waiting to reach arrival ratio

    // Constant-speed stage
    float fs_pulses_per_period;     // Target encoder pulses per control period
    uint16_t fs_high_duty;          // High duty limit
    uint16_t fs_overtime;           // Delay before deceleration after stop request
    uint16_t fs_stable_timeout;     // Timeout while waiting for stable feedback
    uint8_t fs_stable_err_low;      // Lower acceptable error percentage
    uint8_t fs_stable_err_high;     // Upper acceptable error percentage

    // Deceleration stage
    uint16_t rs_low_duty;           // Low duty used near stop threshold
    uint16_t rs_ctrl_duty;          // Duty decrement per control period
    uint16_t rs_break_time;         // Break hold time for mechanical/electrical brake
} PifDutyMotorSpeedEncStage;

/**
 * @class StPifDutyMotorSpeedEnc
 * @brief Encoder-feedback speed-control extension of `PifDutyMotor`.
 */
typedef struct StPifDutyMotorSpeedEnc
{
	// The parent variable must be at the beginning of this structure.
	PifDutyMotor parent;

	// Read-only Member Variable
    uint8_t _stage_index;

	// Private Member Variable
    uint8_t __stage_size;
    const PifDutyMotorSpeedEncStage* __p_stages;
    const PifDutyMotorSpeedEncStage* __p_current_stage;

	PifPulse* __p_encoder;
    PifPidControl __pid_control;

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
 * @fn pifDutyMotorSpeedEnc_Init
 * @brief Initializes an encoder-feedback speed-control duty motor instance.
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
 * @fn pifDutyMotorSpeedEnc_Clear
 * @brief Releases all resources owned by encoder-feedback speed control.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Clear(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_AddStages
 * @brief Registers stage table for encoder-feedback speed control.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_size, const PifDutyMotorSpeedEncStage* p_stages);

/**
 * @fn pifDutyMotorSpeedEnc_GetPidControl
 * @brief Returns the PID controller used for feedback compensation.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @return Pointer to internal `PifPidControl`.
 */
PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_Start
 * @brief Starts operation using the selected encoder stage profile.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when time-based stop mode is selected.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorSpeedEnc_Stop
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Stop(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_Emergency
 * @brief Forces immediate emergency break state.
 * @param p_owner Pointer to the encoder speed-control motor instance.
 */
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotorSpeedEnc* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
