#ifndef PIF_DUTY_MOTOR_SPEED_H
#define PIF_DUTY_MOTOR_SPEED_H


#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


/**
 * @class StPifDutyMotorSpeedStage
 * @brief Stage profile for speed-based duty motor control.
 */
typedef struct StPifDutyMotorSpeedStage
{
    uint8_t mode;               // PifMotorMode bit field

    // Optional stage sensors
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;
    PifSensor* p_stop_sensor;

    // Acceleration stage
    uint16_t gs_start_duty;     // Initial duty at start
    uint16_t gs_ctrl_duty;      // Duty increment per control period (0 disables acceleration stage)

    // Constant-speed stage
    uint16_t fs_high_duty;      // Target duty for constant-speed stage
    uint16_t fs_overtime;       // Delay before deceleration after stop request

    // Deceleration stage
    uint16_t rs_low_duty;       // Duty level used near stop threshold
    uint16_t rs_ctrl_duty;      // Duty decrement per control period (0 disables ramped deceleration)
    uint16_t rs_break_time;     // Break hold time for mechanical/electrical brake
} PifDutyMotorSpeedStage;

/**
 * @class StPifDutyMotorSpeed
 * @brief Speed-control extension of `PifDutyMotor`.
 */
typedef struct StPifDutyMotorSpeed
{
	// The parent variable must be at the beginning of this structure.
	PifDutyMotor parent;

	// Read-only Member Variable
    uint8_t _stage_index;

	// Private Member Variable
    uint8_t __stage_size;
    const PifDutyMotorSpeedStage* __p_stages;
    const PifDutyMotorSpeedStage* __p_current_stage;
} PifDutyMotorSpeed;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDutyMotorSpeed_Init
 * @brief Initializes a speed-control duty motor instance.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param max_duty Maximum supported duty value.
 * @param period1ms Control task period in milliseconds.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeed_Init(PifDutyMotorSpeed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty, uint16_t period1ms);

/**
 * @fn pifDutyMotorSpeed_Clear
 * @brief Releases all resources owned by the speed-control motor.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifDutyMotorSpeed_Clear(PifDutyMotorSpeed* p_owner);

/**
 * @fn pifDutyMotorSpeed_AddStages
 * @brief Registers stage table for speed-control operation.
 * @param p_owner Pointer to the speed-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeed_AddStages(PifDutyMotorSpeed* p_owner, uint8_t stage_size, const PifDutyMotorSpeedStage* p_stages);

/**
 * @fn pifDutyMotorSpeed_Start
 * @brief Starts operation using the selected speed stage profile.
 * @param p_owner Pointer to the speed-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when time-based stop mode is selected.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotorSpeed_Start(PifDutyMotorSpeed* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorSpeed_Stop
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifDutyMotorSpeed_Stop(PifDutyMotorSpeed* p_owner);

/**
 * @fn pifDutyMotorSpeed_Emergency
 * @brief Forces immediate emergency break state.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifDutyMotorSpeed_Emergency(PifDutyMotorSpeed* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_SPEED_H
