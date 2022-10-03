#ifndef PIF_DUTY_MOTOR_POS_H
#define PIF_DUTY_MOTOR_POS_H


#include "core/pif_pulse.h"
#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


/**
 * @class StPifDutyMotorPosStage
 * @brief
 */
typedef struct StPifDutyMotorPosStage
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
	uint32_t fs_pulse_count;	// 정속 구간까지의 이동 pulse

	// 감속 구간 (Reduce speed range)
    uint16_t rs_low_duty;		// 저속 구간의 duty
	uint16_t rs_ctrl_duty;		// 감속 duty 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t rs_break_time;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.

	uint32_t total_pulse;		// 총 이동 pulse
} PifDutyMotorPosStage;

/**
 * @class StPifDutyMotorPos
 * @brief
 */
typedef struct StPifDutyMotorPos
{
	PifDutyMotor parent;

	// Public Member Variable

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
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param max_duty
 * @param period1ms
 * @param p_encoder
 * @return 
 */
BOOL pifDutyMotorPos_Init(PifDutyMotorPos* p_owner, PifId id, PifTimerManager* p_timer_manager,
		uint16_t max_duty, uint16_t period1ms, PifPulse* p_encoder);

/**
 * @fn pifDutyMotorPos_Clear
 * @brief
 * @param p_owner
 */
void pifDutyMotorPos_Clear(PifDutyMotorPos* p_owner);

/**
 * @fn pifDutyMotorPos_AddStages
 * @brief 
 * @param p_owner
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifDutyMotorPos_AddStages(PifDutyMotorPos* p_owner, uint8_t stage_size, const PifDutyMotorPosStage* p_stages);

/**
 * @fn pifDutyMotorPos_Start
 * @brief 
 * @param p_owner
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifDutyMotorPos_Start(PifDutyMotorPos* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorPos_Stop
 * @brief 
 * @param p_owner
 */
void pifDutyMotorPos_Stop(PifDutyMotorPos* p_owner);

/**
 * @fn pifDutyMotorPos_GetCurrentPulse
 * @brief
 * @param p_owner
 */
#ifdef __PIF_NO_USE_INLINE__
	uint32_t pifDutyMotorPos_GetCurrentPulse(PifDutyMotorPos* p_owner);
#else
	inline uint32_t pifDutyMotorPos_GetCurrentPulse(PifDutyMotorPos* p_owner) { return p_owner->__p_encoder->falling_count; }
#endif

/**
 * @fn pifDutyMotorPos_Emergency
 * @brief
 * @param p_owner
 */
void pifDutyMotorPos_Emergency(PifDutyMotorPos* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DUTY_MOTOR_POS_H
