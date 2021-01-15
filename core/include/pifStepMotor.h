#ifndef PIF_STEP_MOTOR_H
#define PIF_STEP_MOTOR_H


#include "pifPulse.h"
#include "pifMotor.h"


typedef enum _PIF_enStepMotorMethod
{
	SMM_enTimer		= 0,
	SMM_enTask		= 1
} PIF_enStepMotorMethod;

typedef enum _PIF_enStepMotorOperation
{
	SMO_en2P_BP			= 1,	// 2-Phase Bipolar
	SMO_en4P_UP_1Phase	= 2,	// 4-Phase Unipolar : 1 Phase
	SMO_en4P_UP_2Phase	= 3,	// 4-Phase Unipolar : 2 Phase
	SMO_en4P_UP_12Phase	= 4,	// 4-Phase Unipolar : 1-2 Phase
	SMO_en5P_PG			= 5		// 5-Phase Pantagon
} PIF_enStepMotorOperation;


struct _PIF_stStepMotor;
typedef struct _PIF_stStepMotor PIF_stStepMotor;

typedef void (*PIF_actStepMotorSetStep)(uint16_t usPhase);

typedef void (*PIF_evtStepMotorStable)(PIF_stStepMotor *pstOwner, void *pvInfo);
typedef void (*PIF_evtStepMotorStop)(PIF_stStepMotor *pstOwner, void *pvInfo);
typedef void (*PIF_evtStepMotorError)(PIF_stStepMotor *pstOwner, void *pvInfo);


/**
 * @class _PIF_stStepMotor
 * @brief
 */
struct _PIF_stStepMotor
{
    PIF_usId usPifId;
    PIF_enStepMotorMethod enMethod;
    PIF_enStepMotorOperation enOperation;
    uint16_t usResolution;
    uint8_t ucReductionGearRatio;
    uint8_t ucDirection;
	float fCurrentRpm;
    PIF_enMotorState enState;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifStepMotor_Init(PIF_stPulse *pstTimer, uint32_t unTimerUnit, uint8_t ucSize);
void pifStepMotor_Exit();

PIF_stStepMotor *pifStepMotor_Add(PIF_usId usPifId, uint16_t usResolution, PIF_enStepMotorOperation enOperation);

void pifStepMotor_AttachAction(PIF_stStepMotor *pstOwner, PIF_actStepMotorSetStep actSetStep);
void pifStepMotor_AttachEvent(PIF_stStepMotor *pstOwner, PIF_evtStepMotorStable evtStable, PIF_evtStepMotorStop evtStop,
		PIF_evtStepMotorError evtError);
void pifStepMotor_AttachTask(PIF_stStepMotor *pstOwner, PIF_stTask *pstTask);

void pifStepMotor_SetDirection(PIF_stStepMotor *pstOwner, uint8_t ucDirection);
BOOL pifStepMotor_SetMethod(PIF_stStepMotor *pstOwner, PIF_enStepMotorMethod enMethod);
BOOL pifStepMotor_SetOperation(PIF_stStepMotor *pstOwner, PIF_enStepMotorOperation enOperation);
BOOL pifStepMotor_SetPps(PIF_stStepMotor *pstOwner, uint16_t usPps);
BOOL pifStepMotor_SetRpm(PIF_stStepMotor *pstOwner, float fRpm);
void pifStepMotor_SetTargetStep(PIF_stStepMotor *pstOwner, uint32_t unTargetStep);

BOOL pifStepMotor_Start(PIF_stStepMotor *pstOwner, uint32_t unTargetStep);
void pifStepMotor_Break(PIF_stStepMotor *pstOwner);
void pifStepMotor_Release(PIF_stStepMotor *pstOwner);
void pifStepMotor_BreakRelease(PIF_stStepMotor *pstOwner, uint16_t usBreakTime);

BOOL pifStepMotor_InitControl(PIF_stStepMotor *pstOwner, uint16_t usControlPeriodMs);
BOOL pifStepMotor_StartControl(PIF_stStepMotor *pstOwner);

// Task Function
void pifStepMotor_taskAll(PIF_stTask *pstTask);
void pifStepMotor_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
