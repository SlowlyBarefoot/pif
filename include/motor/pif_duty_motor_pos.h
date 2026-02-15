#ifndef PIF_DUTY_MOTOR_POS_H
#define PIF_DUTY_MOTOR_POS_H


#include "core/pif_pulse.h"
#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


/**
 * @class StPifDutyMotorPosStage
 * @brief Stage profile for pulse-position duty motor control.
 */
typedef struct StPifDutyMotorPosStage
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
    uint16_t fs_high_duty;      // High duty value during run
    uint32_t fs_pulse_count;    // Pulse count at which deceleration begins

    // Deceleration stage
    uint16_t rs_low_duty;       // Low duty used near final position
    uint16_t rs_ctrl_duty;      // Duty decrement per control period (0 disables ramped deceleration)
    uint16_t rs_break_time;     // Break hold time for mechanical/electrical brake

    uint32_t total_pulse;       // Total pulses for the move
} PifDutyMotorPosStage;

/**
 * @class StPifDutyMotorPos
 * @brief Position-control extension of `PifDutyMotor`.
 */
typedef struct StPifDutyMotorPos
{
	// The parent variable must be at the beginning of this structure.
	PifDutyMotor parent;

	// Read-only Member Variable
	uint8_t _stage_index;

	// Private Member Variable
    uint8_t __stage_size;
    const PifDutyMotorPosStage* __p_stages;
    const PifDutyMotorPosStage* __p_current_stage;

    PifPulse* __p_encoder;
} PifDutyMotorPos;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDutyMotorPos_Init
 * @brief Initializes a position-control duty motor with encoder input.
 * @param p_owner Pointer to the object to initialize.
 * @param id Motor identifier, or `PIF_ID_AUTO` for auto allocation.
 * @param p_timer_manager Timer manager used to allocate timers.
 * @param max_duty Maximum supported duty value.
 * @param period1ms Control task period in milliseconds.
 * @param p_encoder Pulse encoder used for position counting.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifDutyMotorPos_Init(PifDutyMotorPos* p_owner, PifId id, PifTimerManager* p_timer_manager,
		uint16_t max_duty, uint16_t period1ms, PifPulse* p_encoder);

/**
 * @fn pifDutyMotorPos_Clear
 * @brief Releases all resources owned by the position-control motor.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifDutyMotorPos_Clear(PifDutyMotorPos* p_owner);

/**
 * @fn pifDutyMotorPos_AddStages
 * @brief Registers stage table for position-control operation.
 * @param p_owner Pointer to the position-control motor instance.
 * @param stage_size Number of entries in `p_stages`.
 * @param p_stages Pointer to a valid stage array.
 * @return `TRUE` if all stages are valid and stored, otherwise `FALSE`.
 */
BOOL pifDutyMotorPos_AddStages(PifDutyMotorPos* p_owner, uint8_t stage_size, const PifDutyMotorPosStage* p_stages);

/**
 * @fn pifDutyMotorPos_Start
 * @brief Starts operation using the selected position stage profile.
 * @param p_owner Pointer to the position-control motor instance.
 * @param stage_index Index of the stage profile to run.
 * @param operating_time Runtime limit when time-based stop mode is selected.
 * @return `TRUE` when startup succeeds, otherwise `FALSE`.
 */
BOOL pifDutyMotorPos_Start(PifDutyMotorPos* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorPos_Stop
 * @brief Requests stop sequence according to the active stage settings.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifDutyMotorPos_Stop(PifDutyMotorPos* p_owner);

/**
 * @fn pifDutyMotorPos_GetCurrentPulse
 * @brief Returns the current encoder pulse count.
 * @param p_owner Pointer to the position-control motor instance.
 * @return Current falling-edge pulse count.
 */
uint32_t pifDutyMotorPos_GetCurrentPulse(PifDutyMotorPos* p_owner);

/**
 * @fn pifDutyMotorPos_Emergency
 * @brief Forces immediate emergency deceleration/break state.
 * @param p_owner Pointer to the position-control motor instance.
 */
void pifDutyMotorPos_Emergency(PifDutyMotorPos* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DUTY_MOTOR_POS_H
