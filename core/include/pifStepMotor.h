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
	SMO_en2P_2W			= 1,	// 2-Phase 2-Wire
	SMO_en2P_4W_1S		= 2,	// 2-Phase 4-Wire : 1 Step
	SMO_en2P_4W_2S		= 3,	// 2-Phase 4-Wire : 2 Step
	SMO_en2P_4W_1_2S	= 4,	// 2-Phase 4-Wire : 1-2 Step
#if 0
	SMO_en5P_PG			= 5		// 5-Phase Pantagon
#endif
} PIF_enStepMotorOperation;


struct _PIF_stStepMotor;
typedef struct _PIF_stStepMotor PIF_stStepMotor;

typedef void (*PIF_actStepMotorSetStep)(uint16_t usPhase);

typedef void (*PIF_evtStepMotorStable)(PIF_stStepMotor *pstOwner);
typedef void (*PIF_evtStepMotorStop)(PIF_stStepMotor *pstOwner);
typedef void (*PIF_evtStepMotorError)(PIF_stStepMotor *pstOwner);

typedef void (*PIF_fnStepMotorControl)(PIF_stStepMotor *pstOwner);
typedef void (*PIF_fnStepMotorStopStep)(PIF_stStepMotor *pstOwner);


/**
 * @class _PIF_stStepMotor
 * @brief
 */
struct _PIF_stStepMotor
{
	// Public Member Variable

    // Public Event Function
    PIF_evtStepMotorStable evtStable;
    PIF_evtStepMotorStop evtStop;
    PIF_evtStepMotorError evtError;

	// Read-only Member Variable
    PifId _usPifId;
    PIF_stPulse *_p_timer;
    PIF_enStepMotorMethod _enMethod;
    PIF_enStepMotorOperation _enOperation;
    uint16_t _usResolution;
	uint16_t _usCurrentPps;
    uint8_t _ucReductionGearRatio;
    uint8_t _ucDirection;
    PIF_enMotorState _enState;
    uint32_t _unCurrentPulse;

	// Private Member Variable
    uint8_t __ucError;
	uint8_t __ucStepSize;
	uint16_t __usStepPeriodUs;
	uint16_t __usControlPeriodMs;

	PifTask *__pstTask;

	PIF_stPulseItem *__pstTimerStep;
	PIF_stPulseItem *__pstTimerControl;
	PIF_stPulseItem *__pstTimerDelay;
	PIF_stPulseItem *__pstTimerBreak;

	uint8_t __ucCurrentStep;
	uint32_t __unTargetPulse;
	const uint16_t *__pusPhaseOperation;

    // Private Action Function
    PIF_actStepMotorSetStep __actSetStep;

	// Private Member Function
    PIF_fnStepMotorControl __fnControl;
    PIF_fnStepMotorStopStep __fnStopStep;
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stStepMotor *pifStepMotor_Create(PifId usPifId, PIF_stPulse* p_timer, uint16_t usResolution, PIF_enStepMotorOperation enOperation);
void pifStepMotor_Destroy(PIF_stStepMotor** pp_owner);

BOOL pifStepMotor_Init(PIF_stStepMotor* p_owner, PifId usPifId, PIF_stPulse* p_timer, uint16_t usResolution, PIF_enStepMotorOperation enOperation);
void pifStepMotor_Clear(PIF_stStepMotor* p_owner);

void pifStepMotor_AttachAction(PIF_stStepMotor *pstOwner, PIF_actStepMotorSetStep actSetStep);

BOOL pifStepMotor_SetDirection(PIF_stStepMotor *pstOwner, uint8_t ucDirection);
BOOL pifStepMotor_SetMethod(PIF_stStepMotor *pstOwner, PIF_enStepMotorMethod enMethod);
BOOL pifStepMotor_SetOperatingTime(PIF_stStepMotor *pstOwner, uint32_t unOperatingTime);
BOOL pifStepMotor_SetOperation(PIF_stStepMotor *pstOwner, PIF_enStepMotorOperation enOperation);
BOOL pifStepMotor_SetPps(PIF_stStepMotor *pstOwner, uint16_t usPps);
BOOL pifStepMotor_SetReductionGearRatio(PIF_stStepMotor *pstOwner, uint8_t ucReductionGearRatio);
BOOL pifStepMotor_SetRpm(PIF_stStepMotor *pstOwner, float fRpm);
float pifStepMotor_GetRpm(PIF_stStepMotor *pstOwner);
BOOL pifStepMotor_SetTargetPulse(PIF_stStepMotor *pstOwner, uint32_t unTargetPulse);

BOOL pifStepMotor_Start(PIF_stStepMotor *pstOwner, uint32_t unTargetPulse);
void pifStepMotor_Break(PIF_stStepMotor *pstOwner);
void pifStepMotor_Release(PIF_stStepMotor *pstOwner);
void pifStepMotor_BreakRelease(PIF_stStepMotor *pstOwner, uint16_t usBreakTime);

BOOL pifStepMotor_InitControl(PIF_stStepMotor *pstOwner, uint16_t usControlPeriodMs);
BOOL pifStepMotor_StartControl(PIF_stStepMotor *pstOwner);

// Task Function
PifTask *pifStepMotor_AttachTask(PIF_stStepMotor *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart);

#ifdef __cplusplus
}
#endif


#endif	// PIF_STEP_MOTOR_H
