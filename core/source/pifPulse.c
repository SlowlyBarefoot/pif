#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifPulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


/**
 * @fn pifPulse_Init
 * @brief Pulse를 추가한다.
 * @param usPifId
 * @param ucSize Pulse 항목 크기
 * @param unPeriodUs
 * @return Pulse 구조체 포인터를 반환한다.
 */
PIF_stPulse *pifPulse_Init(PIF_usId usPifId, uint8_t ucSize, uint32_t unPeriodUs)
{
	PIF_stPulse *pstOwner = NULL;

    if (!ucSize || !unPeriodUs) {
        pif_enError = E_enInvalidParam;
        goto fail;
    }

    pstOwner = calloc(sizeof(PIF_stPulse), 1);
    if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->__pstItems = calloc(sizeof(PIF_stPulseItem), ucSize);
    if (!pstOwner->__pstItems) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->_unPeriodUs = unPeriodUs;
    pstOwner->_ucItemSize = ucSize;
    pstOwner->_ucItemCount = 0;

    pstOwner->__unFreeNext = 0;
    pstOwner->__unAllocNext = PIF_PULSE_INDEX_NULL;
    for (int i = 0; i < ucSize - 1; i++) {
    	pstOwner->__pstItems[i].__unPrev = PIF_PULSE_INDEX_NULL;
    	pstOwner->__pstItems[i].__unNext = i + 1;
    }
    pstOwner->__pstItems[ucSize - 1].__unPrev = PIF_PULSE_INDEX_NULL;
    pstOwner->__pstItems[ucSize - 1].__unNext = PIF_PULSE_INDEX_NULL;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Pulse:Init(SZ:%u P:%lu) EC:%d", ucSize, unPeriodUs, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifPulse_Exit
 * @brief Pulse용 메모리를 반환한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_Exit(PIF_stPulse *pstOwner)
{
	if (pstOwner) {
		if (pstOwner->__pstItems) {
			free(pstOwner->__pstItems);
			pstOwner->__pstItems = NULL;
		}
		free(pstOwner);
	}
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
    PIF_stPulseItem *pstItem = &pstOwner->__pstItems[index];
    pstItem->__pstOwner = pstOwner;
    pstItem->_enType = enType;
    pstItem->__evtFinish = NULL;
    pstItem->__pvFinishIssuer = NULL;
    pstItem->__unIndex = index;

    pstItem->_enStep = PS_enStop;

    pstOwner->__unFreeNext = pstItem->__unNext;

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) {
    	pstOwner->__unAllocNext = index;
    	pstItem->__unNext = PIF_PULSE_INDEX_NULL;
    	pstItem->__unPrev = PIF_PULSE_INDEX_NULL;
    }
    else {
    	pstItem->__unNext = pstOwner->__unAllocNext;
    	pstItem->__unPrev = PIF_PULSE_INDEX_NULL;
        pstOwner->__unAllocNext = index;
        pstOwner->__pstItems[pstItem->__unNext].__unPrev = index;
    }

    pstOwner->_ucItemCount++;

    return pstItem;

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
 * @param pstItem Pulse 항목 포인터
 */
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstItem)
{
	uint8_t index;

    if (pstItem->_enStep == PS_enRemove) return;

    index = pstItem->__unNext;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].__unPrev = pstItem->__unPrev;
    }
    index = pstItem->__unPrev;
    if (index != PIF_PULSE_INDEX_NULL) {
    	pstOwner->__pstItems[index].__unNext = pstItem->__unNext;
    }
    else {
    	pstOwner->__unAllocNext = pstItem->__unNext;
    }

    pstItem->__unNext = pstOwner->__unFreeNext;
    pstItem->__unPrev = PIF_PULSE_INDEX_NULL;
    pstItem->_enStep = PS_enRemove;
    pstOwner->__unFreeNext = pstItem->__unIndex;

    if (pstOwner->_ucItemCount) pstOwner->_ucItemCount--;
}

