#ifndef PIF_STEP_MOTOR_SPEED_H
#define PIF_STEP_MOTOR_SPEED_H


#include "pifStepMotor.h"
#include "pifSensor.h"


/**
 * @class StPifStepMotorSpeedStage
 * @brief 
 */
typedef struct StPifStepMotorSpeedStage
{
	uint8_t mode;				// PifMotorMode

    // Sensor
    PifSensor** pp_start_sensor;
    PifSensor** pp_reduce_sensor;
    PifSensor** pp_stop_sensor;

	// 가속 구간 (Gained speed range)
	uint16_t gs_start_pps;		// 초기 기동 P/S 설정
	uint16_t gs_ctrl_pps;		// 가속 P/S 설정. 이 값이 0인 경우 가속 구간을 생략함.

	// 정속 구간 (Fixed speed range)
	uint16_t fs_fixed_pps;		// 정해진 P/S
    uint16_t fs_overtime;		// 정지 신호를 받은 후 감속할 때까지의 시간

	// 감속 구간 (Reduce speed range)
    uint16_t rs_stop_pps;		// 브레이크 잡을 P/S
	uint16_t rs_ctrl_pps;		// 감속 P/S 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t rs_break_time;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PifStepMotorSpeedStage;

/**
 * @class StPifStepMotorSpeed
 * @brief 
 */
typedef struct StPifStepMotorSpeed
{
	PifStepMotor parent;

	// Public Member Variable

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

PifStepMotor* pifStepMotorSpeed_Create(PifId id, PifPulse* p_timer, uint8_t resolution, PifStepMotorOperation operation);
void pifStepMotorSpeed_Destroy(PifStepMotor** pp_parent);

BOOL pifStepMotorSpeed_AddStages(PifStepMotor* p_parent, uint8_t stage_size, const PifStepMotorSpeedStage* p_stages);

BOOL pifStepMotorSpeed_Start(PifStepMotor* p_parent, uint8_t stage_index, uint32_t operating_time);
void pifStepMotorSpeed_Stop(PifStepMotor* p_parent);
void pifStepMotorSpeed_Emergency(PifStepMotor* p_parent);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STEP_MOTOR_SPEED_H
