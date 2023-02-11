#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "core/pif.h"


#define DISALLOW_YIELD_ID_NONE		0


typedef enum EnPifTaskMode
{
	TM_RATIO,
	TM_ALWAYS,
	TM_PERIOD_MS,
	TM_PERIOD_US,
	TM_CHANGE_MS,
	TM_CHANGE_US,
	TM_EXTERNAL_CUTIN,	// After an external trigger occurs, it is executed as soon as the currently running task ends. However, this mode is set to only one task per program.
	TM_EXTERNAL_ORDER,
	TM_TIMER,			// Do not use it for other purposes because it is a mode used by the timer.
	TM_IDLE_MS			// If at least one TM_ALWAYS task exists, the TM_IDLE task is not executed.
} PifTaskMode;


struct StPifTask;
typedef struct StPifTask PifTask;

typedef uint16_t (*PifEvtTaskLoop)(PifTask* p_task);
typedef void (*PifActTaskSignal)(BOOL state);

typedef PifTask* (*PifTaskProcessing)(PifTask* p_owner);


/**
 * @struct StPifTask
 * @brief Task를 관리하는 구조체
 */
struct StPifTask
{
	// Public Member Variable
	BOOL pause;
	uint8_t disallow_yield_id;		// 0 : 모두 허용, 1->255 : 해당 id는 허용하지 않음.

	// Read-only Member Variable
	PifId _id;
	PifTaskMode _mode;
	BOOL _running;
	uint16_t _period;
	void *_p_client;
    uint32_t _max_execution_time;
    uint32_t _total_execution_time;		// total time consumed by task since boot
	uint32_t _execution_count;
	uint32_t _total_period_time;
	uint32_t _period_count;
	uint32_t _max_trigger_delay;
	uint32_t _total_trigger_delay;
	uint16_t _trigger_delay;

	// Private Member Variable
	PifTaskProcessing __processing;
	volatile BOOL __trigger;
	int __table_number;
	uint16_t __delay_ms;
	uint32_t __pretime;
	uint32_t __last_execute_time;
	volatile uint32_t __trigger_time;
#ifdef __PIF_DEBUG__
	uint32_t __count;
	float __period;
#endif

	// Private Event Function
	PifEvtTaskLoop __evt_loop;
};

#ifdef __PIF_DEBUG__

extern PifActTaskSignal pif_act_task_signal;

#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTask_Init
 * @brief
 * @param p_owner
 */
void pifTask_Init(PifTask* p_owner);

/**
 * @fn pifTask_ChangeMode
 * @brief Task의 Mode를 변경한다.
 * @param p_owner Task 자신
 * @param mode Task의 Mode를 설정한다.
 * @param period Task 주기 (ms, us)
 * @return 성공 여부를 반환한다.
 */
BOOL pifTask_ChangeMode(PifTask* p_owner, PifTaskMode mode, uint16_t period);

/**
 * @fn pifTask_ChangePeriod
 * @brief Task의 주기를 변경한다.
 * @param p_owner Task 자신
 * @param period Task 주기 (ms, us)
 * @return 성공 여부를 반환한다.
 */
BOOL pifTask_ChangePeriod(PifTask* p_owner, uint16_t period);

/**
 * @fn pifTask_SetTrigger
 * @brief 
 * @param p_owner Task 자신
 * @return 
 */
BOOL pifTask_SetTrigger(PifTask* p_owner);

/**
 * @fn pifTask_DelayMs
 * @brief ms단위의 일정 시간동안 Task를 정지시킨다.
 * @param p_owner Task 자신
 * @param 정지시킬 시
 */
void pifTask_DelayMs(PifTask* p_owner, uint16_t delay);

/**
 * @fn pifTask_GetDeltaTime
 * @brief 
 * @param p_owner Task 자신
 * @param reset
 * @return
 */
uint32_t pifTask_GetDeltaTime(PifTask* p_owner, BOOL reset);


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
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param evt_loop Task 함수
 * @param p_client 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifTaskManager_Add(PifTaskMode mode, uint16_t period, PifEvtTaskLoop evt_loop, void* p_client, BOOL start);

/**
 * @fn pifTaskManager_Remove
 * @brief Task를 추가한다.
 * @param p_task Task의 Mode를 설정한다.
 */
void pifTaskManager_Remove(PifTask* p_task);

/**
 * @fn pifTaskManager_Count
 * @brief 등록된 task의 수를 구한다.
 * @return 등록된 task의 수를 반환한다.
 */
int pifTaskManager_Count();

/**
 * @fn pifTaskManager_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTaskManager_Loop();

/**
 * @fn pifTaskManager_Yield
 * @brief Task내에서 대기중에 다른 Task를 호출하고자 할 경우에 사용하는 함수이다.
 * @return
 */
BOOL pifTaskManager_Yield();

/**
 * @fn pifTaskManager_YieldMs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 * @return
 */
BOOL pifTaskManager_YieldMs(uint32_t time);

/**
 * @fn pifTaskManager_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 * @return
 */
BOOL pifTaskManager_YieldUs(uint32_t time);

/**
 * @fn pifTaskManager_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTaskManager_Print();

#ifdef __PIF_DEBUG__

/**
 * @fn pifTaskManager_PrintRatioTable
 * @brief Task의 Ratio Table을 출력한다.
 */
void pifTaskManager_PrintRatioTable();

#endif	// __PIF_DEBUG__

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
