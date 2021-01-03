#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "pifPulse.h"
#include "pifMotor.h"


struct _PIF_stDutyMotor;
typedef struct _PIF_stDutyMotor PIF_stDutyMotor;

typedef void (*PIF_actDutyMotorSetDuty)(uint16_t usDuty);
typedef void (*PIF_actDutyMotorSetDirection)(uint8_t ucDir);
typedef void (*PIF_actDutyMotorOperateBreak)(uint8_t ucState);

typedef void (*PIF_evtDutyMotorStable)(PIF_stDutyMotor *pstOwner, void *pvInfo);
typedef void (*PIF_evtDutyMotorStop)(PIF_stDutyMotor *pstOwner, void *pvInfo);
typedef void (*PIF_evtDutyMotorError)(PIF_stDutyMotor *pstOwner, void *pvInfo);


/**
 * @class _PIF_stDutyMotor
 * @brief
 */
struct _PIF_stDutyMotor
{
    PIF_unDeviceCode unDeviceCode;
	uint16_t usMaxDuty;
	uint16_t usCurrentDuty;
	uint8_t ucDirection;
    PIF_enMotorState enState;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifDutyMotor_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifDutyMotor_Exit();

PIF_stDutyMotor *pifDutyMotor_Add(PIF_unDeviceCode unDeviceCode, uint16_t usMaxDuty);

void pifDutyMotor_AttachAction(PIF_stDutyMotor *pstOwner, PIF_actDutyMotorSetDuty actSetDuty, PIF_actDutyMotorSetDirection actSetDirection,
		PIF_actDutyMotorOperateBreak actOperateBreak);
void pifDutyMotor_AttachEvent(PIF_stDutyMotor *pstOwner, PIF_evtDutyMotorStable evtStable, PIF_evtDutyMotorStop evtStop,
		PIF_evtDutyMotorError evtError);

void pifDutyMotor_SetDirection(PIF_stDutyMotor *pstOwner, uint8_t ucDirection);
void pifDutyMotor_SetDuty(PIF_stDutyMotor *pstOwner, uint16_t usDuty);

BOOL pifDutyMotor_Start(PIF_stDutyMotor *pstOwner, uint16_t usDuty);
void pifDutyMotor_BreakRelease(PIF_stDutyMotor *pstOwner, uint16_t usBreakTime);

BOOL pifDutyMotor_InitControl(PIF_stDutyMotor *pstOwner, uint16_t usControlPeriod);
BOOL pifDutyMotor_StartControl(PIF_stDutyMotor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
