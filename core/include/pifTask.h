#ifndef PIF_TASK_H
#define PIF_TASK_H


#include "pif.h"


#ifndef PIF_TASK_TABLE_SIZE
#define PIF_TASK_TABLE_SIZE		32
#endif


typedef enum _PIF_enTaskMode
{
	TM_enRatio		= 0,
	TM_enPeriodMs	= 1,
	TM_enPeriodUs	= 2,
	TM_enAlways		= 3
} PIF_enTaskMode;

typedef enum _PIF_enTaskLoop
{
	TL_enAll	= 0,
	TL_enEach	= 1
} PIF_enTaskLoop;


struct _PIF_stTask;
typedef struct _PIF_stTask PIF_stTask;

typedef void (*PIF_evtTaskLoop)(PIF_stTask *pstTask);


/**
 * @struct _PIF_stTask
 * @brief Task를 관리하는 구조체
 */
struct _PIF_stTask
{
	// Public Member Variable
	BOOL bPause;
	void *pvLoopEach;

	// Read-only Member Variable
	PIF_usId _usPifId;
	PIF_enTaskMode _enMode;

	// Private Member Variable
	const char *__pcName;
	uint8_t __ucRatio;
	uint16_t __usPeriod;
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

PIF_stTask *pifTask_AddRatio(uint8_t ucRatio, PIF_evtTaskLoop evtLoop, void *pvLoopEach);
PIF_stTask *pifTask_AddPeriodMs(uint16_t usPeriodMs, PIF_evtTaskLoop evtLoop, void *pvLoopEach);
PIF_stTask *pifTask_AddPeriodUs(uint16_t usPeriodUs, PIF_evtTaskLoop evtLoop, void *pvLoopEach);

void pifTask_SetName(PIF_stTask *pstOwner, const char *pcName);
void pifTask_SetPeriod(PIF_stTask *pstOwner, uint16_t usPeriod);

void pifTask_Loop();

#ifdef __PIF_DEBUG__

void pifTask_Print();

#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_TASK_H
