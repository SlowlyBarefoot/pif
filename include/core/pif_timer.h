#ifndef PIF_TIMER_H
#define PIF_TIMER_H


#include "core/pif.h"


#ifndef PIF_PWM_MAX_DUTY
#define PIF_PWM_MAX_DUTY				1000
#endif


typedef void (*PifActTimerPwm)(SWITCH value);

typedef void (*PifEvtTimerFinish)(PifIssuerP p_issuer);


typedef enum EnPifTimerType
{
    TT_ONCE			= 0,
    TT_REPEAT		= 1,
	TT_PWM			= 2
} PifTimerType;

typedef enum EnPifTimerStep
{
    TS_STOP			= 0,
    TS_RUNNING		= 1,
    TS_REMOVE		= 2
} PifTimerStep;


/**
 * @struct StPifTimer
 * @brief 한 Timer내에 항목을 관리하는 구조체
 */
typedef struct StPifTimer
{
	// Public Member Variable
    uint32_t target;

    // Public Action Function
    PifActTimerPwm act_pwm;

	// Read-only Member Variable
    PifTimerType _type;
    PifTimerStep _step;

	// Private Member Variable
    uint32_t __current;
    PifIssuerP __p_finish_issuer;
    uint32_t __pwm_duty;
    BOOL __event_into_int;
	BOOL __event;

    // Private Event Function
    PifEvtTimerFinish __evt_finish;
} PifTimer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTimer_Start
 * @brief Timer를 시작한다.
 * @param p_owner Timer 포인터
 * @param target 이동 pulse 수
 * @return
 */
BOOL pifTimer_Start(PifTimer* p_owner, uint32_t pulse);

/**
 * @fn pifTimer_Stop
 * @brief Timer를 종료한다.
 * @param p_owner Timer 포인터
 */
void pifTimer_Stop(PifTimer* p_owner);

/**
 * @fn pifTimer_Reset
 * @brief Timer를 다시 시작한다.
 * @param p_owner Timer 포인터
 */
void pifTimer_Reset(PifTimer* p_owner);

/**
 * @fn pifTimer_SetPwmDuty
 * @brief Timer의 이동 pulse를 재설정한다.
 * @param p_owner Timer 포인터
 * @param duty 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifTimer_SetPwmDuty(PifTimer* p_owner, uint16_t duty);

/**
 * @fn pifTimer_Remain
 * @brief Timer의 남은 pulse 수를 얻는다.
 * @param p_owner Timer 포인터
 * @return 남은 pulse 수를 반환한다.
 */
uint32_t pifTimer_Remain(PifTimer* p_owner);

/**
 * @fn pifTimer_Elapsed
 * @brief Timer의 경과한 pulse 수를 얻는다.
 * @param p_owner Timer 포인터
 * @return 경과한 pulse 수를 반환한다.
 */
uint32_t pifTimer_Elapsed(PifTimer* p_owner);

/**
 * @fn pifTimer_AttachEvtFinish
 * @brief 이동 완료후 task에서 발생시킬 이벤트를 연결한다.
 * @param p_owner Timer 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer);

/**
 * @fn pifTimer_AttachEvtIntFinish
 * @brief 이동 완료후 interrupt에서 즉시 발생시킬 이벤트를 연결한다. 단, 이벤트의 실행코드는 최소화하여야 한다.
 * @param p_owner Timer 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifTimer_AttachEvtIntFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_H
