#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifPulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


static PIF_stPulse *s_pstPulseArray = NULL;
static uint8_t s_ucPulseArraySize;
static uint8_t s_ucPulseArrayPos;


static void _TaskCommon(PIF_stPulse *pstOwner)
{
	uint8_t index;
	PIF_stPulseItem *pstPulseItem;

	index = pstOwner->__unAllocNext;
	while (index != PIF_PULSE_INDEX_NULL) {
		pstPulseItem = &pstOwner->__pstItems[index];

		if (pstPulseItem->__bEvent) {
			pstPulseItem->__bEvent = FALSE;

			if (pstPulseItem->__evtFinish) (*pstPulseItem->__evtFinish)(pstPulseItem->__pvFinishIssuer);
		}

		index = pstPulseItem->__unNext;
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
#ifndef __PIF_NO_LOG__
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
 * @return Pulse 구조체 포인터를 반환한다.
 */
PIF_stPulse *pifPulse_Add(uint8_t ucSize)
{
    if (ucSize >= PIF_PULSE_INDEX_NULL) {
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

    pstOwner->__ucArrayIndex = s_ucPulseArrayPos;
    pstOwner->__ucItemSize = ucSize;

    pstOwner->__unFreeNext = 0;
    pstOwner->__unAllocNext = PIF_PULSE_INDEX_NULL;
    for (int i = 0; i < ucSize - 1; i++) {
    	pstOwner->__pstItems[i].__unPrev = PIF_PULSE_INDEX_NULL;
    	pstOwner->__pstItems[i].__unNext = i + 1;
    }
    pstOwner->__pstItems[ucSize - 1].__unPrev = PIF_PULSE_INDEX_NULL;
    pstOwner->__pstItems[ucSize - 1].__unNext = PIF_PULSE_INDEX_NULL;

    s_ucPulseArrayPos = s_ucPulseArrayPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Pulse:Add(SZ:%u) EC:%d", ucSize, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifPulse_AddItem
 * @brief Pulse의 항목을 추가한다.
 * @param pstOwner Pulse 자신
 * @param enType Pulse의 종류
 * @return Pulse 항목 구조체 포인터를 반환한다.
 */
PIF_stPulseItem *pifPulse_AddItem(PIF_stPulse *pstOwner, PIF_enPulseType enType)
{
	if (pstOwner->__unFreeNext == PIF_PULSE_INDEX_NULL) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

	uint8_t index = pstOwner->__unFreeNext;
    PIF_stPulseItem *pstPulseItem = &pstOwner->__pstItems[index];
    pstPulseItem->enType = enType;
    pstPulseItem->__ucArrayIndex = pstOwner->__ucArrayIndex;
    pstPulseItem->__evtFinish = NULL;
    pstPulseItem->__pvFinishIssuer = NULL;
    pstPulseItem->__unIndex = index;

    pstPulseItem->__enStep = PS_enStop;

    pstOwner->__unFreeNext = pstPulseItem->__unNext;

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) {
    	pstOwner->__unAllocNext = index;
        pstPulseItem->__unNext = PIF_PULSE_INDEX_NULL;
        pstPulseItem->__unPrev = PIF_PULSE_INDEX_NULL;
    }
    else {
        pstPulseItem->__unNext = pstOwner->__unAllocNext;
        pstPulseItem->__unPrev = PIF_PULSE_INDEX_NULL;
        pstOwner->__unAllocNext = index;
        pstOwner->__pstItems[pstPulseItem->__unNext].__unPrev = index;
    }

    return pstPulseItem;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Pulse:AddItem(T:%d) EC:%d", enType, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifPulse_RemoveItem
 * @brief Pulse 항목을 삭제한다.
 * @param pstOwner Pulse 자신
 * @param pstPulseItem Pulse 항목 포인터
 */
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstPulseItem)
{
	uint8_t index;

    if (pstPulseItem->__enStep == PS_enRemove) return;

    index = pstPulseItem->__unNext;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].__unPrev = pstPulseItem->__unPrev;
    }
    index = pstPulseItem->__unPrev;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].__unNext = pstPulseItem->__unNext;
    }
    else {
    	pstOwner->__unAllocNext = pstPulseItem->__unNext;
    }

    pstPulseItem->__unNext = pstOwner->__unFreeNext;
    pstPulseItem->__unPrev = PIF_PULSE_INDEX_NULL;
    pstPulseItem->__enStep = PS_enRemove;
    pstOwner->__unFreeNext = pstPulseItem->__unIndex;
}

