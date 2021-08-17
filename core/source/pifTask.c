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
		if (unTime < pstOwner->__unPretime) {
			unGap = 0xFFFFFFFF - pstOwner->__unPretime + unTime + 1;
		}
		else {
			unGap = unTime - pstOwner->__unPretime;
		}
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			pstOwner->__unPretime = unTime;
		}
		break;

	case TM_enPeriodMs:
		unTime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
		if (unTime < pstOwner->__unPretime) {
			unGap = 60000L - pstOwner->__unPretime + unTime;
		}
		else {
			unGap = unTime - pstOwner->__unPretime;
		}
		if (unGap >= pstOwner->_usPeriod) {
			pstOwner->__bRunning = TRUE;
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__bRunning = FALSE;
			pstOwner->__unPretime = unTime;
		}
		break;

	case TM_enChangeUs:
		unTime = (*pif_actTimer1us)();
		if (unTime < pstOwner->__unPretime) {
			unGap = 0xFFFFFFFF - pstOwner->__unPretime + unTime + 1;
		}
		else {
			unGap = unTime - pstOwner->__unPretime;
		}
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
		if (unTime < pstOwner->__unPretime) {
			unGap = 60000L - pstOwner->__unPretime + unTime;
		}
		else {
			unGap = unTime - pstOwner->__unPretime;
		}
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
 * @fn pifTask_AddRatio
 * @brief Task를 추가한다.
 * @param ucRatio Task 동작 속도(1% ~ 100%) 높을수록 빠르다.
 * @param evtLoop Task 함수
 * @param pvLoopEach 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_AddRatio(uint8_t ucRatio, PIF_evtTaskLoop evtLoop, void *pvLoopEach)
{
	static int base = 0;
	
	if (!ucRatio || ucRatio > 100 || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}
	
    if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    if (ucRatio < 100) {
		pstOwner->_enMode = TM_enRatio;
		pstOwner->__ucRatio = ucRatio;
#ifdef __PIF_DEBUG__
		pstOwner->__unCount = 0;
#endif

		int count = PIF_TASK_TABLE_SIZE * ucRatio / 101;
		int gap = PIF_TASK_TABLE_SIZE - count;
		int index = base;
		for (int i = 0; i <= count; i++) {
			s_aunTable[index & PIF_TASK_TABLE_MASK] |= 1 << s_ucTaskPos;
			index += gap;
		}
		base++;
    }
    else {
		pstOwner->_enMode = TM_enAlways;
		pstOwner->__ucRatio = 100;
    }
	pstOwner->_usPifId = pif_usPifId++;
	pstOwner->__evtLoop = evtLoop;
	pstOwner->pvLoopEach = pvLoopEach;

	if (pvLoopEach) (*evtLoop)(pstOwner);

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:AddRatio(R:%u) EC:%d", ucRatio, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_AddPeriodMs
 * @brief Task를 추가한다.
 * @param usPeriodMs Task 주기를 설정한다. 단위는 1ms.
 * @param evtLoop Task 함수
 * @param pvLoopEach 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_AddPeriodMs(uint16_t usPeriodMs, PIF_evtTaskLoop evtLoop, void *pvLoopEach)
{
	if (!usPeriodMs || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    pstOwner->_enMode = TM_enPeriodMs;
    pstOwner->_usPifId = pif_usPifId++;
    pstOwner->_usPeriod = usPeriodMs;
    pstOwner->__unPretime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    pstOwner->__evtLoop = evtLoop;
    pstOwner->pvLoopEach = pvLoopEach;

	if (pvLoopEach) (*evtLoop)(pstOwner);

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:AddPeriod(P:%ums) EC:%d", usPeriodMs, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_AddPeriodUs
 * @brief Task를 추가한다.
 * @param usPeriodUs Task 주기를 설정한다. 단위는 1us.
 * @param evtLoop Task 함수
 * @param pvLoopEach 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_AddPeriodUs(uint16_t usPeriodUs, PIF_evtTaskLoop evtLoop, void *pvLoopEach)
{
	if (!usPeriodUs || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!pif_actTimer1us) {
        pif_enError = E_enCanNotUse;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    pstOwner->_enMode = TM_enPeriodUs;
    pstOwner->_usPifId = pif_usPifId++;
    pstOwner->_usPeriod = usPeriodUs;
    pstOwner->__unPretime = (*pif_actTimer1us)();
    pstOwner->__evtLoop = evtLoop;
    pstOwner->pvLoopEach = pvLoopEach;

	if (pvLoopEach) (*evtLoop)(pstOwner);

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:AddPeriod(P:%uus) EC:%d", usPeriodUs, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_AddChangeMs
 * @brief Task를 추가한다.
 * @param usPeriodMs Task 초기 주기를 설정한다. 단위는 1ms.
 * @param evtLoop Task 함수
 * @param pvLoopEach 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_AddChangeMs(uint16_t usPeriodMs, PIF_evtTaskLoop evtLoop, void *pvLoopEach)
{
	if (!usPeriodMs || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    pstOwner->_enMode = TM_enChangeMs;
    pstOwner->_usPifId = pif_usPifId++;
    pstOwner->_usPeriod = usPeriodMs;
    pstOwner->__unPretime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    pstOwner->__evtLoop = evtLoop;
    pstOwner->pvLoopEach = pvLoopEach;

	if (pvLoopEach) (*evtLoop)(pstOwner);

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:AddPeriod(P:%ums) EC:%d", usPeriodMs, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_AddChangeUs
 * @brief Task를 추가한다.
 * @param usPeriodUs Task 초기 주기를 설정한다. 단위는 1us.
 * @param evtLoop Task 함수
 * @param pvLoopEach 한 Task에서 통합 관리할 경우에는 NULL을 전달하고 개별관리하고자 한다면 해당 구조체의 포인터를 전달한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_AddChangeUs(uint16_t usPeriodUs, PIF_evtTaskLoop evtLoop, void *pvLoopEach)
{
	if (!usPeriodUs || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}

    if (s_ucTaskPos >= s_ucTaskSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    if (!pif_actTimer1us) {
        pif_enError = E_enCanNotUse;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTask[s_ucTaskPos];

    pstOwner->_enMode = TM_enChangeUs;
    pstOwner->_usPifId = pif_usPifId++;
    pstOwner->_usPeriod = usPeriodUs;
    pstOwner->__unPretime = (*pif_actTimer1us)();
    pstOwner->__evtLoop = evtLoop;
    pstOwner->pvLoopEach = pvLoopEach;

	if (pvLoopEach) (*evtLoop)(pstOwner);

    s_ucTaskPos = s_ucTaskPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "Task:AddPeriod(P:%uus) EC:%d", usPeriodUs, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifTask_SetName
 * @brief Task의 이름을 지정한다.
 * @param pstOwner Task 자신
 * @param pcName Task 이름
 */
void pifTask_SetName(PIF_stTask *pstOwner, const char *pcName)
{
	pstOwner->__pcName = pcName;
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
	for (int i = s_ucCurrent; i < s_ucTaskPos; i++) {
		s_ucCurrent = i;
		_Processing(&s_pstTask[i], s_aunTable[s_ucNumber] & (1 << i));
	}

	s_ucNumber = (s_ucNumber + 1) & PIF_TASK_TABLE_MASK;
	s_ucCurrent = 0;
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
void pifTask_YieldMs(uint16_t usTime)
{
    uint32_t unCurrent = 1000L * pif_unTimer1sec + pif_usTimer1ms;
    uint32_t unTarget = unCurrent + usTime;

    if (!usTime) return;

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
void pifTask_YieldUs(uint16_t usTime)
{
    uint32_t unCurrent = (*pif_actTimer1us)();
    uint32_t unTarget = unCurrent + usTime;

    if (!usTime || !pif_actTimer1us) return;

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
	int i, count;

	switch (pstOwner->_enMode) {
	case TM_enRatio:
		count = PIF_TASK_TABLE_SIZE - PIF_TASK_TABLE_SIZE * pstOwner->__ucRatio / 101;
		for (i = 0; i < s_ucTaskPos * count; i++) pifTask_Yield();
		break;

	case TM_enPeriodMs:
		pifTask_YieldMs(pstOwner->_usPeriod);
		break;

	case TM_enPeriodUs:
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
			pifLog_Printf(LT_enInfo, "Task:%s Id=%d Ratio=%d%% Period=%2fus",
					pstOwner->__pcName ? pstOwner->__pcName : "Null",
					pstOwner->_usPifId, pstOwner->__ucRatio, pstOwner->__fPeriod);
#endif
		}
	}
}

#endif
