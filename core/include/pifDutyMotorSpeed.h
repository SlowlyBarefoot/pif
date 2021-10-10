#ifndef PIF_DUTY_MOTOR_SPEED_H
#define PIF_DUTY_MOTOR_SPEED_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


/**
 * @class _PIF_stDutyMotorSpeedStage
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeedStage
{
	uint8_t enMode;				// PIF_enMotorMode

    // Sensor
    PIF_stSensor **ppstStartSensor;
    PIF_stSensor **ppstReduceSensor;
    PIF_stSensor **ppstStopSensor;

	// 가속 구간 (Gained speed range)
	uint16_t usGsStartDuty;		// 초기 기동 duty 설정
	uint16_t usGsCtrlDuty;		// 가속 duty 설정. 이 값이 0인 경우 가속 구간을 생략함.

	// 정속 구간 (Fixed speed range)
	uint16_t usFsHighDuty;		// 정해진 duty
    uint16_t usFsOverTime;		// 정지 신호를 받은 후 감속할 때까지의 시간

	// 감속 구간 (Reduce speed range)
    uint16_t usRsLowDuty;		// 브레이크 잡을 duty
	uint16_t usRsCtrlDuty;		// 감속 duty 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t usRsBreakTime;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PIF_stDutyMotorSpeedStage;

/**
 * @class _PIF_stDutyMotorSpeed
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeed
{
	PIF_stDutyMotor parent;

	// Public Member Variable

	// Read-only Member Variable
    uint8_t _ucStageIndex;

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stDutyMotorSpeedStage *__pstStages;
    const PIF_stDutyMotorSpeedStage *__pstCurrentStage;
} PIF_stDutyMotorSpeed;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stDutyMotor *pifDutyMotorSpeed_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod);
void pifDutyMotorSpeed_Destroy(PIF_stDutyMotor** pp_owner);

BOOL pifDutyMotorSpeed_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedStage *pstStages);

BOOL pifDutyMotorSpeed_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifDutyMotorSpeed_Stop(PIF_stDutyMotor *pstOwner);
void pifDutyMotorSpeed_Emergency(PIF_stDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
