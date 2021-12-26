#ifndef PIF_TIMER_H
#define PIF_TIMER_H


#include "pif_list.h"
#include "pif_task.h"


typedef void (*PifActTimerPwm)(SWITCH value);

typedef void (*PifEvtTimerFinish)(void* p_issuer);


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
    void* __p_finish_issuer;
    void* __p_int_finish_issuer;
    uint32_t __pwm_duty;
	BOOL __event;

    // Private Event Function
    PifEvtTimerFinish __evt_finish;
    PifEvtTimerFinish __evt_int_finish;
} PifTimer;

/**
 * @struct StPifTimerManager
 * @brief Timer를 관리하기 위한 구조체
 */
typedef struct StPifTimerManager
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	uint32_t _period1us;

	// Private Member Variable
    PifFixList __timers;
	PifTask* __p_task;
} PifTimerManager;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTimerManager_Init
 * @brief Timer manager를 추가한다.
 * @param p_manager
 * @param id
 * @param period1us
 * @return 성공 여부를 반환한다.
 */
BOOL pifTimerManager_Init(PifTimerManager* p_manager, PifId id, uint32_t period1us, int max_count);

/**
 * @fn pifTimerManager_Clear
 * @brief 모든 Timer용 메모리를 반환한다.
 * @param p_manager Timer manager 자신
 */
void pifTimerManager_Clear(PifTimerManager* p_manager);

/**
 * @fn pifTimerManager_Add
 * @brief Timer를 추가한다.
 * @param p_manager Timer manager 자신
 * @param type Timer의 종류
 * @return Timer 구조체 포인터를 반환한다.
 */
PifTimer* pifTimerManager_Add(PifTimerManager* p_manager, PifTimerType type);

/**
 * @fn pifTimerManager_Remove
 * @brief Timer를 삭제한다.
 * @param p_timer Timer 포인터
 */
void pifTimerManager_Remove(PifTimer* p_timer);

/**
 * @fn pifTimerManager_Count
 * @brief Timer 갯수를 구한다.
 * @param p_manager Timer manager 자신
 * @return Timer 갯수를 반환한다.
 */
int pifTimerManager_Count(PifTimerManager* p_manager);

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
 * @fn pifTimerManager_sigTick
 * @brief Timer를 발생하는 Interrupt 함수에서 호출한다.
 * @param p_manager Timer manager 자신
 */
void pifTimerManager_sigTick(PifTimerManager* p_manager);

/**
 * @fn pifTimer_AttachEvtFinish
 * @brief 이동 완료후 task에서 발생시킬 이벤트를 연결한다.
 * @param p_owner Timer 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, void* p_issuer);

/**
 * @fn pifTimer_AttachEvtIntFinish
 * @brief 이동 완료후 interrupt에서 즉시 발생시킬 이벤트를 연결한다. 단, 이벤트의 실행코드는 최소화하여야 한다.
 * @param p_owner Timer 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifTimer_AttachEvtIntFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, void* p_issuer);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_H
