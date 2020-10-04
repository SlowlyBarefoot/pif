#if USE_LOG == 1
#include "pifLog.h"
#endif
#include "pifPulse.h"


static PIF_stPulse *s_pstPulseArray;
static uint8_t s_ucPulseArraySize;
static uint8_t s_ucPulseArrayPos;


static void _LoopCommon(PIF_stPulse *pstOwner)
{
	PIF_unPulseIndex index;
    PIF_stPulseItem *pstPulseItem;

	index = pstOwner->__unAllocNext;
	while (index != PIF_PULSE_INDEX_NULL) {
		pstPulseItem = &pstOwner->__pstItems[index];

		if (pstPulseItem->bEvent) {
			pstPulseItem->bEvent = FALSE;

			if (pstPulseItem->evtFinish) (*pstPulseItem->evtFinish)(pstPulseItem->pvFinishIssuer);
		}

		index = pstPulseItem->unNext;
	}
}

/**
 * @fn pifPulse_Init
 * @brief 입력된 크기만큼 Pulse 구조체를 할당하고 초기화한다.
 * @param ucSize Pulse 크기
 * @return 성공 여부
 */
BOOL pifPulse_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstPulseArray = calloc(sizeof(PIF_stPulse), ucSize);
    if (!s_pstPulseArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucPulseArraySize = ucSize;
    s_ucPulseArrayPos = 0;
    return TRUE;

fail:
#if USE_LOG == 1
	pifLog_Printf(LT_enError, "Pulse:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifPulse_Exit
 * @brief Pulse용 메모리를 반환한다.
 */
void pifPulse_Exit()
{
	PIF_stPulse *pstOwner;

    if (s_pstPulseArray) {
		for (int i = 0; i < s_ucPulseArrayPos; i++) {
			pstOwner = &s_pstPulseArray[i];
			if (pstOwner->__pstItems) {
				free(pstOwner->__pstItems);
				pstOwner->__pstItems = NULL;
			}
		}
        free(s_pstPulseArray);
        s_pstPulseArray = NULL;
    }
}

/**
 * @fn pifPulse_Add
 * @brief Pulse를 추가한다.
 * @param ucSize Pulse 항목 크기
 * @param unScale 측정되는 Pulse 단위와 함수에 전달되는 파라메터의 단위가 다를 경우에 사용한다. 보통은 1을 사용한다.
 * @return Pulse 구조체 포인터를 반환한다.
 */
PIF_stPulse *pifPulse_Add(uint8_t ucSize, uint32_t unScale)
{
    if (ucSize >= PIF_PULSE_INDEX_NULL || !unScale) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}
    if (s_ucPulseArrayPos >= s_ucPulseArraySize) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

    PIF_stPulse *pstOwner = &s_pstPulseArray[s_ucPulseArrayPos];

    pstOwner->__pstItems = calloc(sizeof(PIF_stPulseItem), ucSize);
    if (!pstOwner->__pstItems) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->unScale = unScale;
    pstOwner->__ucItemSize = ucSize;

    pstOwner->__unFreeNext = 0;
    pstOwner->__unAllocNext = PIF_PULSE_INDEX_NULL;
    for (int i = 0; i < ucSize - 1; i++) {
    	pstOwner->__pstItems[i].unPrev = PIF_PULSE_INDEX_NULL;
    	pstOwner->__pstItems[i].unNext = i + 1;
    }
    pstOwner->__pstItems[ucSize - 1].unPrev = PIF_PULSE_INDEX_NULL;
    pstOwner->__pstItems[ucSize - 1].unNext = PIF_PULSE_INDEX_NULL;

    s_ucPulseArrayPos = s_ucPulseArrayPos + 1;
    return pstOwner;

fail:
#if USE_LOG == 1
	pifLog_Printf(LT_enError, "Pulse:Add(SZ:%u SC:%u) EC:%d", ucSize, unScale, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifPulse_AddItem
 * @brief Pulse의 항목을 추가한다.
 * @param pstOwner Pulse 자신
 * @param enType Pulse의 종류
 * @return Pulse 항목 번호
 */
PIF_unPulseIndex pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType)
{
	if (pstOwner->__unFreeNext == PIF_PULSE_INDEX_NULL) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_unPulseIndex index = pstOwner->__unFreeNext;
    PIF_stPulseItem *pstPulseItem = &pstOwner->__pstItems[index];
    pstPulseItem->enType = enType;
    pstPulseItem->evtFinish = NULL;
    pstPulseItem->pvFinishIssuer = NULL;

    pstPulseItem->enStep = TS_enStop;

    pstOwner->__unFreeNext = pstPulseItem->unNext;

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) {
    	pstOwner->__unAllocNext = index;
        pstPulseItem->unNext = PIF_PULSE_INDEX_NULL;
        pstPulseItem->unPrev = PIF_PULSE_INDEX_NULL;
    }
    else {
        pstPulseItem->unNext = pstOwner->__unAllocNext;
        pstPulseItem->unPrev = PIF_PULSE_INDEX_NULL;
        pstOwner->__unAllocNext = index;
        pstOwner->__pstItems[pstPulseItem->unNext].unPrev = index;
    }

    return index;

fail:
#if USE_LOG == 1
	pifLog_Printf(LT_enError, "Pulse:AddItem(T:%d) EC:%d", enType, pif_enError);
#endif
    return PIF_PULSE_INDEX_NULL;
}

/**
 * @fn pifPulse_RemoveItem
 * @brief Pulse 항목을 삭제한다.
 * @param pstOwner Pulse 자신
 * @param unIndex Pulse 항목 번호
 */
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
    PIF_unPulseIndex index;
    PIF_stPulseItem *pstPulseItem = &pstOwner->__pstItems[unIndex];

    if (pstPulseItem->enStep == TS_enRemove) return;

    index = pstPulseItem->unNext;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].unPrev = pstPulseItem->unPrev;
    }
    index = pstPulseItem->unPrev;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].unNext = pstPulseItem->unNext;
    }
    else {
    	pstOwner->__unAllocNext = pstPulseItem->unNext;
    }

    pstPulseItem->unNext = pstOwner->__unFreeNext;
    pstPulseItem->unPrev = PIF_PULSE_INDEX_NULL;
    pstPulseItem->enStep = TS_enRemove;
    pstOwner->__unFreeNext = unIndex;
}