/**
 * @fn pifPulse_StartItem
 * @brief Pulse 항목을 시작한다.
 * @param pstItem Pulse 항목 포인터
 * @param unPulse 이동 pulse 수
 * @return
 */
BOOL pifPulse_StartItem(PIF_stPulseItem *pstItem, uint32_t unTarget)
{
	if (!unTarget) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (pstItem->_enStep == PS_enStop) {
    	pstItem->_enStep = PS_enRunning;
    	pstItem->__bEvent = FALSE;
    }
    pstItem->unTarget = unTarget;
    pstItem->__unCurrent = unTarget;

    if (pstItem->_enType == PT_enPwm) {
    	pstItem->__unPwmDuty = 0;
    }
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Pulse:StartItem(P:%d) EC:%d", unTarget, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifPulse_StopItem
 * @brief Pulse 항목을 종료한다.
 * @param pstItem Pulse 항목 포인터
 */
void pifPulse_StopItem(PIF_stPulseItem *pstItem)
{
	pstItem->__unCurrent = 0;
	pstItem->_enStep = PS_enStop;
	if (pstItem->_enType == PT_enPwm) {
		(*pstItem->__actPwm)(OFF);
	}
}

/**
 * @fn pifPulse_SetPwmDuty
 * @brief Pulse 항목의 이동 pulse를 재설정한다.
 * @param pstItem Pulse 항목 포인터
 * @param usDuty 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifPulse_SetPwmDuty(PIF_stPulseItem *pstItem, uint16_t usDuty)
{
	pstItem->__unPwmDuty = pstItem->unTarget * usDuty / PIF_PWM_MAX_DUTY;
	if (pstItem->__unPwmDuty == pstItem->unTarget) {
		(*pstItem->__actPwm)(ON);
	}
}

/**
 * @fn pifPulse_RemainItem
 * @brief Pulse 항목의 남은 pulse 수를 얻는다.
 * @param pstItem Pulse 항목 포인터
 * @return 남은 pulse 수를 반환한다.
 */
uint32_t pifPulse_RemainItem(PIF_stPulseItem *pstItem)
{
	if (pstItem->_enStep != PS_enRunning) return 0;
	else return pstItem->__unCurrent;
}

/**
 * @fn pifPulse_ElapsedItem
 * @brief Pulse 항목의 경과한 pulse 수를 얻는다.
 * @param pstItem Pulse 항목 포인터
 * @return 경과한 pulse 수를 반환한다.
 */
uint32_t pifPulse_ElapsedItem(PIF_stPulseItem *pstItem)
{
	if (pstItem->_enStep != PS_enRunning) return 0;
	else return pstItem->unTarget - pstItem->__unCurrent;
}

/**
 * @fn pifPulse_sigTick
 * @brief Pulse를 발생하는 Interrupt 함수에서 호출한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_sigTick(PIF_stPulse *pstOwner)
{
	PIF_stPulseItem *pstItem;

    if (!pstOwner) return;

	uint8_t index = pstOwner->__unAllocNext;
    while (index != PIF_PULSE_INDEX_NULL) {
        pstItem = &pstOwner->__pstItems[index];

		if (pstItem->__unCurrent) {
			pstItem->__unCurrent--;
			switch (pstItem->_enType) {
			case PT_enOnce:
				if (!pstItem->__unCurrent) {
					pstItem->_enStep = PS_enStop;
					pstItem->__bEvent = TRUE;
				}
				break;

			case PT_enRepeat:
				if (!pstItem->__unCurrent) {
					pstItem->__unCurrent = pstItem->unTarget;
					pstItem->__bEvent = TRUE;
				}
				break;

			case PT_enPwm:
				if (pstItem->__unPwmDuty != pstItem->unTarget) {
					if (!pstItem->__unCurrent) {
						(*pstItem->__actPwm)(OFF);
						pstItem->__unCurrent = pstItem->unTarget;
					}
					if (pstItem->__unCurrent == pstItem->__unPwmDuty) {
						(*pstItem->__actPwm)(ON);
					}
				}
				else {
					if (!pstItem->__unCurrent) {
						pstItem->__unCurrent = pstItem->unTarget;
					}
				}
				break;
			}
		}

        index = pstItem->__unNext;
    }
}

/**
 * @fn pifPulse_AttachAction
 * @brief
 * @param pstItem Pulse 항목 포인터
 * @param actPwm 연결시킬 Action
 */
void pifPulse_AttachAction(PIF_stPulseItem *pstItem, PIF_actPulsePwm actPwm)
{
	pstItem->__actPwm = actPwm;
}

/**
 * @fn pifPulse_AttachEvtFinish
 * @brief Pulse 항목의 이동 완료시 발생시킬 이벤트를 연결한다.
 * @param pstItem Pulse 항목 포인터
 * @param evtFinish 연결시킬 이벤트
 * @param pvIssuer 이벤트 발생시 전달할 발행자
 */
void pifPulse_AttachEvtFinish(PIF_stPulseItem *pstItem, PIF_evtPulseFinish evtFinish, void *pvIssuer)
{
	pstItem->__evtFinish = evtFinish;
	pstItem->__pvFinishIssuer = pvIssuer;
}

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	PIF_stPulse *pstOwner = pstTask->_pvClient;
	PIF_stPulseItem *pstItem;
	uint8_t index;

	index = pstOwner->__unAllocNext;
	while (index != PIF_PULSE_INDEX_NULL) {
		pstItem = &pstOwner->__pstItems[index];

		if (pstItem->_enType != PT_enPwm) {
			if (pstItem->__bEvent) {
				pstItem->__bEvent = FALSE;

				if (pstItem->__evtFinish) (*pstItem->__evtFinish)(pstItem->__pvFinishIssuer);
			}
		}

		index = pstItem->__unNext;
	}
	return 0;
}

