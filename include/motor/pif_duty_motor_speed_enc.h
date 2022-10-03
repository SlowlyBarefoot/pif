#ifndef PIF_MOTOR_ENCODER_H
#define PIF_MOTOR_ENCODER_H


#include "core/pif_pulse.h"
#include "motor/pif_duty_motor.h"
#include "sensor/pif_sensor.h"


#define MAX_STABLE_CNT				(10)


/**
 * @class StPifDutyMotorSpeedEncStage
 * @brief 
 */
typedef struct StPifDutyMotorSpeedEncStage
{
	uint8_t mode;					// PifMotorMode

    // Sensor
    PifSensor* p_start_sensor;
    PifSensor* p_reduce_sensor;
    PifSensor* p_stop_sensor;

	// 가속 구간 (Gained speed range)
	uint8_t gs_arrive_ratio;		// % (0 ~ 100)
	uint16_t gs_start_duty;
	uint16_t gs_ctrl_duty;
	uint16_t gs_arrive_timeout;

	// 정속 구간 (Fixed speed range)
	float fs_pulses_per_period;
	uint16_t fs_high_duty;
    uint16_t fs_overtime;
	uint16_t fs_stable_timeout;
	uint8_t fs_stable_err_low;		// % (0 ~ 100)
	uint8_t fs_stable_err_high;     // % (0 ~ 100)

	// 감속 구간 (Reduce speed range)
	uint16_t rs_low_duty;
	uint16_t rs_ctrl_duty;
    uint16_t rs_break_time;			// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PifDutyMotorSpeedEncStage;

/**
 * @class StPifDutyMotorSpeedEnc
 * @brief 
 */
typedef struct StPifDutyMotorSpeedEnc
{
	PifDutyMotor parent;

	// Public Member Variable

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
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param max_duty
 * @param period1ms
 * @param p_encoder
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_Init(PifDutyMotorSpeedEnc* p_owner, PifId id, PifTimerManager* p_timer_manager,
		uint16_t max_duty, uint16_t period1ms, PifPulse* p_encoder);

/**
 * @fn pifDutyMotorSpeedEnc_Clear
 * @brief
 * @param p_owner
 */
void pifDutyMotorSpeedEnc_Clear(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_AddStages
 * @brief 
 * @param p_owner
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_size, const PifDutyMotorSpeedEncStage* p_stages);

/**
 * @fn pifDutyMotorSpeedEnc_GetPidControl
 * @brief
 * @param p_owner
 * @return
 */
PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_Start
 * @brief 
 * @param p_owner
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotorSpeedEnc* p_owner, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifDutyMotorSpeedEnc_Stop
 * @brief 
 * @param p_owner
 */
void pifDutyMotorSpeedEnc_Stop(PifDutyMotorSpeedEnc* p_owner);

/**
 * @fn pifDutyMotorSpeedEnc_Emergency
 * @brief
 * @param p_owner
 */
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotorSpeedEnc* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