/**
 * @fn pifPulse_StartItem
 * @brief Pulse 항목을 시작
 * @param pstOwner
 * @param unIndex
 * @param unPulse
 */
BOOL pifPulse_StartItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, uint32_t unPulse)
{
    if (!unPulse) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (pstOwner->__pstItems[unIndex].enStep == TS_enStop) {
    	pstOwner->__pstItems[unIndex].enStep = TS_enRunning;
    	pstOwner->__pstItems[unIndex].bEvent = FALSE;
    }
    pstOwner->__pstItems[unIndex].unValue = unPulse / pstOwner->unScale;
	pstOwner->__pstItems[unIndex].unPulse = pstOwner->__pstItems[unIndex].unValue;
    return TRUE;

fail:
#if USE_LOG == 1
	pifLog_Printf(LT_enError, "Pulse:StartItem(I:%u P:%d) EC:%d", unIndex, unPulse, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifPulse_StopItem
 * @brief
 * @param pstOwner
 * @param unIndex
 */
void pifPulse_StopItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
	pstOwner->__pstItems[unIndex].unPulse = 0;
	pstOwner->__pstItems[unIndex].enStep = TS_enStop;
}

/**
 * @fn pifPulse_PauseItem
 * @brief
 * @param pstOwner
 * @param unIndex
 * @return
 */
BOOL pifPulse_PauseItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
    if (pstOwner->__pstItems[unIndex].enStep == TS_enRunning) {
    	pstOwner->__pstItems[unIndex].enStep = TS_enPause;
    	return TRUE;
    }
    return FALSE;
}

/**
 * @fn pifPulse_RestartItem
 * @brief
 * @param pstOwner
 * @param unIndex
 */
void pifPulse_RestartItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
    pstOwner->__pstItems[unIndex].enStep = TS_enRunning;
}

/**
 * @fn pifPulse_ResetItem
 * @brief
 * @param pstOwner
 * @param unIndex
 * @param unPulse
 */
void pifPulse_ResetItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, uint32_t unPulse)
{
    pstOwner->__pstItems[unIndex].unValue = unPulse / pstOwner->unScale;
}

/**
 * @fn pifPulse_GetStep
 * @brief
 * @param pstOwner
 * @param unIndex
 * @return
 */
PIF_enPulseStep pifPulse_GetStep(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
	return pstOwner->__pstItems[unIndex].enStep;
}

/**
 * @fn pifPulse_RemainItem
 * @brief
 * @param pstOwner
 * @param unIndex
 * @return
 */
uint32_t pifPulse_RemainItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
	if (pstOwner->__pstItems[unIndex].enStep != TS_enRunning) return 0;
	else return pstOwner->__pstItems[unIndex].unPulse;

}

/**
 * @fn pifPulse_ElapsedItem
 * @brief
 * @param pstOwner
 * @param unIndex
 * @return
 */
uint32_t pifPulse_ElapsedItem(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex)
{
	if (pstOwner->__pstItems[unIndex].enStep != TS_enRunning) return 0;
	else return pstOwner->__pstItems[unIndex].unValue - pstOwner->__pstItems[unIndex].unPulse;
}

/**
 * @fn pifPulse_sigTick
 * @brief Interrupt Function에서 호출할 것
 * @param pstOwner
 */
