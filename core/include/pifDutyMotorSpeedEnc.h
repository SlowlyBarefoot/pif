#ifndef PIF_MOTOR_ENCODER_H
#define PIF_MOTOR_ENCODER_H


#include "pifDutyMotorBase.h"
#include "pifSwitch.h"


#define MAX_STABLE_CNT				(10)


/**
 * @class _PIF_stDutyMotorSpeedEncStage
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeedEncStage
{
	uint8_t enMode;					// PIF_enMotorMode

    // Switch
    PIF_stSwitch **ppstStartSwitch;
    PIF_stSwitch **ppstReduceSwitch;
    PIF_stSwitch **ppstStopSwitch;

	// 가속 구간 (Gained speed range)
	uint8_t ucGsArriveRatio;		// % (0 ~ 100)
	uint16_t usGsStartDuty;
	uint16_t usGsCtrlDuty;
	uint16_t usGsArriveTimeout;

	// 정속 구간 (Fixed speed range)
	float fFsPulsesPerRange;
	uint16_t usFsHighDuty;
    uint16_t usFsOverTime;
	uint16_t usFsStableTimeout;
	uint8_t ucFsStableErrLow;		// % (0 ~ 100)
	uint8_t ucFsStableErrHigh;      // % (0 ~ 100)

	// 감속 구간 (Reduce speed range)
	uint16_t usRsLowDuty;
	uint16_t usRsCtrlDuty;
    uint16_t usRsBreakTime;			// 전자식 또는 기계식 브레이크 사용시 브레이크 잡는 시간 설정.
} PIF_stDutyMotorSpeedEncStage;

/**
 * @class _PIF_stDutyMotorSpeedEncInfo
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeedEncInfo
{
    uint8_t ucStageSize;
    const PIF_stDutyMotorSpeedEncStage *pstStages;
    const PIF_stDutyMotorSpeedEncStage *pstCurrentStage;

    PIF_stPidControl stPidControl;

	uint16_t usArrivePPR;
	uint16_t usErrLowPPR;
	uint16_t usErrHighPPR;
	volatile uint16_t usMeasureEnc;	// pulse
	uint8_t ucEncSampleIdx;
	uint16_t ausEncSample[MAX_STABLE_CNT];
	uint32_t unEncSampleSum;
} PIF_stDutyMotorSpeedEncInfo;


#ifdef __cplusplus
extern "C" {
#endif

PIF_stDutyMotor *pifDutyMotorSpeedEnc_Add(PIF_usId usPifId, uint16_t usMaxDuty, uint16_t usControlPeriod);
BOOL pifDutyMotorSpeedEnc_AddStages(PIF_stDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedEncStage *pstStages);

PIF_stPidControl *pifDutyMotorSpeedEnc_GetPidControl(PIF_stDutyMotor *pstOwner);

BOOL pifDutyMotorSpeedEnc_Start(PIF_stDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifDutyMotorSpeedEnc_Stop(PIF_stDutyMotor *pstOwner);
void pifDutyMotorSpeedEnc_Emergency(PIF_stDutyMotor *pstOwner);

// Signal Function
void pifDutyMotorSpeedEnc_sigEncoder(PIF_stDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
