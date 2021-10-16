#ifndef PIF_DUTY_MOTOR_POS_H
#define PIF_DUTY_MOTOR_POS_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


/**
 * @class _PIF_stDutyMotorPosStage
 * @brief
 */
typedef struct _PIF_stDutyMotorPosStage
{
	uint8_t enMode;				// PIF_enMotorMode

    // Sensor
    PifSensor **ppstStartSensor;
    PifSensor **ppstReduceSensor;
    PifSensor **ppstStopSensor;

	// 가속 구간 (Gained speed range)
	uint16_t usGsStartDuty;		// 초기 기동 duty 설정
	uint16_t usGsCtrlDuty;		// 가속 duty 설정. 이 값이 0인 경우 가속 구간을 생략함.

	// 정속 구간 (Fixed speed range)
	uint16_t usFsHighDuty;		// 정해진 duty
	uint32_t unFsPulseCount;	// 정속 구간까지의 이동 pulse

	// 감속 구간 (Reduce speed range)
    uint16_t usRsLowDuty;		// 저속 구간의 duty
	uint16_t usRsCtrlDuty;		// 감속 duty 설정. 이 값이 0인 경우 감속 구간을 생략함.
    uint16_t usRsBreakTime;		// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.

	uint32_t unTotalPulse;		// 총 이동 pulse
} PIF_stDutyMotorPosStage;

/**
 * @class _PIF_stDutyMotorPos
 * @brief
 */
typedef struct _PIF_stDutyMotorPos
{
	PifDutyMotor parent;

	// Public Member Variable

	// Read-only Member Variable
	uint8_t _ucStageIndex;
	volatile uint32_t _unCurrentPulse;		// 현재까지 이동 pulse

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stDutyMotorPosStage *__pstStages;
    const PIF_stDutyMotorPosStage *__pstCurrentStage;
} PIF_stDutyMotorPos;


#ifdef __cplusplus
extern "C" {
#endif

PifDutyMotor *pifDutyMotorPos_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod);
void pifDutyMotorPos_Destroy(PifDutyMotor** pp_parent);

BOOL pifDutyMotorPos_AddStages(PifDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorPosStage *pstStages);

BOOL pifDutyMotorPos_Start(PifDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifDutyMotorPos_Stop(PifDutyMotor *pstOwner);
void pifDutyMotorPos_Emergency(PifDutyMotor *pstOwner);

// Signal Function
void pifDutyMotorPos_sigEncoder(PifDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_DUTY_MOTOR_POS_H
