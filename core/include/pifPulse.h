#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "pifTask.h"


typedef void (*PIF_evtPulseFinish)(void *pvIssuer);

typedef enum _PIF_enPulseType
{
    PT_enOnce       = 0,
    PT_enRepeat		= 1
} PIF_enPulseType;

typedef enum _PIF_enPulseStep
{
    PS_enStop		= 0,
    PS_enRunning	= 1,
	PS_enPause		= 2,
    PS_enRemove		= 3
} PIF_enPulseStep;

/**
 * @struct _PIF_stPulseItem
 * @brief 한 Pulse내에 항목을 관리하는 구조체
 */
typedef struct _PIF_stPulseItem
{
	// Public Member Variable
    PIF_enPulseType enType;

	// Private Member Variable
    PIF_enPulseStep __enStep;
	BOOL __bEvent;
    uint32_t __unValue;
    uint32_t __unPulse;
    void *__pvFinishIssuer;

	uint8_t __ucArrayIndex;
    uint8_t __unIndex;
    uint8_t __unNext;
    uint8_t __unPrev;

    // Private Member Function
    PIF_evtPulseFinish __evtFinish;
} PIF_stPulseItem;

/**
 * @struct _PIF_Pulse
 * @brief Pulse를 관리하기 위한 구조체
 */
typedef struct _PIF_stPulse
{
	// Public Member Variable
    uint32_t unScale;

	// Private Member Variable
	uint8_t __ucArrayIndex;
    uint8_t __unFreeNext;
    uint8_t __unAllocNext;
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

PIF_stPulseItem *pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType);
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstPulseItem);

BOOL pifPulse_StartItem(PIF_stPulseItem *pstPulseItem, uint32_t unPulse);
void pifPulse_StopItem(PIF_stPulseItem *pstPulseItem);
BOOL pifPulse_PauseItem(PIF_stPulseItem *pstPulseItem);
void pifPulse_RestartItem(PIF_stPulseItem *pstPulseItem);
void pifPulse_ResetItem(PIF_stPulseItem *pstPulseItem, uint32_t unPulse);

PIF_enPulseStep pifPulse_GetStep(PIF_stPulseItem *pstPulseItem);

uint32_t pifPulse_RemainItem(PIF_stPulseItem *pstPulseItem);
uint32_t pifPulse_ElapsedItem(PIF_stPulseItem *pstPulseItem);

// Signal Function
void pifPulse_sigTick(PIF_stPulse *pstOwner);

// Attach Event Function
void pifPulse_AttachEvtFinish(PIF_stPulseItem *pstPulseItem, PIF_evtPulseFinish evtFinish, void *pvIssuer);

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
