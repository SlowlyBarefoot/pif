#ifndef PIF_TIMER_MANAGER_H
#define PIF_TIMER_MANAGER_H


#include "core/pif_obj_array.h"
#include "core/pif_task_manager.h"
#include "core/pif_timer.h"


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
    PifObjArray __timers;
	PifTask *__p_task;
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
BOOL pifTimerManager_Init(PifTimerManager *p_manager, PifId id, uint32_t period1us, int max_count);

/**
 * @fn pifTimerManager_Clear
 * @brief 모든 Timer용 메모리를 반환한다.
 * @param p_manager Timer manager 자신
 */
void pifTimerManager_Clear(PifTimerManager *p_manager);

/**
 * @fn pifTimerManager_Add
 * @brief Timer를 추가한다.
 * @param p_manager Timer manager 자신
 * @param type Timer의 종류
 * @return Timer 구조체 포인터를 반환한다.
 */
PifTimer *pifTimerManager_Add(PifTimerManager *p_manager, PifTimerType type);

/**
 * @fn pifTimerManager_Remove
 * @brief Timer를 삭제한다.
 * @param p_timer Timer 포인터
 */
void pifTimerManager_Remove(PifTimer *p_timer);

/**
 * @fn pifTimerManager_Count
 * @brief Timer 갯수를 구한다.
 * @param p_manager Timer manager 자신
 * @return Timer 갯수를 반환한다.
 */
int pifTimerManager_Count(PifTimerManager *p_manager);

/**
 * @fn pifTimerManager_sigTick
 * @brief Timer를 발생하는 Interrupt 함수에서 호출한다.
 * @param p_manager Timer manager 자신
 */
void pifTimerManager_sigTick(PifTimerManager *p_manager);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_MANAGER_H
