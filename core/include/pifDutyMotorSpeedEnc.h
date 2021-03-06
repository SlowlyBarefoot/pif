#ifndef PIF_MOTOR_ENCODER_H
#define PIF_MOTOR_ENCODER_H


#include "pifDutyMotor.h"
#include "pifSensor.h"


#define MAX_STABLE_CNT				(10)


/**
 * @class _PIF_stDutyMotorSpeedEncStage
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeedEncStage
{
	uint8_t enMode;					// PIF_enMotorMode

    // Sensor
    PIF_stSensor **ppstStartSensor;
    PIF_stSensor **ppstReduceSensor;
    PIF_stSensor **ppstStopSensor;

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
 * @class _PIF_stDutyMotorSpeedEnc
 * @brief 
 */
typedef struct _PIF_stDutyMotorSpeedEnc
{
	// Public Member Variable

	// Read-only Member Variable
    uint8_t _ucStageIndex;

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stDutyMotorSpeedEncStage *__pstStages;
    const PIF_stDutyMotorSpeedEncStage *__pstCurrentStage;

    PIF_stPidControl __stPidControl;

	uint16_t __usArrivePPR;
	uint16_t __usErrLowPPR;
	uint16_t __usErrHighPPR;
	volatile uint16_t __usMeasureEnc;	// pulse
	uint8_t __ucEncSampleIdx;
	uint16_t __ausEncSample[MAX_STABLE_CNT];
	uint32_t __unEncSampleSum;
} PIF_stDutyMotorSpeedEnc;


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
