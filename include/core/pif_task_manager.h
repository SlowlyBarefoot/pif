#ifndef PIF_TASK_MANAGER_H
#define PIF_TASK_MANAGER_H


#include "core/pif_task.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTaskManager_Init
 * @brief Task용 구조체를 초기화한다.
 * @param max_count 추가할 task의 수를 지정한다..
 * @return
 */
BOOL pifTaskManager_Init(int max_count);

/**
 * @fn pifTaskManager_Clear
 * @brief Task용 메모리를 반환한다.
 */
void pifTaskManager_Clear();

/**
 * @fn pifTaskManager_Add
 * @brief Task를 추가한다.
 * @param id Task의 ID를 설정한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param evt_loop Task 함수
 * @param p_client 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask *pifTaskManager_Add(PifId id, PifTaskMode mode, uint32_t period, PifEvtTaskLoop evt_loop, void *p_client, BOOL start);

/**
 * @fn pifTaskManager_Remove
 * @brief Task를 추가한다.
 * @param p_task Task의 Mode를 설정한다.
 */
void pifTaskManager_Remove(PifTask *p_task);

/**
 * @fn pifTaskManager_Count
 * @brief 등록된 task의 수를 구한다.
 * @return 등록된 task의 수를 반환한다.
 */
int pifTaskManager_Count();

/**
 * @fn pifTaskManager_Count
 * @brief 등록된 task의 수를 구한다.
 * @return 등록된 task의 수를 반환한다.
 */
PifTask *pifTaskManager_CurrentTask();

/**
 * @fn pifTaskManager_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTaskManager_Loop();

/**
 * @fn pifTaskManager_Yield
 * @brief Task내에서 대기중에 다른 Task를 호출하고자 할 경우에 사용하는 함수이다.
 */
void pifTaskManager_Yield();

/**
 * @fn pifTaskManager_YieldMs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 */
void pifTaskManager_YieldMs(uint32_t time);

/**
 * @fn pifTaskManager_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 */
void pifTaskManager_YieldUs(uint32_t time);

/**
 * @fn pifTaskManager_YieldAbort
 * @brief loop내에서 중단 조건 함수가 TRUE를 반환할 때까지 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param p_check_abort
 * @param p_issuer
 */
void pifTaskManager_YieldAbort(PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_YieldAbortMs
 * @brief loop내에서 지정한 시간과 중단 조건 함수가 TRUE를 반환할 때까지 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 * @param p_check_abort
 * @param p_issuer
 */
void pifTaskManager_YieldAbortMs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_YieldAbortUs
 * @brief loop내에서 지정한 시간과 중단 조건 함수가 TRUE를 반환할 때까지 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 * @param p_check_abort
 * @param p_issuer
 */
void pifTaskManager_YieldAbortUs(uint32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_AllTask
 * @brief 
 * @param callback
 */
void pifTaskManager_AllTask(void (*callback)(PifTask *p_task));

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

/**
 * @fn pifTaskManager_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTaskManager_Print();

#endif	// PIF_NO_LOG

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_MANAGER_H
