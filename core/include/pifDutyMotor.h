#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "pifPulse.h"
#include "pifMotor.h"


struct _PIF_stDutyMotor;
typedef struct _PIF_stDutyMotor PIF_stDutyMotor;

typedef void (*PIF_actDutyMotorSetDuty)(uint16_t usDuty);
typedef void (*PIF_actDutyMotorSetDirection)(uint8_t ucDir);
typedef void (*PIF_actDutyMotorOperateBreak)(uint8_t ucState);

typedef void (*PIF_evtDutyMotorStable)(PIF_stDutyMotor *pstOwner);
typedef void (*PIF_evtDutyMotorStop)(PIF_stDutyMotor *pstOwner);
typedef void (*PIF_evtDutyMotorError)(PIF_stDutyMotor *pstOwner);

typedef void (*PIF_fnDutyMotorControl)(PIF_stDutyMotor *pstOwner);


/**
 * @class _PIF_stDutyMotor
 * @brief
 */
struct _PIF_stDutyMotor
{
	// Public Member Variable

    // Public Event Function
    PIF_evtDutyMotorStable evtStable;
    PIF_evtDutyMotorStop evtStop;
    PIF_evtDutyMotorError evtError;

    // Read-only Member Variable
    PIF_usId _usPifId;
    PIF_stPulse* _p_timer;
	uint16_t _usMaxDuty;
	uint16_t _usCurrentDuty;
	uint8_t _ucDirection;
    PIF_enMotorState _enState;

	// Private Member Variable
    uint8_t __ucError;
	uint16_t __usControlPeriod;

	PIF_stPulseItem *__pstTimerControl;
	PIF_stPulseItem *__pstTimerDelay;
	PIF_stPulseItem *__pstTimerBreak;

    // Private Action Function
    PIF_actDutyMotorSetDuty __actSetDuty;
    PIF_actDutyMotorSetDirection __actSetDirection;
    PIF_actDutyMotorOperateBreak __actOperateBreak;

	// Private Member Function
    PIF_fnDutyMotorControl __fnControl;
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stDutyMotor *pifDutyMotor_Create(PIF_usId usPifId, PIF_stPulse* p_timer, uint16_t usMaxDuty);
void pifDutyMotor_Destroy(PIF_stDutyMotor** pp_owner);

BOOL pifDutyMotor_Init(PIF_stDutyMotor* pstOwner, PIF_usId usPifId, PIF_stPulse* p_timer, uint16_t usMaxDuty);
void pifDutyMotor_Clear(PIF_stDutyMotor* p_owner);

void pifDutyMotor_AttachAction(PIF_stDutyMotor *pstOwner, PIF_actDutyMotorSetDuty actSetDuty, PIF_actDutyMotorSetDirection actSetDirection,
		PIF_actDutyMotorOperateBreak actOperateBreak);

void pifDutyMotor_SetDirection(PIF_stDutyMotor *pstOwner, uint8_t ucDirection);
void pifDutyMotor_SetDuty(PIF_stDutyMotor *pstOwner, uint16_t usDuty);
BOOL pifDutyMotor_SetOperatingTime(PIF_stDutyMotor *pstOwner, uint32_t unOperatingTime);

BOOL pifDutyMotor_Start(PIF_stDutyMotor *pstOwner, uint16_t usDuty);
void pifDutyMotor_BreakRelease(PIF_stDutyMotor *pstOwner, uint16_t usBreakTime);

BOOL pifDutyMotor_InitControl(PIF_stDutyMotor *pstOwner, uint16_t usControlPeriod);
BOOL pifDutyMotor_StartControl(PIF_stDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
