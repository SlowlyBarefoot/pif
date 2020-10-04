#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pifTask.h"


#define PIF_PULSE_INDEX_NULL   0xFF


typedef void (*PIF_evtPulseFinish)(void *pvIssuer);

typedef uint8_t PIF_unPulseIndex;

typedef enum _PIF_enPulseType
{
    TT_enOnce       = 0,
    TT_enRepeat		= 1
} PIF_enPulseType;

typedef enum _PIF_enPulseStep
{
    TS_enStop		= 0,
    TS_enRunning	= 1,
	TS_enPause		= 2,
    TS_enRemove		= 3
} PIF_enPulseStep;

/**
 * @class _PIF_stPulseItem
 * @author SlowlyBarefoot
 * @date 26/04/20
 * @file pifPulse.h
 * @brief 한 Pulse내에 한 항목을 관리하는 구조체
 */
typedef struct _PIF_stPulseItem
{
    PIF_enPulseType enType;
    PIF_evtPulseFinish evtFinish;
    void *pvFinishIssuer;

    PIF_enPulseStep enStep;
	BOOL bEvent;
    uint32_t unValue;
    uint32_t unPulse;

    PIF_unPulseIndex unNext;
    PIF_unPulseIndex unPrev;
} PIF_stPulseItem;

/**
 * @class _PIF_Pulse
 * @author SlowlyBarefoot
 * @date 26/04/20
 * @file pifPulse.h
 * @brief Pulse를 관리하기 위한 구조체
 */
typedef struct _PIF_stPulse
{
	// Public Member Variable
    uint32_t unScale;

	// Private Member Variable
    PIF_unPulseIndex __unFreeNext;
    PIF_unPulseIndex __unAllocNext;
    uint8_t __ucItemSize;
    PIF_stPulseItem *__pstItems;

	PIF_enTaskLoop __enTaskLoop;
} PIF_stPulse;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifPulse_Init(uint8_t ucSize);
void pifPulse_Exit();

PIF_stPulse *pifPulse_Add(uint8_t ucSize, uint32_t unScale);

PIF_unPulseIndex pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType);
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);

BOOL pifPulse_StartItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, uint32_t unPulse);
void pifPulse_StopItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);
BOOL pifPulse_PauseItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);
void pifPulse_RestartItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);
void pifPulse_ResetItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, uint32_t unPulse);

PIF_enPulseStep pifPulse_GetStep(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);

uint32_t pifPulse_RemainItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);
uint32_t pifPulse_ElapsedItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex);

// Signal Function
void pifPulse_sigTick(PIF_stPulse *pstOwner);

// Attach Event Function
void pifPulse_AttachEvtFinish(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, PIF_evtPulseFinish evtFinish, void *pvIssuer);

// Task Function
void pifPulse_LoopAll(PIF_stTask *pstTask);
void pifPulse_LoopEach(PIF_stTask *pstTask);

#ifdef __PIF_DEBUG__

BOOL pifPulse_CheckItem(PIF_stPulse *pstOwner);

void pifPulse_PrintItemList(PIF_stPulse *pstOwner);
void pifPulse_PrintItemFree(PIF_stPulse *pstOwner);
void pifPulse_PrintItemAlloc(PIF_stPulse *pstOwner);

#endif  // __PIF_DEBUG__

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