void pifPulse_sigTick(PIF_stPulse *pstOwner)
{
	PIF_stPulseItem *pstPulseItem;

    if (!pstOwner) return;

    PIF_unPulseIndex index = pstOwner->__unAllocNext;
    while (index != PIF_PULSE_INDEX_NULL) {
        pstPulseItem = &pstOwner->__pstItems[index];

        if (pstPulseItem->enStep != TS_enPause) {
			if (pstPulseItem->unPulse) {
				pstPulseItem->unPulse--;
				if (!pstPulseItem->unPulse) {
					if (pstPulseItem->enType == TT_enRepeat) {
						pstPulseItem->unPulse = pstPulseItem->unValue;
					}
					else {
						pstPulseItem->enStep = TS_enStop;
					}
					pstPulseItem->bEvent = TRUE;
				}
			}
        }

        index = pstPulseItem->unNext;
    }
}

/**
 * @fn pifPulse_AttachEvtFinish
 * @brief
 * @param pstOwner
 * @param unIndex
 * @param evtFinish
 * @param pvIssuer
 */
void pifPulse_AttachEvtFinish(PIF_stPulse *pstOwner, PIF_unPulseIndex unIndex, PIF_evtPulseFinish evtFinish, void *pvIssuer)
{
	pstOwner->__pstItems[unIndex].evtFinish = evtFinish;
	pstOwner->__pstItems[unIndex].pvFinishIssuer = pvIssuer;
}

/**
 * @fn pifPulse_LoopAll
 * @brief
 * @param pstTask
 */
void pifPulse_LoopAll(PIF_stTask *pstTask)
{
	(void) pstTask;

    for (int i = 0; i < s_ucPulseArrayPos; i++) {
        PIF_stPulse *pstOwner = &s_pstPulseArray[i];
    	if (!pstOwner->__enTaskLoop) _LoopCommon(pstOwner);
    }
}

/**
 * @fn pifPulse_LoopEach
 * @brief
 * @param pstTask
 */
void pifPulse_LoopEach(PIF_stTask *pstTask)
{
	PIF_stPulse *pstOwner = pstTask->pvParam;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_LoopCommon(pstOwner);
	}
}

#ifdef __PIF_DEBUG__

/**
 * @fn pifPulse_CheckItem
 * @brief
 * @param pstOwner
 * @return
 */
BOOL pifPulse_CheckItem(PIF_stPulse *pstOwner)
{
	PIF_unPulseIndex index;
    int count = 0;

    if (pstOwner->__unFreeNext != PIF_PULSE_INDEX_NULL) {
        index = pstOwner->__unFreeNext;
        do {
            count++;
            index = pstOwner->__pstItems[index].unNext;
        } while (index != PIF_PULSE_INDEX_NULL);
    }

    if (pstOwner->__unAllocNext != PIF_PULSE_INDEX_NULL) {
        index = pstOwner->__unAllocNext;
        do {
            count++;
            index = pstOwner->__pstItems[index].unNext;
        } while (index != PIF_PULSE_INDEX_NULL);
    }
    return count == pstOwner->__ucItemSize;
}

/**
 * @fn pifPulse_PrintItemList
 * @brief
 * @param pstOwner
 */
void pifPulse_PrintItemList(PIF_stPulse *pstOwner)
{
	pifLog_Printf(LT_enNone, "\nScale=%u: Free = %d, Alloc = %d\n", pstOwner->unScale, pstOwner->__unFreeNext, pstOwner->__unAllocNext);

    for (PIF_unPulseIndex index = 0; index < pstOwner->__ucItemSize; index++) {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d", index, pstOwner->__pstItems[index].unNext, pstOwner->__pstItems[index].unPrev);
    }
}

/**
 * @fn pifPulse_PrintItemFree
 * @brief
 * @param pstOwner
 */
void pifPulse_PrintItemFree(PIF_stPulse *pstOwner)
{
	PIF_unPulseIndex index;

	pifLog_Printf(LT_enNone, "\nScale=%u: Free = %d\n", pstOwner->unScale, pstOwner->__unFreeNext);

    if (pstOwner->__unFreeNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unFreeNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d", index, pstOwner->__pstItems[index].unNext, pstOwner->__pstItems[index].unPrev);

        index = pstOwner->__pstItems[index].unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
}

/**
 * @fn pifPulse_PrintItemAlloc
 * @brief
 * @param pstOwner
 */
void pifPulse_PrintItemAlloc(PIF_stPulse *pstOwner)
{
	PIF_unPulseIndex index;

	pifLog_Printf(LT_enNone, "\nScale=%u: Alloc = %d\n", pstOwner->unScale, pstOwner->__unAllocNext);

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unAllocNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d", index, pstOwner->__pstItems[index].unNext, pstOwner->__pstItems[index].unPrev);

        index = pstOwner->__pstItems[index].unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
}

#endif  // __PIF_DEBUG__
