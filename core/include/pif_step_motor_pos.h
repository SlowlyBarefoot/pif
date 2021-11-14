#ifndef PIF_STEP_MOTOR_POS_H
#define PIF_STEP_MOTOR_POS_H


#include "pif_step_motor.h"
#include "pif_sensor.h"


/**
 * @class StPifStepMotorPosStage
 * @brief
 */
typedef struct StPifStepMotorPosStage
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
	uint16_t fs_high_pps;		// 정해진 P/S
	uint32_t fs_pulse_count;	// 정속 구간까지의 이동 pulse

	// 감속 구간 (Reduce speed range)
    uint16_t rs_low_pps;		// 정위치에 정지하기 위한 저속 P/S
	uint16_t rs_ctrl_pps;		// 감속 P/S 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t rs_break_time;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.

	uint32_t total_pulse;		// 총 이동 pulse
} PifStepMotorPosStage;

/**
 * @class StPifStepMotorPos
 * @brief
 */
typedef struct StPifStepMotorPos
{
	PifStepMotor parent;

	// Public Member Variable

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
 * @fn pifStepMotorPos_Create
 * @brief 
 * @param id
 * @param p_timer_manager
 * @param resolution
 * @param operation
 * @param period1ms
 * @return
 */
PifStepMotor* pifStepMotorPos_Create(PifId id, PifTimerManager* p_timer_manager, uint8_t resolution, PifStepMotorOperation operation, uint16_t period1ms);

/**
 * @fn pifStepMotorPos_Destroy
 * @brief
 * @param pp_parent
 */
void pifStepMotorPos_Destroy(PifStepMotor** pp_parent);

/**
 * @fn pifStepMotorPos_AddStages
 * @brief 
 * @param p_parent
 * @param stage_size
 * @param p_stages
 * @return 
 */
BOOL pifStepMotorPos_AddStages(PifStepMotor* p_parent, uint8_t stage_size, const PifStepMotorPosStage* p_stages);

/**
 * @fn pifStepMotorPos_Start
 * @brief 
 * @param p_parent
 * @param stage_index
 * @param operating_time
 * @return 
 */
BOOL pifStepMotorPos_Start(PifStepMotor* p_parent, uint8_t stage_index, uint32_t operating_time);

/**
 * @fn pifStepMotorPos_Stop
 * @brief 
 * @param p_parent
 */
void pifStepMotorPos_Stop(PifStepMotor* p_parent);

/**
 * @fn pifStepMotorPos_Emergency
 * @brief
 * @param p_parent
 */
void pifStepMotorPos_Emergency(PifStepMotor* p_parent);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_POS_H