/**
 * @fn pifPulse_AttachTask
 * @brief Task를 추가한다.
 * @param pstOwner
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifPulse_AttachTask(PIF_stPulse *pstOwner, PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTask_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
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
    return count == pstOwner->_ucItemSize;
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

    for (int index = 0; index < pstOwner->_ucItemSize; index++) {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);
    }
#else
    (void)pstOwner;
#endif
}

/**
 * @fn pifPulse_PrintItemFree
 * @brief Pulse의 항목중 할당되지 않은 항목을 출력한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_PrintItemFree(PIF_stPulse *pstOwner)
{
#ifndef __PIF_NO_LOG__
	uint8_t index;

	pifLog_Printf(LT_enNone, "\nFree = %d\n", pstOwner->__unFreeNext);

    if (pstOwner->__unFreeNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unFreeNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);

        index = pstOwner->__pstItems[index].__unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
#else
    (void)pstOwner;
#endif
}

/**
 * @fn pifPulse_PrintItemAlloc
 * @brief Pulse의 항목중 할당된 항목을 출력한다.
 * @param pstOwner Pulse 자신
 */
void pifPulse_PrintItemAlloc(PIF_stPulse *pstOwner)
{
#ifndef __PIF_NO_LOG__
	uint8_t index;

	pifLog_Printf(LT_enNone, "\nAlloc = %d\n", pstOwner->__unAllocNext);

    if (pstOwner->__unAllocNext == PIF_PULSE_INDEX_NULL) return;

    index = pstOwner->__unAllocNext;
    do {
    	pifLog_Printf(LT_enNone, "\n  %d, Next = %d, Prev = %d",
    			index, pstOwner->__pstItems[index].__unNext, pstOwner->__pstItems[index].__unPrev);

        index = pstOwner->__pstItems[index].__unNext;
    } while (index != PIF_PULSE_INDEX_NULL);
#else
    (void)pstOwner;
#endif
}

#endif  // __PIF_DEBUG__

