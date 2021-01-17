#ifndef PIF_STEP_MOTOR_BASE_H
#define PIF_STEP_MOTOR_BASE_H


#include "pifPulse.h"
#include "pifStepMotor.h"


struct _PIF_stStepMotorBase;
typedef struct _PIF_stStepMotorBase PIF_stStepMotorBase;

typedef void (*PIF_fnStepMotorControl)(PIF_stStepMotorBase *pstBase);
typedef void (*PIF_fnStepMotorStopStep)(PIF_stStepMotorBase *pstBase);


/**
 * @class _PIF_stStepMotorBase
 * @brief
 */
struct _PIF_stStepMotorBase
{
	// Public Member Variable
	PIF_stStepMotor stOwner;

	// Private Member Variable
    void *pvInfo;
    uint8_t ucError;
	uint8_t ucStepSize;
	uint16_t usStepPeriodUs;
	uint16_t usControlPeriodMs;

	PIF_stTask *pstTask;

	PIF_stPulseItem *pstTimerStep;
	PIF_stPulseItem *pstTimerControl;
	PIF_stPulseItem *pstTimerDelay;
	PIF_stPulseItem *pstTimerBreak;

	uint8_t ucCurrentStep;
	uint32_t unTargetPulse;
	const uint16_t *pusPhaseOperation;

	PIF_enTaskLoop enTaskLoop;

    // Public Action Function
    PIF_actStepMotorSetStep actSetStep;

    // Public Event Function
    PIF_evtStepMotorStable evtStable;
    PIF_evtStepMotorStop evtStop;
    PIF_evtStepMotorError evtError;

	// Private Member Function
    PIF_fnStepMotorControl fnControl;
    PIF_fnStepMotorStopStep fnStopStep;
};


extern PIF_stPulse *g_pstStepMotorTimer;
extern uint32_t g_unStepMotorTimerUs;


#endif  // PIF_STEP_MOTOR_BASE_H
