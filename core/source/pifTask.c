#include <string.h>

#if USE_LOG == 1
#include "pifLog.h"
#endif
#include "pifTask.h"


#define TABLE_SIZE		32
#define TABLE_MASK		(TABLE_SIZE - 1)


static PIF_stTask *s_pstTaskArray;
static uint8_t s_ucTaskArraySize;
static uint8_t s_ucTaskArrayPos;

static uint8_t s_ucId = 1;
static uint32_t s_aunTable[TABLE_SIZE];

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

    s_pstTaskArray = calloc(sizeof(PIF_stTask), ucSize);
    if (!s_pstTaskArray) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucTaskArraySize = ucSize;
    s_ucTaskArrayPos = 0;

    memset(s_aunTable, 0, sizeof(s_aunTable));
    return TRUE;

fail:
#if USE_LOG == 1
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
    if (s_pstTaskArray) {
        free(s_pstTaskArray);
        s_pstTaskArray = NULL;
    }
}

/**
 * @fn pifTask_Add
 * @brief Task를 추가한다.
 * @param ucRatio Task 동작 속도(1% ~ 100%) 높을수록 빠르다.
 * @param evtLoop Task 함수
 * @param pvParam Task 호출시 전달되는 파라메터 포인터.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifTask_Add(uint8_t ucRatio, PIF_evtTaskLoop evtLoop, void *pvParam)
{
	static int base = 0;
	
	if (!ucRatio || ucRatio > 100 || !evtLoop) {
        pif_enError = E_enInvalidParam;
        goto fail;
	}
	
    if (s_ucTaskArrayPos >= s_ucTaskArraySize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    PIF_stTask *pstOwner = &s_pstTaskArray[s_ucTaskArrayPos];

    pstOwner->ucId = s_ucId++;
    pstOwner->ucRatio = ucRatio;
    pstOwner->__unCount = 0;
    pstOwner->__evtLoop = evtLoop;
    pstOwner->pvParam = pvParam;

	int count = TABLE_SIZE * ucRatio / 101;
	int gap = TABLE_SIZE - count;
	int index = base;
	for (int i = 0; i <= count; i++) {
		s_aunTable[index & TABLE_MASK] |= 1 << s_ucTaskArrayPos;
		index += gap;
	}
	base++;

	if (pvParam) {
		pstOwner->__bTaskLoop = TRUE;
		(*evtLoop)(pstOwner);
		pstOwner->__bTaskLoop = FALSE;
	}

    s_ucTaskArrayPos = s_ucTaskArrayPos + 1;
    return pstOwner;

fail:
#if USE_LOG == 1
	pifLog_Printf(LT_enError, "Task:Add(R:%u) EC:%d", ucRatio, pif_enError);
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
	pstOwner->pcName = pcName;
}

/**
 * @fn pifTask_Pause
 * @brief Task를 일시중지한다.
 * @param pstOwner Task 자신
 */
void pifTask_Pause(PIF_stTask *pstOwner)
{
	pstOwner->__bPause = TRUE;
}

/**
 * @fn pifTask_Restart
 * @brief 일시중지한 Task를 재개하다.
 * @param pstOwner Task 자신
 */
void pifTask_Restart(PIF_stTask *pstOwner)
{
	pstOwner->__bPause = FALSE;
}

/**
 * @fn pifTask_Loop
 * @brief Main loop에서 수행해야 하는 Task 함수이다.
 */
void pifTask_Loop()
{
	PIF_stTask *pstOwner;
	uint32_t unTime = pif_unTimer1sec;
	static uint8_t ucNumber = 0;

	for (int i = 0; i < s_ucTaskArrayPos; i++) {
		pstOwner = &s_pstTaskArray[i];
		if (!pstOwner->__bPause && (s_aunTable[ucNumber] & (1 << i))) {
			if (unTime != pstOwner->__unPretime) {
				pstOwner->__fPeriod = 1000000.0 / pstOwner->__unCount;
				pstOwner->usPeriodUs = pstOwner->__fPeriod + 0.5;
				pstOwner->__unCount = 0;
				pstOwner->__unPretime = unTime;
			}
			(*pstOwner->__evtLoop)(pstOwner);
			pstOwner->__unCount++;
		}
	}

	ucNumber = (ucNumber + 1) & TABLE_MASK;
}

#ifdef __PIF_DEBUG__

/**
 * @fn pifTask_Print
 * @brief Task 할당 정보를 표시한다.
 */
void pifTask_Print()
{
	if (!pif_stLogFlag.btTask) return;

	for (int i = 0; i < s_ucTaskArrayPos; i++) {
		PIF_stTask *pstOwner = &s_pstTaskArray[i];
		pifLog_Printf(LT_enInfo, "Task:%s Id=%d Ratio=%d%% Period=%2fus", pstOwner->pcName ? pstOwner->pcName : "Null",
				pstOwner->ucId, pstOwner->ucRatio, pstOwner->__fPeriod);
	}
}

#endif
