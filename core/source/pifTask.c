#include <string.h>

#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifTask.h"


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)


static PIF_stTask *s_pstTask = NULL;
static uint8_t s_ucTaskSize;
static uint8_t s_ucTaskPos;

static uint32_t s_aunTable[PIF_TASK_TABLE_SIZE];
static uint8_t s_ucNumber = 0;
static uint8_t s_ucCurrent = 0;


static void _Processing(PIF_stTask *pstOwner, BOOL bRatio)
{
	uint16_t usPeriod;
	uint32_t unTime, unGap;

	if (pstOwner->bPause) return;
	else if (pstOwner->bImmediate) {
		pstOwner->__bRunning = TRUE;
		(*pstOwner->__evtLoop)(pstOwner);
		pstOwner->__bRunning = FALSE;
		pstOwner->bImmediate = FALSE;
		return;
	}

	switch (pstOwner->_enMode) {
	case TM_enAlways:
		pstOwner->__bRunning = TRUE;
		(*pstOwner->__evtLoop)(pstOwner);
		pstOwner->__bRunning = FALSE;
		break;

	case TM_enPeriodUs:
		unTime = (*pif_actTimer1us)();
		unGap = unTime - pstOwner->__unPretime;
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			pstOwner->__unPretime = unTime;
		}
		break;

	case TM_enPeriodMs:
		unTime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
		unGap = unTime - pstOwner->__unPretime;
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			pstOwner->__unPretime = unTime;
		}
		break;

	case TM_enChangeUs:
		unTime = (*pif_actTimer1us)();
		unGap = unTime - pstOwner->__unPretime;
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			usPeriod = (*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			if (usPeriod > 0) pstOwner->_usPeriod = usPeriod;
			pstOwner->__unPretime = unTime;
		}
		break;

	case TM_enChangeMs:
		unTime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
		unGap = unTime - pstOwner->__unPretime;
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			usPeriod = (*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			if (usPeriod > 0) pstOwner->_usPeriod = usPeriod;
			pstOwner->__unPretime = unTime;
		}
		break;

	default:
		if (bRatio) {
#ifdef __PIF_DEBUG__
			unTime = pif_unTimer1sec;
			if (unTime != pstOwner->__unPretime) {
				pstOwner->__fPeriod = 1000000.0 / pstOwner->__unCount;
				pstOwner->__unCount = 0;
				pstOwner->__unPretime = unTime;
			}
#endif
			pstOwner->__bRunning = TRUE;
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
#ifdef __PIF_DEBUG__
			pstOwner->__unCount++;
#endif
		}
		break;
	}
}

/**
 * @fn pifTask_Init
 * @brief 입력된 크기만큼 Task 구조체를 할당하고 초기화한다.
 * @param ucSize Task 갯수
 * @return 성공 여부
 */
BOOL pifTask_Init(uint8_t ucSize)
{
    if (ucSize == 0 || ucSize > 32) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstTask = calloc(sizeof(PIF_stTask), ucSize);
    if (!s_pstTask) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucTaskSize = ucSize;
    s_ucTaskPos = 0;

    memset(s_aunTable, 0, sizeof(s_aunTable));
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:Init(S:%u) EC:%d", ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifTask_Exit
 * @brief Task용 메모리를 반환한다.
 */
void pifTask_Exit()
{
    if (s_pstTask) {
        free(s_pstTask);
        s_pstTask = NULL;
    }
}

/**
 * @fn pifTask_Add
 * @brief Task를 추가한다.
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param evtLoop Task 함수
 * @param pvClient 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_Add(PIF_enTaskMode enMode, uint16_t usPeriod, PIF_evtTaskLoop evtLoop, void *pvClient, BOOL bStart)
{
	uint32_t count, gap, index;
	static int base = 0;

	if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

	if (!evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}

	switch (enMode) {
    case TM_enRatio:
    	if (!usPeriod || usPeriod > 100) {
    		pif_enError = E_enInvalidParam;
            goto fail;
    	}
    	break;

    case TM_enAlways:
    	break;

    case TM_enPeriodMs:
    case TM_enChangeMs:
    	if (!usPeriod) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}
    	break;

    case TM_enPeriodUs:
    case TM_enChangeUs:
    	if (!usPeriod) {
            pif_enError = E_enInvalidParam;
            goto fail;
    	}

    	if (!pif_actTimer1us) {
            pif_enError = E_enCanNotUse;
            goto fail;
        }
    	break;
    }

    switch (enMode) {
    case TM_enRatio:
		count = (PIF_TASK_TABLE_SIZE - 1) * usPeriod + 100;
		gap = 10000L * PIF_TASK_TABLE_SIZE / count;
		if (gap > 100) {
			index = 100 * base;
			for (uint16_t i = 0; i < count / 100; i++) {
				s_aunTable[(index / 100) & PIF_TASK_TABLE_MASK] |= 1 << s_ucTaskPos;
				index += gap;
			}
			base++;
		}
		else {
			enMode = TM_enAlways;
			usPeriod = 100;
		}
    	break;

    case TM_enAlways:
    	enMode = TM_enAlways;
    	usPeriod = 100;
    	break;

    default:
    	break;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    switch (enMode) {
    case TM_enPeriodMs:
    case TM_enChangeMs:
    	pstOwner->__unPretime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    	break;

    case TM_enPeriodUs:
    case TM_enChangeUs:
    	pstOwner->__unPretime = (*pif_actTimer1us)();
    	break;

    default:
    	break;
    }
    pstOwner->_enMode = enMode;
    pstOwner->_usPeriod = usPeriod;
    pstOwner->__evtLoop = evtLoop;
    pstOwner->_pvClient = pvClient;
    pstOwner->bPause = !bStart;

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:Add(P:%uus) EC:%d", usPeriod, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_SetPeriod
 * @brief Task의 주기를 설정한다.
 * @param pstOwner Task 자신
 * @param usPeriod Task 주기 (ms, us)
 */
void pifTask_SetPeriod(PIF_stTask *pstOwner, uint16_t usPeriod)
{
	switch (pstOwner->_enMode) {
	case TM_enPeriodMs:
	case TM_enPeriodUs:
		pstOwner->_usPeriod = usPeriod;
		break;

	default:
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enWarn, "Task:ChangePeriod(P:%u)", usPeriod);
#endif
		break;
	}
}

