#ifndef PIF_STEP_MOTOR_POS_H
#define PIF_STEP_MOTOR_POS_H


#include "pifStepMotor.h"
#include "pifSensor.h"


/**
 * @class _PIF_stStepMotorPosStage
 * @brief
 */
typedef struct _PIF_stStepMotorPosStage
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
	uint16_t usFsHighPps;		// 정해진 P/S
	uint32_t unFsPulseCount;	// 정속 구간까지의 이동 pulse

	// 감속 구간 (Reduce speed range)
    uint16_t usRsLowPps;		// 정위치에 정지하기 위한 저속 P/S
	uint16_t usRsCtrlPps;		// 감속 P/S 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t usRsBreakTime;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.

	uint32_t unTotalPulse;		// 총 이동 pulse
} PIF_stStepMotorPosStage;

/**
 * @class _PIF_stStepMotorPos
 * @brief
 */
typedef struct _PIF_stStepMotorPos
{
	PIF_stStepMotor parent;

	// Public Member Variable

	// Read-only Member Variable
    uint8_t _ucStageIndex;

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stStepMotorPosStage *__pstStages;
    const PIF_stStepMotorPosStage *__pstCurrentStage;
} PIF_stStepMotorPos;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stStepMotor *pifStepMotorPos_Create(PifId usPifId, PIF_stPulse* p_timer, uint8_t ucResolution,
		PIF_enStepMotorOperation enOperation, uint16_t usControlPeriodMs);
void pifStepMotorPos_Destroy(PIF_stStepMotor** pp_parent);

BOOL pifStepMotorPos_AddStages(PIF_stStepMotor *pstOwner, uint8_t ucStageSize, const PIF_stStepMotorPosStage *pstStages);

BOOL pifStepMotorPos_Start(PIF_stStepMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifStepMotorPos_Stop(PIF_stStepMotor *pstOwner);
void pifStepMotorPos_Emergency(PIF_stStepMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_POS_H
