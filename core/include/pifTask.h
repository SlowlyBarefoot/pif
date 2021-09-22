#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "pif.h"


typedef enum _PIF_enTaskMode
{
	TM_enRatio		= 0,
	TM_enAlways		= 1,
	TM_enPeriodMs	= 2,
	TM_enPeriodUs	= 3,
	TM_enChangeMs	= 4,
	TM_enChangeUs	= 5
} PIF_enTaskMode;


struct _PIF_stTask;
typedef struct _PIF_stTask PIF_stTask;

typedef uint16_t (*PIF_evtTaskLoop)(PIF_stTask *pstTask);


/**
 * @struct _PIF_stTask
 * @brief Task를 관리하는 구조체
 */
struct _PIF_stTask
{
	// Public Member Variable
	BOOL bPause;
	BOOL bImmediate;

	// Read-only Member Variable
	PIF_usId _usPifId;
	PIF_enTaskMode _enMode;
	uint16_t _usPeriod;
	void *_pvClient;

	// Private Member Variable
	BOOL __bRunning;
	uint32_t __unPretime;
#ifdef __PIF_DEBUG__
	uint32_t __unCount;
	float __fPeriod;
#endif

	// Private Event Function
	PIF_evtTaskLoop __evtLoop;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifTask_Init(uint8_t ucSize);
void pifTask_Exit();

PIF_stTask *pifTask_Add(PIF_enTaskMode enMode, uint16_t usPeriod, PIF_evtTaskLoop evtLoop, void *pvClient, BOOL bStart);

void pifTask_SetPeriod(PIF_stTask *pstOwner, uint16_t usPeriod);

void pifTask_Loop();
void pifTask_Yield();
void pifTask_YieldMs(uint32_t unTime);
void pifTask_YieldUs(uint32_t unTime);
void pifTask_YieldPeriod(PIF_stTask *pstOwner);

#ifdef __PIF_DEBUG__

void pifTask_Print();

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
