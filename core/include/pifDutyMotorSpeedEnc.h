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
    PifSensor **ppstStartSensor;
    PifSensor **ppstReduceSensor;
    PifSensor **ppstStopSensor;

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
	PifDutyMotor parent;

	// Public Member Variable

	// Read-only Member Variable
    uint8_t _ucStageIndex;

	// Private Member Variable
    uint8_t __ucStageSize;
    const PIF_stDutyMotorSpeedEncStage *__pstStages;
    const PIF_stDutyMotorSpeedEncStage *__pstCurrentStage;

    PifPidControl __stPidControl;

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

PifDutyMotor *pifDutyMotorSpeedEnc_Create(PifId usPifId, PifPulse* p_timer, uint16_t usMaxDuty, uint16_t usControlPeriod);
void pifDutyMotorSpeedEnc_Destroy(PifDutyMotor** pp_owner);

BOOL pifDutyMotorSpeedEnc_AddStages(PifDutyMotor *pstOwner, uint8_t ucStageSize, const PIF_stDutyMotorSpeedEncStage *pstStages);

PifPidControl *pifDutyMotorSpeedEnc_GetPidControl(PifDutyMotor *pstOwner);

BOOL pifDutyMotorSpeedEnc_Start(PifDutyMotor *pstOwner, uint8_t ucStageIndex, uint32_t unOperatingTime);
void pifDutyMotorSpeedEnc_Stop(PifDutyMotor *pstOwner);
void pifDutyMotorSpeedEnc_Emergency(PifDutyMotor *pstOwner);

// Signal Function
void pifDutyMotorSpeedEnc_sigEncoder(PifDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTOR_ENCODER_H
