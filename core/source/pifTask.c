#include <string.h>

#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifTask.h"


#define PIF_TASK_TABLE_MASK		(PIF_TASK_TABLE_SIZE - 1)


typedef struct _PIF_stTaskBase
{
	// Public Member Variable
	PIF_stTask stOwner;

	// Private Member Variable
	const char *pcName;
	uint8_t ucRatio;
	BOOL bPause;
	uint16_t usPeriod;
	uint32_t unPretime;
	uint32_t unCount;
	float fPeriod;

	// Private Member Function
	PIF_evtTaskLoop __evtLoop;
} PIF_stTaskBase;


static PIF_stTaskBase *s_pstTaskBase = NULL;
static uint8_t s_ucTaskBaseSize;
static uint8_t s_ucTaskBasePos;

static uint8_t s_ucId = 1;
static uint32_t s_aunTable[PIF_TASK_TABLE_SIZE];


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

    s_pstTaskBase = calloc(sizeof(PIF_stTaskBase), ucSize);
    if (!s_pstTaskBase) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucTaskBaseSize = ucSize;
    s_ucTaskBasePos = 0;

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
    if (s_pstTaskBase) {
        free(s_pstTaskBase);
        s_pstTaskBase = NULL;
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
	
    if (s_ucTaskBasePos >= s_ucTaskBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTaskBase *pstBase = &s_pstTaskBase[s_ucTaskBasePos];

    pstBase->stOwner.enMode = TM_enRatio;
    pstBase->stOwner.ucId = s_ucId++;
    pstBase->ucRatio = ucRatio;
    pstBase->unCount = 0;
    pstBase->__evtLoop = evtLoop;
    pstBase->stOwner.pvLoopEach = pvLoopEach;

	int count = PIF_TASK_TABLE_SIZE * ucRatio / 101;
	int gap = PIF_TASK_TABLE_SIZE - count;
	int index = base;
	for (int i = 0; i <= count; i++) {
		s_aunTable[index & PIF_TASK_TABLE_MASK] |= 1 << s_ucTaskBasePos;
		index += gap;
	}
	base++;

	if (pvLoopEach) {
		(*evtLoop)(&pstBase->stOwner);
	}

    s_ucTaskBasePos = s_ucTaskBasePos + 1;
    return &pstBase->stOwner;

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

    if (s_ucTaskBasePos >= s_ucTaskBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTaskBase *pstBase = &s_pstTaskBase[s_ucTaskBasePos];

    pstBase->stOwner.enMode = TM_enPeriodMs;
    pstBase->stOwner.ucId = s_ucId++;
    pstBase->usPeriod = usPeriodMs;
    pstBase->unCount = 0;
    pstBase->__evtLoop = evtLoop;
    pstBase->stOwner.pvLoopEach = pvLoopEach;

	if (pvLoopEach) {
		(*evtLoop)(&pstBase->stOwner);
	}

    s_ucTaskBasePos = s_ucTaskBasePos + 1;
    return &pstBase->stOwner;

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

    if (s_ucTaskBasePos >= s_ucTaskBaseSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTaskBase *pstBase = &s_pstTaskBase[s_ucTaskBasePos];

    pstBase->stOwner.enMode = TM_enPeriodUs;
    pstBase->stOwner.ucId = s_ucId++;
    pstBase->usPeriod = usPeriodUs;
    pstBase->unCount = 0;
    pstBase->__evtLoop = evtLoop;
    pstBase->stOwner.pvLoopEach = pvLoopEach;

	if (pvLoopEach) {
		(*evtLoop)(&pstBase->stOwner);
	}

    s_ucTaskBasePos = s_ucTaskBasePos + 1;
    return &pstBase->stOwner;

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
	((PIF_stTaskBase *)pstOwner)->pcName = pcName;
}

/**
 * @fn pifTask_Pause
 * @brief Task를 일시중지한다.
 * @param pstOwner Task 자신
 */
void pifTask_Pause(PIF_stTask *pstOwner)
{
	((PIF_stTaskBase *)pstOwner)->bPause = TRUE;
}

/**
 * @fn pifTask_Restart
 * @brief 일시중지한 Task를 재개하다.
 * @param pstOwner Task 자신
 */
void pifTask_Restart(PIF_stTask *pstOwner)
{
	((PIF_stTaskBase *)pstOwner)->bPause = FALSE;
}

/**
 * @fn pifTask_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTask_Loop()
{
	PIF_stTaskBase *pstBase;
	uint32_t unTime, unGap;
	static uint8_t ucNumber = 0;

	for (int i = 0; i < s_ucTaskBasePos; i++) {
		pstBase = &s_pstTaskBase[i];
		if (pstBase->bPause) continue;

		switch (pstBase->stOwner.enMode) {
		case TM_enPeriodUs:
			unTime = 1000000L * g_unPerformanceCount / g_unPerformanceMeasure;
			if (unTime < pstBase->unPretime) {
				unGap = 1000000L - pstBase->unPretime + unTime;
			}
			else {
				unGap = unTime - pstBase->unPretime;
			}
			if (unGap >= pstBase->usPeriod) {
				(*pstBase->__evtLoop)(&pstBase->stOwner);
				pstBase->unPretime = unTime;
			}
			break;

		case TM_enPeriodMs:
			unTime = 1000L * pif_unTimer1sec + pif_usTimer1ms;
			if (unTime < pstBase->unPretime) {
				unGap = 60000L - pstBase->unPretime + unTime;
			}
			else {
				unGap = unTime - pstBase->unPretime;
			}
			if (unGap >= pstBase->usPeriod) {
				(*pstBase->__evtLoop)(&pstBase->stOwner);
				pstBase->unPretime = unTime;
			}
			break;

		default:
			if (s_aunTable[ucNumber] & (1 << i)) {
				unTime = pif_unTimer1sec;
				if (unTime != pstBase->unPretime) {
					pstBase->fPeriod = 1000000.0 / pstBase->unCount;
					pstBase->unCount = 0;
					pstBase->unPretime = unTime;
				}
				(*pstBase->__evtLoop)(&pstBase->stOwner);
				pstBase->unCount++;
			}
			break;
		}
	}

	ucNumber = (ucNumber + 1) & PIF_TASK_TABLE_MASK;
}

#ifdef __PIF_DEBUG__

/**
 * @fn pifTask_Print
 * @brief Task 할당 정보를 출력한다.
 */
void pifTask_Print()
{
	if (!pif_stLogFlag.btTask) return;

	for (int i = 0; i < s_ucTaskBasePos; i++) {
		PIF_stTaskBase *pstBase = &s_pstTaskBase[i];
		if (pstBase->stOwner.enMode == TM_enRatio) {
			pifLog_Printf(LT_enInfo, "Task:%s Id=%d Ratio=%d%% Period=%2fus",
					pstBase->pcName ? pstBase->pcName : "Null",
					pstBase->stOwner.ucId, pstBase->ucRatio, pstBase->fPeriod);
		}
	}
}

#endif
