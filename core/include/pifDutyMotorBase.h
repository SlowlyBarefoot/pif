#ifndef PIF_DUTY_MOTOR_BASE_H
#define PIF_DUTY_MOTOR_BASE_H


#include "pifDutyMotor.h"


struct _PIF_stDutyMotorBase;
typedef struct _PIF_stDutyMotorBase PIF_stDutyMotorBase;

typedef void (*PIF_fnDutyMotorControl)(PIF_stDutyMotorBase *pstOwner);


/**
 * @class _PIF_stDutyMotorBase
 * @brief
 */
struct _PIF_stDutyMotorBase
{
	// Public Member Variable
	PIF_stDutyMotor stOwner;

	// Private Member Variable
    void *pvInfo;
	struct {
        uint8_t btError	: 1;
	};
	uint16_t usControlPeriod;

	PIF_stPulseItem *pstTimerControl;
	PIF_stPulseItem *pstTimerDelay;
	PIF_stPulseItem *pstTimerBreak;

    // Private Action Function
    PIF_actDutyMotorSetDuty actSetDuty;
    PIF_actDutyMotorSetDirection actSetDirection;
    PIF_actDutyMotorOperateBreak actOperateBreak;

    // Public Event Function
    PIF_evtDutyMotorStable evtStable;
    PIF_evtDutyMotorStop evtStop;
    PIF_evtDutyMotorError evtError;

	// Private Member Function
    PIF_fnDutyMotorControl fnControl;
};


extern PIF_stPulse *g_pstDutyMotorTimer;


#endif  // PIF_DUTY_MOTOR_BASE_H
