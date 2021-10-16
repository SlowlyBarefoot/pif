#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "pifPulse.h"
#include "pifMotor.h"


struct StPifDutyMotor;
typedef struct StPifDutyMotor PifDutyMotor;

typedef void (*PifActDutyMotorSetDuty)(uint16_t duty);
typedef void (*PifActDutyMotorSetDirection)(uint8_t dir);
typedef void (*PifActDutyMotorOperateBreak)(uint8_t state);

typedef void (*PifEvtDutyMotorStable)(PifDutyMotor* p_owner);
typedef void (*PifEvtDutyMotorStop)(PifDutyMotor* p_owner);
typedef void (*PifEvtDutyMotorError)(PifDutyMotor* p_owner);

typedef void (*PifDutyMotorControl)(PifDutyMotor* p_owner);


/**
 * @class StPifDutyMotor
 * @brief
 */
struct StPifDutyMotor
{
	// Public Member Variable

    // Public Event Function
    PifEvtDutyMotorStable evt_stable;
    PifEvtDutyMotorStop evt_stop;
    PifEvtDutyMotorError evt_error;

    // Read-only Member Variable
    PifId _id;
    PifPulse* _p_timer;
	uint16_t _max_duty;
	uint16_t _current_duty;
	uint8_t _direction;
    PifMotorState _state;

	// Private Member Variable
    uint8_t __error;
	uint16_t __control_period;

	PifPulseItem* __p_timer_control;
	PifPulseItem* __p_timer_delay;
	PifPulseItem* __p_timer_break;

    // Private Action Function
    PifActDutyMotorSetDuty __act_set_duty;
    PifActDutyMotorSetDirection __act_set_direction;
    PifActDutyMotorOperateBreak __act_operate_break;

	// Private Member Function
    PifDutyMotorControl __control;
};


#ifdef __cplusplus
extern "C" {
#endif

PifDutyMotor* pifDutyMotor_Create(PifId id, PifPulse* p_timer, uint16_t max_duty);
void pifDutyMotor_Destroy(PifDutyMotor** pp_owner);

BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifPulse* p_timer, uint16_t max_duty);
void pifDutyMotor_Clear(PifDutyMotor* p_owner);

void pifDutyMotor_AttachAction(PifDutyMotor* p_owner, PifActDutyMotorSetDuty act_set_duty,
		PifActDutyMotorSetDirection act_set_direction, PifActDutyMotorOperateBreak act_operate_break);

void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction);
void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty);
BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time);

BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty);
void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time);

BOOL pifDutyMotor_InitControl(PifDutyMotor* p_owner, uint16_t control_period);
BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
