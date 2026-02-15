#ifndef PIF_STEP_MOTOR_POS_H
#define PIF_STEP_MOTOR_POS_H


#include "motor/pif_step_motor.h"
#include "sensor/pif_sensor.h"


/**
 * @class StPifStepMotorPosStage
 * @brief Stage profile for pulse-position step motor control.
 */
typedef struct StPifStepMotorPosStage
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
    uint16_t fs_high_pps;       // High-speed pulses-per-second
    uint32_t fs_pulse_count;    // Pulse count at which deceleration begins

    // Deceleration stage
    uint16_t rs_low_pps;        // Low-speed pulses-per-second near final position
    uint16_t rs_ctrl_pps;       // Decrement per control period (0 disables ramped deceleration)
    uint16_t rs_break_time;     // Break hold time for mechanical/electrical brake

    uint32_t total_pulse;       // Total pulses for the move
} PifStepMotorPosStage;

/**
 * @class StPifStepMotorPos
 * @brief Position-control extension of `PifStepMotor`.
 */
typedef struct StPifStepMotorPos
{
	// The parent variable must be at the beginning of this structure.
	PifStepMotor parent;

	// Read-only Member Variable
    uint8_t _stage_index;

	// Private Member Variable
    uint8_t __stage_size;
    const PifStepMotorPosStage* __p_stages;
    const PifStepMotorPosStage* __p_current_stage;
} PifStepMotorPos;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifStepMotorPos_Init
 * @brief Initializes a position-control step motor instance.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param resolution Motor resolution in pulses-per-revolution.
 * @param operation Step operation mode.
 * @param period1ms Control task period in milliseconds.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifStepMotorPos_Init(PifStepMotorPos* p_owner, PifId id, PifTimerManager* p_timer_manager, uint8_t resolution,
		PifStepMotorOperation operation, uint16_t period1ms);

/**
 * @fn pifStepMotorPos_Clear
 * @brief Releases all resources owned by the position-control motor.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifStepMotorPos_Clear(PifStepMotorPos* p_owner);

/**
 * @fn pifStepMotorPos_AddStages
 * @brief Registers the stage table used for position-control operation.
 * @param p_owner Pointer to the position-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifStepMotorPos_AddStages(PifStepMotorPos* p_owner, uint8_t stage_size, const PifStepMotorPosStage* p_stages);

/**
 * @fn pifStepMotorPos_Start
 * @brief Starts motion using the selected position stage profile.
 * @param p_owner Pointer to the position-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when the stage uses time-based stop mode.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifStepMotorPos_Start(PifStepMotorPos* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifStepMotorPos_Stop
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifStepMotorPos_Stop(PifStepMotorPos* p_owner);

/**
 * @fn pifStepMotorPos_Emergency
 * @brief Forces immediate emergency break state.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifStepMotorPos_Emergency(PifStepMotorPos* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_POS_H
