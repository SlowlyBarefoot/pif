#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pif_list.h"
#include "pif_task.h"


typedef void (*PifActPulsePwm)(SWITCH value);

typedef void (*PifEvtPulseFinish)(void* p_issuer);


typedef enum EnPifPulseType
{
    PT_ONCE			= 0,
    PT_REPEAT		= 1,
	PT_PWM			= 2
} PifPulseType;

typedef enum EnPifPulseStep
{
    PS_STOP			= 0,
    PS_RUNNING		= 1,
    PS_REMOVE		= 2
} PifPulseStep;


struct StPifPulse;
typedef struct StPifPulse PifPulse;


/**
 * @struct StPifPulseItem
 * @brief 한 Pulse내에 항목을 관리하는 구조체
 */
typedef struct StPifPulseItem
{
	// Public Member Variable
    uint32_t target;

    // Public Action Function
    PifActPulsePwm act_pwm;

	// Read-only Member Variable
    PifPulseType _type;
    PifPulseStep _step;

	// Private Member Variable
	PifPulse* __p_owner;
    uint32_t __current;
    void* __p_finish_issuer;
    void* __p_int_finish_issuer;
    uint32_t __pwm_duty;
	BOOL __event;

    // Private Event Function
    PifEvtPulseFinish __evt_finish;
    PifEvtPulseFinish __evt_int_finish;
} PifPulseItem;

/**
 * @struct StPifPulse
 * @brief Pulse를 관리하기 위한 구조체
 */
struct StPifPulse
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	uint32_t _period1us;

	// Private Member Variable
    PifFixList __items;
	PifTask* __p_task;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPulse_Create
 * @brief Pulse를 추가한다.
 * @param id
 * @param period1us
 * @return Pulse 구조체 포인터를 반환한다.
 */
PifPulse* pifPulse_Create(PifId id, uint32_t period1us, int max_count);

/**
 * @fn pifPulse_Destroy
 * @brief Pulse용 메모리를 반환한다.
 * @param pp_owner Pulse 자신
 */
void pifPulse_Destroy(PifPulse** pp_owner);

/**
 * @fn pifPulse_Init
 * @brief Pulse를 추가한다.
 * @param p_owner
 * @param id
 * @param period1us
 * @return Pulse 구조체 포인터를 반환한다.
 */
BOOL pifPulse_Init(PifPulse* p_owner, PifId id, uint32_t period1us, int max_count);

/**
 * @fn pifPulse_Clear
 * @brief Pulse용 메모리를 반환한다.
 * @param p_owner Pulse 자신
 */
void pifPulse_Clear(PifPulse* p_owner);

/**
 * @fn pifPulse_AddItem
 * @brief Pulse의 항목을 추가한다.
 * @param p_owner Pulse 자신
 * @param type Pulse의 종류
 * @return Pulse 항목 구조체 포인터를 반환한다.
 */
PifPulseItem* pifPulse_AddItem(PifPulse* p_owner, PifPulseType type);

/**
 * @fn pifPulse_RemoveItem
 * @brief Pulse 항목을 삭제한다.
 * @param p_item Pulse 항목 포인터
 */
void pifPulse_RemoveItem(PifPulseItem* p_item);

/**
 * @fn pifPulse_Count
 * @brief Pulse 항목의 갯수를 구한다.
 * @return Pulse 항목의 갯수를 반환한다.
 */
int pifPulse_Count(PifPulse* p_owner);

/**
 * @fn pifPulse_StartItem
 * @brief Pulse 항목을 시작한다.
 * @param p_item Pulse 항목 포인터
 * @param target 이동 pulse 수
 * @return
 */
BOOL pifPulse_StartItem(PifPulseItem* p_item, uint32_t pulse);

/**
 * @fn pifPulse_StopItem
 * @brief Pulse 항목을 종료한다.
 * @param p_item Pulse 항목 포인터
 */
void pifPulse_StopItem(PifPulseItem* p_item);

/**
 * @fn pifPulse_SetPwmDuty
 * @brief Pulse 항목의 이동 pulse를 재설정한다.
 * @param p_item Pulse 항목 포인터
 * @param duty 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifPulse_SetPwmDuty(PifPulseItem* p_item, uint16_t duty);

/**
 * @fn pifPulse_RemainItem
 * @brief Pulse 항목의 남은 pulse 수를 얻는다.
 * @param p_item Pulse 항목 포인터
 * @return 남은 pulse 수를 반환한다.
 */
uint32_t pifPulse_RemainItem(PifPulseItem* p_item);

/**
 * @fn pifPulse_ElapsedItem
 * @brief Pulse 항목의 경과한 pulse 수를 얻는다.
 * @param p_item Pulse 항목 포인터
 * @return 경과한 pulse 수를 반환한다.
 */
uint32_t pifPulse_ElapsedItem(PifPulseItem* p_item);

/**
 * @fn pifPulse_sigTick
 * @brief Pulse를 발생하는 Interrupt 함수에서 호출한다.
 * @param p_owner Pulse 자신
 */
void pifPulse_sigTick(PifPulse* p_owner);

/**
 * @fn pifPulse_AttachEvtFinish
 * @brief 이동 완료후 task에서 발생시킬 이벤트를 연결한다.
 * @param p_item Pulse 항목 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer);

/**
 * @fn pifPulse_AttachEvtIntFinish
 * @brief 이동 완료후 interrupt에서 즉시 발생시킬 이벤트를 연결한다. 단, 이벤트의 실행코드는 최소화하여야 한다.
 * @param p_item Pulse 항목 포인터
 * @param evt_finish 연결시킬 이벤트
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtIntFinish(PifPulseItem* p_item, PifEvtPulseFinish evt_finish, void* p_issuer);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
