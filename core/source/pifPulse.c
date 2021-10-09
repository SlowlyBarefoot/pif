#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifPulse.h"


#define PIF_PULSE_INDEX_NULL   0xFF


/**
 * @fn pifPulse_Create
 * @brief Pulse를 추가한다.
 * @param usPifId
 * @param unPeriodUs
 * @return Pulse 구조체 포인터를 반환한다.
 */
PIF_stPulse *pifPulse_Create(PifId usPifId, uint32_t unPeriodUs)
{
	PIF_stPulse *pstOwner = NULL;

    if (!unPeriodUs) {
        pif_error = E_INVALID_PARAM;
        goto fail;
    }

    pstOwner = calloc(sizeof(PIF_stPulse), 1);
    if (!pstOwner) {
    	pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    if (!pifDList_Init(&pstOwner->__items)) goto fail;

    pstOwner->_unPeriodUs = unPeriodUs;
    return pstOwner;

fail:
	if (pstOwner) free(pstOwner);
    return NULL;
}

/**
 * @fn pifPulse_Destroy
 * @brief Pulse용 메모리를 반환한다.
 * @param ppstOwner Pulse 자신
 */
void pifPulse_Destroy(PIF_stPulse **ppstOwner)
{
	if (*ppstOwner) {
		pifDList_Clear(&(*ppstOwner)->__items);
		free(*ppstOwner);
		*ppstOwner = NULL;
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
    PIF_stPulseItem *pstItem = (PIF_stPulseItem *)pifDList_AddLast(&pstOwner->__items, sizeof(PIF_stPulseItem));
    if (!pstItem) return NULL;

    pstItem->_enType = enType;
    pstItem->__evtFinish = NULL;
    pstItem->__pvFinishIssuer = NULL;

    pstItem->_enStep = PS_enStop;
    return pstItem;
}

/**
 * @fn pifPulse_RemoveItem
 * @brief Pulse 항목을 삭제한다.
 * @param pstOwner Pulse 자신
 * @param pstItem Pulse 항목 포인터
 */
void pifPulse_RemoveItem(PIF_stPulse *pstOwner, PIF_stPulseItem *pstItem)
{
    if (pstItem->_enStep == PS_enRemove) return;

	PIF_DListIterator it = pifDList_Begin(&pstOwner->__items);
	while (it) {
		if (pstItem == (PIF_stPulseItem *)it->data) {
			pifDList_Remove(&pstOwner->__items, it);
			break;
		}

		it = pifDList_Next(it);
	}
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
		pif_error = E_INVALID_PARAM;
	    return FALSE;
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
    if (!pstOwner) return;

	PIF_DListIterator it = pifDList_Begin(&pstOwner->__items);
	while (it) {
		PIF_stPulseItem *pstItem = (PIF_stPulseItem *)it->data;

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

		it = pifDList_Next(it);
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

	PIF_DListIterator it = pifDList_Begin(&pstOwner->__items);
	while (it) {
		PIF_stPulseItem *pstItem = (PIF_stPulseItem *)it->data;

		if (pstItem->_enType != PT_enPwm) {
			if (pstItem->__bEvent) {
				pstItem->__bEvent = FALSE;

				if (pstItem->__evtFinish) (*pstItem->__evtFinish)(pstItem->__pvFinishIssuer);
			}
		}

		it = pifDList_Next(it);
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
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, pstOwner, bStart);
}
