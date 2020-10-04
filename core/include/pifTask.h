#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "pif.h"


typedef enum _PIF_enTaskLoop
{
	TL_enAll	= 0,
	TL_enEach	= 1
} PIF_enTaskLoop;


struct _PIF_stTask;
typedef struct _PIF_stTask PIF_stTask;

typedef void (*PIF_evtTaskLoop)(PIF_stTask *pstTask);


/**
 * @class _PIF_stTask
 * @author SlowlyBarefoot
 * @date 11/06/20
 * @file pifTask.h
 * @brief Task를 관리하는 구조체
 */
struct _PIF_stTask
{
	// Public Member Variable
	const char *pcName;
	uint8_t ucId;
	uint8_t ucRatio;
	uint16_t usPeriodUs;
	void *pvParam;

	// Private Member Variable
	BOOL __bPause;
	BOOL __bTaskLoop;
	uint32_t __unPretime;
	uint32_t __unCount;
	float __fPeriod;

	// Private Member Function
	PIF_evtTaskLoop __evtLoop;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifTask_Init(uint8_t ucSize);
void pifTask_Exit();

PIF_stTask *pifTask_Add(uint8_t ucRatio, PIF_evtTaskLoop evtLoop, void *pvParam);

void pifTask_SetName(PIF_stTask *pstOwner, const char *pcName);

void pifTask_Pause(PIF_stTask *pstTask);
void pifTask_Retart(PIF_stTask *pstTask);

void pifTask_Loop();

#ifdef __PIF_DEBUG__

void pifTask_Print();

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
