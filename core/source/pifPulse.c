#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifPulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


static PIF_stPulse *s_pstPulse = NULL;
static uint8_t s_ucPulseSize;
static uint8_t s_ucPulsePos;


static void _TaskCommon(PIF_stPulse *pstOwner)
{
	uint8_t index;
	uint32_t unTime, unGap;
	PIF_stPulseItem *pstItem;

	index = pstOwner->__unAllocNext;
	while (index != PIF_PULSE_INDEX_NULL) {
		pstItem = &pstOwner->__pstItems[index];

		if (pstItem->_enType == PT_enPwm) {
			if (pstItem->__btPwmState == ON) {
				if (pif_stPerformance._unCount > pif_stPerformance._unCurrent) {
					unTime = PIF_PERFORMANCE_PERIOD_US;
				}
				else {
					unTime = PIF_PERFORMANCE_PERIOD_US * pif_stPerformance._unCount / pif_stPerformance._unCurrent;
				}
				if (unTime < pstItem->__unPretime) {
					unGap = PIF_PERFORMANCE_PERIOD_US - pstItem->__unPretime + unTime;
				}
				else {
					unGap = unTime - pstItem->__unPretime;
				}
				if (unGap >= pstItem->__unPwmGap) {
					(*pstItem->__actPwm)(OFF);
					pstItem->__btPwmState = OFF;
				}
			}
		}
		else {
			if (pstItem->__btEvent) {
				pstItem->__btEvent = FALSE;

				if (pstItem->__evtFinish) (*pstItem->__evtFinish)(pstItem->__pvFinishIssuer);
			}
		}

		index = pstItem->__unNext;
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

    s_pstPulse = calloc(sizeof(PIF_stPulse), ucSize);
    if (!s_pstPulse) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucPulseSize = ucSize;
    s_ucPulsePos = 0;
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

    if (s_pstPulse) {
		for (int i = 0; i < s_ucPulsePos; i++) {
			pstOwner = &s_pstPulse[i];
			if (pstOwner->__pstItems) {
				free(pstOwner->__pstItems);
				pstOwner->__pstItems = NULL;
			}
		}
        free(s_pstPulse);
        s_pstPulse = NULL;
    }
}

/**
 * @fn pifPulse_Add
 * @brief Pulse를 추가한다.
 * @param usPifId
 * @param ucSize Pulse 항목 크기
 * @param unPeriodUs
 * @return Pulse 구조체 포인터를 반환한다.
 */
PIF_stPulse *pifPulse_Add(PIF_usId usPifId, uint8_t ucSize, uint32_t unPeriodUs)
{
    if (ucSize >= PIF_PULSE_INDEX_NULL) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}
    if (s_ucPulsePos >= s_ucPulseSize) {
		pif_enError = E_enOverflowBuffer;
		goto fail;
	}

    PIF_stPulse *pstOwner = &s_pstPulse[s_ucPulsePos];

    if (usPifId == PIF_ID_AUTO) usPifId = g_usPifId++;
    pstOwner->_usPifId = usPifId;
    pstOwner->__pstItems = calloc(sizeof(PIF_stPulseItem), ucSize);
    if (!pstOwner->__pstItems) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }

    pstOwner->_unPeriodUs = unPeriodUs;
    pstOwner->__ucItemSize = ucSize;

    pstOwner->__unFreeNext = 0;
    pstOwner->__unAllocNext = PIF_PULSE_INDEX_NULL;
    for (int i = 0; i < ucSize - 1; i++) {
    	pstOwner->__pstItems[i].__unPrev = PIF_PULSE_INDEX_NULL;
    	pstOwner->__pstItems[i].__unNext = i + 1;
    }
    pstOwner->__pstItems[ucSize - 1].__unPrev = PIF_PULSE_INDEX_NULL;
    pstOwner->__pstItems[ucSize - 1].__unNext = PIF_PULSE_INDEX_NULL;

    s_ucPulsePos = s_ucPulsePos + 1;
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
    	pstItem->__btEvent = FALSE;

        if (pstItem->_enType == PT_enPwm) {
    		(*pstItem->__actPwm)(ON);
    		pstItem->__btPwmState = ON;
			if (pif_stPerformance._unCount > pif_stPerformance._unCurrent) {
				pstItem->__unPretime = PIF_PERFORMANCE_PERIOD_US;
			}
			else {
				pstItem->__unPretime = PIF_PERFORMANCE_PERIOD_US * pif_stPerformance._unCount / pif_stPerformance._unCurrent;
			}
        }
    }
    pstItem->unTarget = unTarget;
    pstItem->__unCurrent = unTarget;
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
}

/**
 * @fn pifPulse_SetPwmDuty
 * @brief Pulse 항목의 이동 pulse를 재설정한다.
 * @param pstItem Pulse 항목 포인터
 * @param usDuty 이동 pulse수. 단, 현재 동작에는 영향이 없고 다음 동작부터 변경된다.
 */
void pifPulse_SetPwmDuty(PIF_stPulseItem *pstItem, uint16_t usDuty)
{
	pstItem->__unPwmGap = pstItem->__pstOwner->_unPeriodUs * pstItem->unTarget * usDuty / PIF_PWM_MAX_DUTY;
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
			if (!pstItem->__unCurrent) {
				switch (pstItem->_enType) {
				case PT_enOnce:
					pstItem->_enStep = PS_enStop;
					pstItem->__btEvent = TRUE;
					break;

				case PT_enRepeat:
					pstItem->__unCurrent = pstItem->unTarget;
					pstItem->__btEvent = TRUE;
					break;

				case PT_enPwm:
					pstItem->__unCurrent = pstItem->unTarget;
					(*pstItem->__actPwm)(ON);
					pstItem->__btPwmState = ON;
					if (pif_stPerformance._unCount > pif_stPerformance._unCurrent) {
						pstItem->__unPretime = PIF_PERFORMANCE_PERIOD_US;
					}
					else {
						pstItem->__unPretime = PIF_PERFORMANCE_PERIOD_US * pif_stPerformance._unCount / pif_stPerformance._unCurrent;
					}
					break;
				}
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

/**
 * @fn pifPulse_taskAll
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 */
void pifPulse_taskAll(PIF_stTask *pstTask)
{
	(void) pstTask;

    for (int i = 0; i < s_ucPulsePos; i++) {
        PIF_stPulse *pstOwner = &s_pstPulse[i];
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
	PIF_stPulse *pstOwner = pstTask->pvLoopEach;

	if (pstOwner->__enTaskLoop != TL_enEach) {
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

