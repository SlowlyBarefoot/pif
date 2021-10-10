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

void pifTask_Init(PifTask* p_owner);

void pifTask_SetPeriod(PifTask* p_owner, uint16_t period);


void pifTaskManager_Init();
void pifTaskManager_Clear();

PifTask* pifTaskManager_Add(PifTaskMode mode, uint16_t period, PifEvtTaskLoop evt_loop, void* p_client, BOOL start);

void pifTaskManager_Loop();
void pifTaskManager_Yield();
void pifTaskManager_YieldMs(uint32_t time);
void pifTaskManager_YieldUs(uint32_t time);
void pifTaskManager_YieldPeriod(PifTask* p_owner);

#ifdef __PIF_DEBUG__

void pifTaskManager_Print();

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
