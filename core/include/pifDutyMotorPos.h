#ifndef PIF_DUTY_MOTOR_POS_H
#define PIF_DUTY_MOTOR_POS_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


/**
 * @class StPifDutyMotorPosStage
 * @brief
 */
typedef struct StPifDutyMotorPosStage
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
	volatile uint32_t _current_pulse;		// 현재까지 이동 pulse

	// Private Member Variable
    uint8_t __stage_size;
    const PifDutyMotorPosStage* __p_stages;
    const PifDutyMotorPosStage* __p_current_stage;
} PifDutyMotorPos;


#ifdef __cplusplus
extern "C" {
#endif

PifDutyMotor* pifDutyMotorPos_Create(PifId id, PifPulse* p_timer, uint16_t max_duty);
void pifDutyMotorPos_Destroy(PifDutyMotor** pp_parent);

BOOL pifDutyMotorPos_AddStages(PifDutyMotor* p_parent, uint8_t stage_size, const PifDutyMotorPosStage* p_stages);

BOOL pifDutyMotorPos_Start(PifDutyMotor* p_parent, uint8_t stage_index, uint32_t operating_time);
void pifDutyMotorPos_Stop(PifDutyMotor* p_parent);
void pifDutyMotorPos_Emergency(PifDutyMotor* p_parent);

// Signal Function
void pifDutyMotorPos_sigEncoder(PifDutyMotor* p_parent);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DUTY_MOTOR_POS_H
