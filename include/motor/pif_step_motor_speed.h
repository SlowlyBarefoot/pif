#ifndef PIF_STEP_MOTOR_SPEED_H
#define PIF_STEP_MOTOR_SPEED_H


#include "motor/pif_step_motor.h"
#include "sensor/pif_sensor.h"


/**
 * @class StPifStepMotorSpeedStage
 * @brief Stage profile for speed-based step motor control.
 */
typedef struct StPifStepMotorSpeedStage
{
    uint8_t mode;               // PifMotorMode bit field

    // Optional stage sensors
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;
    PifSensor* p_stop_sensor;

    // Acceleration stage
    uint16_t gs_start_pps;      // Initial pulses-per-second at start
    uint16_t gs_ctrl_pps;       // Increment per control period (0 disables acceleration stage)

    // Constant-speed stage
    uint16_t fs_fixed_pps;      // Target constant speed in pulses-per-second
    uint16_t fs_overtime;       // Delay before deceleration after stop request

    // Deceleration stage
    uint16_t rs_stop_pps;       // Threshold speed to switch to break handling
    uint16_t rs_ctrl_pps;       // Decrement per control period (0 disables deceleration ramp)
    uint16_t rs_break_time;     // Break hold time for mechanical/electrical brake
} PifStepMotorSpeedStage;

/**
 * @class StPifStepMotorSpeed
 * @brief Speed-control extension of `PifStepMotor`.
 */
typedef struct StPifStepMotorSpeed
{
	// The parent variable must be at the beginning of this structure.
	PifStepMotor parent;

	// Read-only Member Variable
    uint8_t _stage_index;

	// Private Member Variable
    uint8_t __stage_size;
    const PifStepMotorSpeedStage* __p_stages;
    const PifStepMotorSpeedStage* __p_current_stage;
} PifStepMotorSpeed;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStepMotorSpeed_Init
 * @brief Initializes a speed-control step motor instance.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param resolution Motor resolution in pulses-per-revolution.
 * @param operation Step operation mode.
 * @param period1ms Control task period in milliseconds.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStepMotorSpeed_Init(PifStepMotorSpeed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t resolution,
		PifStepMotorOperation operation, uint16_t period1ms);

/**
 * @fn pifStepMotorSpeed_Clear
 * @brief Releases all resources owned by the speed-control motor.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifStepMotorSpeed_Clear(PifStepMotorSpeed* p_owner);

/**
 * @fn pifStepMotorSpeed_AddStages
 * @brief Registers the stage table used for speed-control operation.
 * @param p_owner Pointer to the speed-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifStepMotorSpeed_AddStages(PifStepMotorSpeed* p_owner, uint8_t stage_size, const PifStepMotorSpeedStage* p_stages);

/**
 * @fn pifStepMotorSpeed_Start
 * @brief Starts motion using the selected speed stage profile.
 * @param p_owner Pointer to the speed-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when the stage uses time-based stop mode.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotorSpeed_Start(PifStepMotorSpeed* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifStepMotorSpeed_Stop
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifStepMotorSpeed_Stop(PifStepMotorSpeed* p_owner);

/**
 * @fn pifStepMotorSpeed_Emergency
 * @brief Forces immediate emergency break state.
 * @param p_owner Pointer to the speed-control motor instance.
 */
void pifStepMotorSpeed_Emergency(PifStepMotorSpeed* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STEP_MOTOR_SPEED_H
