#ifndef PIF_DUTY_MOTOR_H
#define PIF_DUTY_MOTOR_H


#include "pif_pulse.h"
#include "pif_motor.h"


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

	PifPulseItem* __p_timer_delay;
	PifPulseItem* __p_timer_break;

    PifTask* __p_task;

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

/**
 * @fn pifDutyMotor_Create
 * @brief 
 * @param id
 * @param p_timer
 * @param max_duty
 * @return 
 */
PifDutyMotor* pifDutyMotor_Create(PifId id, PifPulse* p_timer, uint16_t max_duty);

/**
 * @fn pifDutyMotor_Destroy
 * @brief
 * @param pp_owner
 */
void pifDutyMotor_Destroy(PifDutyMotor** pp_owner);

/**
 * @fn pifDutyMotor_Init
 * @brief 
 * @param p_owner
 * @param id
 * @param p_timer
 * @param max_duty
 * @return 
 */
BOOL pifDutyMotor_Init(PifDutyMotor* p_owner, PifId id, PifPulse* p_timer, uint16_t max_duty);

/**
 * @fn pifDutyMotor_Clear
 * @brief
 * @param p_owner
 */
void pifDutyMotor_Clear(PifDutyMotor* p_owner);

/**
 * @fn pifDutyMotor_AttachAction
 * @brief
 * @param p_owner
 * @param act_set_duty
 * @param act_set_direction
 * @param act_operate_break
 */
void pifDutyMotor_AttachAction(PifDutyMotor* p_owner, PifActDutyMotorSetDuty act_set_duty,
		PifActDutyMotorSetDirection act_set_direction, PifActDutyMotorOperateBreak act_operate_break);

/**
 * @fn pifDutyMotor_SetDirection
 * @brief
 * @param p_owner
 * @param direction
 */
void pifDutyMotor_SetDirection(PifDutyMotor* p_owner, uint8_t direction);

/**
 * @fn pifDutyMotor_SetDuty
 * @brief
 * @param p_owner
 * @param duty
 */
void pifDutyMotor_SetDuty(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @fn pifDutyMotor_SetOperatingTime
 * @brief
 * @param p_owner
 * @param operating_time
 */
BOOL pifDutyMotor_SetOperatingTime(PifDutyMotor* p_owner, uint32_t operating_time);

/**
 * @fn pifDutyMotor_Start
 * @brief
 * @param p_owner
 * @param duty
 * @return
 */
BOOL pifDutyMotor_Start(PifDutyMotor* p_owner, uint16_t duty);

/**
 * @fn pifDutyMotor_BreakRelease
 * @brief
 * @param p_owner
 * @param break_time
 */
void pifDutyMotor_BreakRelease(PifDutyMotor* p_owner, uint16_t break_time);

/**
 * @fn pifDutyMotor_StartControl
 * @brief 
 * @param p_owner
 * @return 
 */
BOOL pifDutyMotor_StartControl(PifDutyMotor* p_owner);

/**
 * @fn pifDutyMotor_StopControl
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifDutyMotor_StopControl(PifDutyMotor* p_owner);

/**
 * @fn pifComm_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifDutyMotor_AttachTask(PifDutyMotor* p_owner, PifTaskMode mode, uint16_t period);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DUTY_MOTOR_H
