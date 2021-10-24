#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "pif.h"


typedef enum EnPifTaskMode
{
	TM_RATIO		= 0,
	TM_ALWAYS		= 1,
	TM_PERIOD_MS	= 2,
	TM_PERIOD_US	= 3,
	TM_CHANGE_MS	= 4,
	TM_CHANGE_US	= 5
} PifTaskMode;


struct StPifTask;
typedef struct StPifTask PifTask;

typedef uint16_t (*PifEvtTaskLoop)(PifTask* p_task);


/**
 * @struct StPifTask
 * @brief Task를 관리하는 구조체
 */
struct StPifTask
{
	// Public Member Variable
	BOOL pause;
	BOOL immediate;

	// Read-only Member Variable
	PifId _id;
	PifTaskMode _mode;
	uint16_t _period;
	void *_p_client;

	// Private Member Variable
	int __table_number;
	BOOL __running;
	uint32_t __pretime;
#ifdef __PIF_DEBUG__
	uint32_t __count;
	float __period;
#endif

	// Private Event Function
	PifEvtTaskLoop __evt_loop;
};


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
 * @fn pifTask_SetPeriod
 * @brief Task의 주기를 설정한다.
 * @param p_owner Task 자신
 * @param period Task 주기 (ms, us)
 */
void pifTask_SetPeriod(PifTask* p_owner, uint16_t period);


/**
 * @fn pifTaskManager_Init
 * @brief Task용 구조체를 초기화한다.
 */
void pifTaskManager_Init();

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
 * @param usTime
 */
void pifTaskManager_YieldMs(uint32_t time);

/**
 * @fn pifTaskManager_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param usTime
 */
void pifTaskManager_YieldUs(uint32_t time);

/**
 * @fn pifTaskManager_YieldPeriod
 * @brief loop내에서 지정된 주기동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param p_owner
 */
void pifTaskManager_YieldPeriod(PifTask* p_owner);

#ifdef __PIF_DEBUG__

/**
 * @fn pifTaskManager_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTaskManager_Print();

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