/**
 * @fn pifPulse_StartItem
 * @brief Pulse 항목을 시작한다.
 * @param pstPulseItem Pulse 항목 포인터
 * @param unPulse 이동 pulse 수
 */
BOOL pifPulse_StartItem(PIF_stPulseItem *pstPulseItem, uint32_t unPulse)
{
    if (!unPulse) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (pstPulseItem->__enStep == PS_enStop) {
    	pstPulseItem->__enStep = PS_enRunning;
    	pstPulseItem->__bEvent = FALSE;
    }
    pstPulseItem->__unValue = unPulse;
    pstPulseItem->__unPulse = pstPulseItem->__unValue;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Pulse:StartItem(P:%d) EC:%d", unPulse, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifPulse_StopItem
 * @brief Pulse 항목을 종료한다.
 * @param pstPulseItem Pulse 항목 포인터
 */
void pifPulse_StopItem(PIF_stPulseItem *pstPulseItem)
{
	pstPulseItem->__unPulse = 0;
	pstPulseItem->__enStep = PS_enStop;
}

/**
 * @fn pifPulse_ResetItem
 * @brief Pulse 항목의 이동 pulse를 재설정한다.
 * @param pstPulseItem Pulse 항목 포인터
 * @param unPulse 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifPulse_ResetItem(PIF_stPulseItem *pstPulseItem, uint32_t unPulse)
{
	pstPulseItem->__unValue = unPulse;
}

/**
 * @fn pifPulse_GetStep
 * @brief Pulse 항목의 step을 얻는다.
 * @param pstPulseItem Pulse 항목 포인터
 * @return 현재 step을 반환한다.
 */
PIF_enPulseStep pifPulse_GetStep(PIF_stPulseItem *pstPulseItem)
{
	return pstPulseItem->__enStep;
}

/**
 * @fn pifPulse_RemainItem
 * @brief Pulse 항목의 남은 pulse 수를 얻는다.
 * @param pstPulseItem Pulse 항목 포인터
 * @return 남은 pulse 수를 반환한다.
 */
uint32_t pifPulse_RemainItem(PIF_stPulseItem *pstPulseItem)
{
	if (pstPulseItem->__enStep != PS_enRunning) return 0;
	else return pstPulseItem->__unPulse;

}

/**
 * @fn pifPulse_ElapsedItem
 * @brief Pulse 항목의 경과한 pulse 수를 얻는다.
 * @param pstPulseItem Pulse 항목 포인터
 * @return 경과한 pulse 수를 반환한다.
 */
uint32_t pifPulse_ElapsedItem(PIF_stPulseItem *pstPulseItem)
{
	if (pstPulseItem->__enStep != PS_enRunning) return 0;
	else return pstPulseItem->__unValue - pstPulseItem->__unPulse;
}

/**
 * @fn pifPulse_sigTick
 * @brief Pulse를 발생하는 Interrupt 함수에서 호출한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_sigTick(PIF_stPulse *pstOwner)
{
	PIF_stPulseItem *pstPulseItem;

    if (!pstOwner) return;

    uint8_t index = pstOwner->__unAllocNext;
    while (index != PIF_PULSE_INDEX_NULL) {
        pstPulseItem = &pstOwner->__pstItems[index];

		if (pstPulseItem->__unPulse) {
			pstPulseItem->__unPulse--;
			if (!pstPulseItem->__unPulse) {
				if (pstPulseItem->enType == PT_enRepeat) {
					pstPulseItem->__unPulse = pstPulseItem->__unValue;
				}
				else {
					pstPulseItem->__enStep = PS_enStop;
				}
				pstPulseItem->__bEvent = TRUE;
			}
		}

        index = pstPulseItem->__unNext;
    }
}

/**
 * @fn pifPulse_AttachEvtFinish
 * @brief Pulse 항목의 이동 완료시 발생시킬 이벤트를 연결한다.
 * @param pstPulseItem Pulse 항목 포인터
 * @param evtFinish 연결시킬 이벤트
 * @param pvIssuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtFinish(PIF_stPulseItem *pstPulseItem, PIF_evtPulseFinish evtFinish, void *pvIssuer)
{
	pstPulseItem->__evtFinish = evtFinish;
	pstPulseItem->__pvFinishIssuer = pvIssuer;
}

/**
 * @fn pifPulse_taskAll
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifPulse_taskAll(PIF_stTask *pstTask)
{
	(void) pstTask;

    for (int i = 0; i < s_ucPulseArrayPos; i++) {
        PIF_stPulse *pstOwner = &s_pstPulseArray[i];
    	if (!pstOwner->__enTaskLoop) _TaskCommon(pstOwner);
    }
}

/**
 * @fn pifPulse_taskEach
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifPulse_taskEach(PIF_stTask *pstTask)
{
	PIF_stPulse *pstOwner = pstTask->__pvOwner;

	if (pstTask->__bTaskLoop) {
		pstOwner->__enTaskLoop = TL_enEach;
	}
	else {
		_TaskCommon(pstOwner);
	}
}

#ifdef __PIF_DEBUG__

/**
 * @fn pifPulse_CheckItem
 * @brief Pulse 관리용 변수를 검증하는 함수이다.
 * @param pstOwner Pulse 자신
 * @return 성공 여부
 */
BOOL pifPulse_CheckItem(PIF_stPulse *pstOwner)
{
	uint8_t index;
    int count = 0;

    if (pstOwner->__unFreeNext != PIF_PULSE_INDEX_NULL) {
        index = pstOwner->__unFreeNext;
        do {
            count++;
            index = pstOwner->__pstItems[index].__unNext;
        } while (index != PIF_PULSE_INDEX_NULL);
    }

    if (pstOwner->__unAllocNext != PIF_PULSE_INDEX_NULL) {
        index = pstOwner->__unAllocNext;
        do {
            count++;
            index = pstOwner->__pstItems[index].__unNext;
        } while (index != PIF_PULSE_INDEX_NULL);
    }
    return count == pstOwner->__ucItemSize;
}

/**
 * @fn pifPulse_PrintItemList
 * @brief Pulse의 항목을 모두 출력한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_PrintItemList(PIF_stPulse *pstOwner)
{
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enNone, "\nFree = %d, Alloc = %d\n",
			pstOwner->__unFreeNext, pstOwner->__unAllocNext);

    for (int index = 0; index < pstOwner->__ucItemSize; index++) {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);
    }
#endif
}

/**
 * @fn pifPulse_PrintItemFree
 * @brief Pulse의 항목중 할당되지 않은 항목을 출력한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_PrintItemFree(PIF_stPulse *pstOwner)
{
	uint8_t index;

#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enNone, "\nFree = %d\n", pstOwner->__unFreeNext);

    if (pstOwner->__unFreeNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unFreeNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);

        index = pstOwner->__pstItems[index].__unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
#endif
}

/**
 * @fn pifPulse_PrintItemAlloc
 * @brief Pulse의 항목중 할당된 항목을 출력한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_PrintItemAlloc(PIF_stPulse *pstOwner)
{
	uint8_t index;

#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enNone, "\nAlloc = %d\n", pstOwner->__unAllocNext);

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unAllocNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);

        index = pstOwner->__pstItems[index].__unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
#endif
}

#endif  // __PIF_DEBUG__
