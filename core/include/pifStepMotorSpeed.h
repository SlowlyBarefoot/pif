#ifndef PIF_STEP_MOTOR_SPEED_H
#define PIF_STEP_MOTOR_SPEED_H


#include "pifStepMotor.h"
#include "pifSensor.h"


/**
 * @class _PIF_stStepMotorSpeedStage
 * @brief 
 */
typedef struct _PIF_stStepMotorSpeedStage
{
	uint8_t enMode;				// PIF_enMotorMode

    // Sensor
    PIF_stSensor **ppstStartSensor;
    PIF_stSensor **ppstReduceSensor;
    PIF_stSensor **ppstStopSensor;

	// 가속 구간 (Gained speed range)
	uint16_t usGsStartPps;		// 초기 기동 P/S 설정
	uint16_t usGsCtrlPps;		// 가속 P/S 설정. 이 값이 0인 경우 가속 구간을 생략함.

	// 정속 구간 (Fixed speed range)
	uint16_t usFsFixedPps;		// 정해진 P/S
    uint16_t usFsOverTime;		// 정지 신호를 받은 후 감속할 때까지의 시간

	// 감속 구간 (Reduce speed range)
    uint16_t usRsStopPps;		// 브레이크 잡을 P/S
	uint16_t usRsCtrlPps;		// 감속 P/S 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t usRsBreakTime;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PIF_stStepMotorSpeedStage;

/**
 * @class _PIF_stStepMotorSpeed
 * @brief 
 */
typedef struct _PIF_stStepMotorSpeed
{
	PIF_stStepMotor parent;

	// Public Member Variable

	// Read-only Member Variable
    uint8_t _ucStageIndex;

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stStepMotorSpeedStage *__pstStages;
    const PIF_stStepMotorSpeedStage *__pstCurrentStage;
} PIF_stStepMotorSpeed;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stStepMotor *pifStepMotorSpeed_Create(PifId usPifId, PIF_stPulse* p_timer, uint8_t ucResolution,
		PIF_enStepMotorOperation enOperation, uint16_t usControlPeriodMs);
void pifStepMotorSpeed_Destroy(PIF_stStepMotor** pp_parent);

BOOL pifStepMotorSpeed_AddStages(PIF_stStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorSpeedStage *pstStages);

BOOL pifStepMotorSpeed_Start(PIF_stStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifStepMotorSpeed_Stop(PIF_stStepMotor *pstOwner);
void pifStepMotorSpeed_Emergency(PIF_stStepMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STEP_MOTOR_SPEED_H
