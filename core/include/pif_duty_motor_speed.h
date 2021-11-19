#ifndef PIF_DUTY_MOTOR_SPEED_H
#define PIF_DUTY_MOTOR_SPEED_H


#include "pif_duty_motor.h"
#include "pif_sensor.h"


/**
 * @class StPifDutyMotorSpeedStage
 * @brief 
 */
typedef struct StPifDutyMotorSpeedStage
{
	uint8_t mode;				// PifMotorMode

    // Sensor
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;
    PifSensor* p_stop_sensor;

	// 가속 구간 (Gained speed range)
	uint16_t gs_start_duty;		// 초기 기동 duty 설정
	uint16_t gs_ctrl_duty;		// 가속 duty 설정. 이 값이 0인 경우 가속 구간을 생략함.

	// 정속 구간 (Fixed speed range)
	uint16_t fs_high_duty;		// 정해진 duty
    uint16_t fs_overtime;		// 정지 신호를 받은 후 감속할 때까지의 시간

	// 감속 구간 (Reduce speed range)
    uint16_t rs_low_duty;		// 브레이크 잡을 duty
	uint16_t rs_ctrl_duty;		// 감속 duty 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t rs_break_time;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PifDutyMotorSpeedStage;

/**
 * @class StPifDutyMotorSpeed
 * @brief 
 */
typedef struct StPifDutyMotorSpeed
{
	PifDutyMotor parent;

	// Public Member Variable

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
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param max_duty
 * @param period1ms
 * @return 
 */
BOOL pifDutyMotorSpeed_Init(PifDutyMotorSpeed* p_owner, PifId id, PifTimerManager* p_timer_manager, uint16_t max_duty, uint16_t period1ms);

/**
 * @fn pifDutyMotorSpeed_Clear
 * @brief
 * @param p_owner
 */
void pifDutyMotorSpeed_Clear(PifDutyMotorSpeed* p_owner);

/**
 * @fn pifDutyMotorSpeed_AddStages
 * @brief 
 * @param p_owner
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifDutyMotorSpeed_AddStages(PifDutyMotorSpeed* p_owner, uint8_t stage_size, const PifDutyMotorSpeedStage* p_stages);

/**
 * @fn pifDutyMotorSpeed_Start
 * @brief 
 * @param p_owner
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifDutyMotorSpeed_Start(PifDutyMotorSpeed* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorSpeed_Stop
 * @brief 
 * @param p_owner
 */
void pifDutyMotorSpeed_Stop(PifDutyMotorSpeed* p_owner);

/**
 * @fn pifDutyMotorSpeed_Emergency
 * @brief
 * @param p_owner
 */
void pifDutyMotorSpeed_Emergency(PifDutyMotorSpeed* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
