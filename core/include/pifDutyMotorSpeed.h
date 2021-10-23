#ifndef PIF_DUTY_MOTOR_SPEED_H
#define PIF_DUTY_MOTOR_SPEED_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


/**
 * @class StPifDutyMotorSpeedStage
 * @brief 
 */
typedef struct StPifDutyMotorSpeedStage
{
	uint8_t mode;				// PifMotorMode

    // Sensor
    PifSensor** pp_start_sensor;
    PifSensor** pp_reduce_sensor;
    PifSensor** pp_stop_sensor;

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
 * @fn pifDutyMotorSpeed_Create
 * @brief 
 * @param id
 * @param p_timer
 * @param max_duty
 * @return 
 */
PifDutyMotor* pifDutyMotorSpeed_Create(PifId id, PifPulse* p_timer, uint16_t max_duty);

/**
 * @fn pifDutyMotorSpeed_Destroy
 * @brief
 * @param pp_parent
 */
void pifDutyMotorSpeed_Destroy(PifDutyMotor** pp_parent);

/**
 * @fn pifDutyMotorSpeed_AddStages
 * @brief 
 * @param p_parent
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifDutyMotorSpeed_AddStages(PifDutyMotor* p_parent, uint8_t stage_size, const PifDutyMotorSpeedStage* p_stages);

/**
 * @fn pifDutyMotorSpeed_Start
 * @brief 
 * @param p_parent
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifDutyMotorSpeed_Start(PifDutyMotor* p_parent, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorSpeed_Stop
 * @brief 
 * @param p_parent
 */
void pifDutyMotorSpeed_Stop(PifDutyMotor* p_parent);

/**
 * @fn pifDutyMotorSpeed_Emergency
 * @brief
 * @param p_parent
 */
void pifDutyMotorSpeed_Emergency(PifDutyMotor* p_parent);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
