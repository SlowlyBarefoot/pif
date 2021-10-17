#ifndef PIF_MOTOR_ENCODER_H
#define PIF_MOTOR_ENCODER_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


#define MAX_STABLE_CNT				(10)


/**
 * @class StPifDutyMotorSpeedEncStage
 * @brief 
 */
typedef struct StPifDutyMotorSpeedEncStage
{
	uint8_t mode;					// PifMotorMode

    // Sensor
    PifSensor** pp_start_sensor;
    PifSensor** pp_reduce_sensor;
    PifSensor** pp_stop_sensor;

	// 가속 구간 (Gained speed range)
	uint8_t gs_arrive_ratio;		// % (0 ~ 100)
	uint16_t gs_start_duty;
	uint16_t gs_ctrl_duty;
	uint16_t gs_arrive_timeout;

	// 정속 구간 (Fixed speed range)
	float fs_pulses_per_range;
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

    PifPidControl __pid_control;

	uint16_t __arrive_ppr;
	uint16_t __err_low_ppr;
	uint16_t __err_high_ppr;
	volatile uint16_t __measure_enc;	// pulse
	uint8_t __enc_sample_idx;
	uint16_t __enc_sample[MAX_STABLE_CNT];
	uint32_t __enc_sample_sum;
} PifDutyMotorSpeedEnc;


#ifdef __cplusplus
extern "C" {
#endif

PifDutyMotor* pifDutyMotorSpeedEnc_Create(PifId id, PifPulse* p_timer, uint16_t max_duty, uint16_t control_period);
void pifDutyMotorSpeedEnc_Destroy(PifDutyMotor** pp_parent);

BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotor* p_parent, uint8_t stage_size, const PifDutyMotorSpeedEncStage* p_stages);

PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotor* p_parent);

BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotor* p_parent, uint8_t stage_index, uint32_t operating_time);
void pifDutyMotorSpeedEnc_Stop(PifDutyMotor* p_parent);
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotor* p_parent);

// Signal Function
void pifDutyMotorSpeedEnc_sigEncoder(PifDutyMotor* p_parent);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
