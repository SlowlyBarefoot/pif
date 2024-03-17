#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "core/pif.h"


#ifndef PIF_TASK_TABLE_SIZE
#define PIF_TASK_TABLE_SIZE				32
#endif

#define DISALLOW_YIELD_ID_NONE		0


typedef enum EnPifTaskMode
{
	TM_NONE				= 0,

	TM_MAIN_MASK		= 0xF0,
	TM_SUB_MASK			= 0x0E,
	TM_UNIT_MASK		= 0x01,

	TM_RATIO			= 0x02,
	TM_ALWAYS			= 0x04,
	TM_TIMER			= 0x08,		// Do not use it for other purposes because it is a mode used by the timer.

	TM_EXTERNAL			= 0x10,
	TM_EXTERNAL_CUTIN	= 0x12,		// After an external trigger occurs, it is executed as soon as the currently running task ends. However, this mode is set to only one task per program.
	TM_EXTERNAL_ORDER	= 0x14,

	TM_PERIOD			= 0x20,
	TM_PERIOD_MS		= 0x20,
	TM_PERIOD_US		= 0x21,

	TM_CHANGE			= 0x40,
	TM_CHANGE_MS		= 0x40,
	TM_CHANGE_US		= 0x41,

	TM_IDLE				= 0x80,
	TM_IDLE_MS			= 0x80,		// If at least one TM_ALWAYS task exists, the TM_IDLE_MS task is not executed.
	TM_IDLE_US			= 0x81		// If at least one TM_ALWAYS task exists, the TM_IDLE_US task is not executed.
} PifTaskMode;


struct StPifTask;
typedef struct StPifTask PifTask;

typedef uint16_t (*PifEvtTaskLoop)(PifTask* p_task);
typedef void (*PifActTaskSignal)(BOOL state);

typedef PifTask* (*PifTaskProcessing)(PifTask* p_owner);

typedef BOOL (*PifTaskCheckAbort)(PifIssuerP p_issuer);


/**
 * @struct StPifTask
 * @brief Task를 관리하는 구조체
 */
struct StPifTask
{
	// Public Member Variable
	const char* name;
	BOOL pause;
	uint8_t disallow_yield_id;		// 0 : 모두 허용, 1->255 : 해당 id는 허용하지 않음.

	// Read-only Member Variable
	PifId _id;
	PifTaskMode _mode;
	BOOL _running;
	uint16_t _default_period;
	uint16_t _delta_time;
	void *_p_client;
#ifdef PIF_USE_TASK_STATISTICS
    int32_t _max_execution_time;
	uint32_t _max_trigger_delay;
#endif

	// Private Member Variable
	PifTaskProcessing __processing;
	uint16_t __period;
	BOOL __trigger;
	int __table_number;
	uint16_t __delay_ms;
	uint32_t __current_time;
	uint32_t __pretime;
	uint32_t __last_execute_time;
	uint32_t __trigger_time;
#ifdef PIF_USE_TASK_STATISTICS
	uint32_t __total_delta_time[2];
    uint32_t __total_execution_time[2];		// total time consumed by task since boot
	uint32_t __total_trigger_delay[2];
	uint16_t __execution_count;
	uint16_t __trigger_count;
	uint8_t __execute_index;
	uint8_t __trigger_index;
#endif
#ifdef PIF_DEBUG
	uint32_t __ratio_count;
	float __ratio_period;
#endif

	// Private Event Function
	PifEvtTaskLoop __evt_loop;
};

#ifdef PIF_DEBUG

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

#ifdef PIF_USE_TASK_STATISTICS

/**
 * @fn pifTask_GetAverageDeltaTime
 * @brief
 * @param p_owner Task 자신
 * @return
 */
uint32_t pifTask_GetAverageDeltaTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageExecuteTime
 * @brief
 * @param p_owner Task 자신
 * @return
 */
uint32_t pifTask_GetAverageExecuteTime(PifTask* p_owner);

/**
 * @fn pifTask_GetAverageTriggerTime
 * @brief
 * @param p_owner Task 자신
 * @return
 */
uint32_t pifTask_GetAverageTriggerTime(PifTask* p_owner);

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
void pifTaskManager_YieldMs(int32_t time);

/**
 * @fn pifTaskManager_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 */
void pifTaskManager_YieldUs(int32_t time);

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
void pifTaskManager_YieldAbortMs(int32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

/**
 * @fn pifTaskManager_YieldAbortUs
 * @brief loop내에서 지정한 시간과 중단 조건 함수가 TRUE를 반환할 때까지 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param time
 * @param p_check_abort
 * @param p_issuer
 */
void pifTaskManager_YieldAbortUs(int32_t time, PifTaskCheckAbort p_check_abort, PifIssuerP p_issuer);

#if !defined(PIF_NO_LOG) || defined(PIF_LOG_COMMAND)

/**
 * @fn pifTaskManager_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTaskManager_Print();

#ifdef PIF_DEBUG

/**
 * @fn pifTaskManager_PrintRatioTable
 * @brief Task의 Ratio Table을 출력한다.
 */
void pifTaskManager_PrintRatioTable();

#endif	// PIF_DEBUG

#endif	// PIF_NO_LOG

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