/**
 * @fn pifTask_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTask_Loop()
{
#ifdef __PIF_DEBUG__
	static uint8_t ucSec = 0;
#endif

	for (int i = s_ucCurrent; i < s_ucTaskPos; i++) {
		s_ucCurrent = i;
		_Processing(&s_pstTask[i], s_aunTable[s_ucNumber] & (1 << i));
	}

	s_ucNumber = (s_ucNumber + 1) & PIF_TASK_TABLE_MASK;
	s_ucCurrent = 0;

#ifdef __PIF_DEBUG__
    if (ucSec != pif_stDateTime.ucSecond) {
    	pifTask_Print();
    	ucSec = pif_stDateTime.ucSecond;
    }
#endif
}

/**
 * @fn pifTask_Yield
 * @brief Task내에서 대기중에 다른 Task를 호출하고자 할 경우에 사용하는 함수이다.
 */
void pifTask_Yield()
{
	PIF_stTask *pstOwner;

	s_ucCurrent++;
	if (s_ucCurrent >= s_ucTaskPos) {
		s_ucNumber = (s_ucNumber + 1) & PIF_TASK_TABLE_MASK;
		s_ucCurrent = 0;
	}
	pstOwner = &s_pstTask[s_ucCurrent];
	if (!pstOwner->__bRunning) {
		_Processing(pstOwner, s_aunTable[s_ucNumber] & (1 << s_ucCurrent));
	}
}

/**
 * @fn pifTask_YieldMs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param usTime
 */
void pifTask_YieldMs(uint32_t unTime)
{
    uint32_t unCurrent = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    uint32_t unTarget = unCurrent + unTime;

    if (!unTime) return;

    if (unTarget < unCurrent) {
    	while (unCurrent <= 0xFFFFFFFF) {
    		pifTask_Yield();
    		unCurrent = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    	}
    }
	while (unCurrent < unTarget) {
		pifTask_Yield();
		unCurrent = 1000L * pif_unTimer1sec + pif_usTimer1ms;
	}
}

/**
 * @fn pifTask_YieldUs
 * @brief loop내에서 지정한 시간동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param usTime
 */
void pifTask_YieldUs(uint32_t unTime)
{
    uint32_t unCurrent = (*pif_actTimer1us)();
    uint32_t unTarget = unCurrent + unTime;

    if (!unTime) return;
    if (!pif_actTimer1us) {
    	pifTask_YieldMs((unTime + 999) / 1000);
    	return;
    }

    if (unTarget < unCurrent) {
    	while (unCurrent <= 0xFFFFFFFF) {
    		pifTask_Yield();
    		unCurrent = (*pif_actTimer1us)();
    	}
    }
	while (unCurrent < unTarget) {
		pifTask_Yield();
		unCurrent = (*pif_actTimer1us)();
	}
}

/**
 * @fn pifTask_YieldPeriod
 * @brief loop내에서 지정된 주기동안 다른 Task를 실행하고자 할 경우에 사용하는 함수이다.
 * @param pstOwner
 */
void pifTask_YieldPeriod(PIF_stTask *pstOwner)
{
	switch (pstOwner->_enMode) {
	case TM_enRatio:
	case TM_enAlways:
		for (int i = 0; i < s_ucTaskPos; i++) pifTask_Yield();
		break;

	case TM_enPeriodMs:
	case TM_enChangeMs:
		pifTask_YieldMs(pstOwner->_usPeriod);
		break;

	case TM_enPeriodUs:
	case TM_enChangeUs:
		pifTask_YieldUs(pstOwner->_usPeriod);
		break;

	default:
		break;
	}
}

#ifdef __PIF_DEBUG__

/**
 * @fn pifTask_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTask_Print()
{
	if (!pif_stLogFlag.bt.Task) return;

	for (int i = 0; i < s_ucTaskPos; i++) {
		PIF_stTask *pstOwner = &s_pstTask[i];
		if (pstOwner->_enMode == TM_enRatio) {
#ifndef __PIF_NO_LOG__
			pifLog_Printf(LT_enInfo, "Task Id=%d Ratio=%d%% Period=%2fus",
					pstOwner->_usPifId, pstOwner->_usPeriod, pstOwner->__fPeriod);
#endif
		}
	}
}

#endif
