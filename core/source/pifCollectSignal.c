#ifdef __PIF_COLLECT_SIGNAL__

#include <string.h>

#include "pifCollectSignal.h"
#include "pifLog.h"


#define COLLECT_SIGNAL_DEVICE_MAX		94


typedef enum _PIF_enCollectSignalStep
{
	CSS_enIdle		= 0,
	CSS_enCollect	= 1,
	CSS_enSendLog	= 2
} PIF_enCollectSignalStep;


typedef struct _PIF_stCollectSignalDevice
{
	PIF_usId usPifId;
	PIF_enCollectSignalVarType enVarType;
	uint16_t usSize;
	uint16_t usInitialValue;
	char pcReference[10];
} PIF_stCollectSignalDevice;

typedef struct _PIF_stCollectSignal
{
	const char *c_pcModuleName;
	PIF_enCollectSignalMethod enMethod;
	PIF_enCollectSignalStep enStep;
	uint8_t ucDeviceCount;
	uint32_t unTimer1ms;
	PIF_stRingBuffer *pstBuffer;
	PIF_stCollectSignalDevice stDevice[COLLECT_SIGNAL_DEVICE_MAX];
	PIF_fnCollectSignalDevice afnDevice[32];
} PIF_stCollectSignal;


static PIF_stCollectSignal s_stCollectSignal;


static void _PrintHeader()
{
	const char *pc_acVarType[] =
	{
		"event",
		"integer",
		"parameter",
		"real",
		"reg",
		"supply0",
		"supply1",
		"time",
		"tri",
		"triand",
		"trior",
		"triReg",
		"tri0",
		"tri1",
		"wand",
		"wire",
		"wor"
	};
	int i;

	pifLog_Printf(LT_enVcd, "\n$date\n");
	pifLog_Printf(LT_enVcd, "\t%u-%u-%u %u:%u:%u\n", pif_stDateTime.usYear, pif_stDateTime.ucMinute, pif_stDateTime.ucDay,
			pif_stDateTime.ucHour, pif_stDateTime.ucMinute, pif_stDateTime.ucSec);
	pifLog_Printf(LT_enVcd, "$end\n");

	pifLog_Printf(LT_enVcd, "$version\n");
	pifLog_Printf(LT_enVcd, "\tPIF collect signal\n");
	pifLog_Printf(LT_enVcd, "$end\n");

	pifLog_Printf(LT_enVcd, "$timescale 1ms $end\n");

	pifLog_Printf(LT_enVcd, "$scope module %s $end\n", s_stCollectSignal.c_pcModuleName);
	for (i = 0; i < s_stCollectSignal.ucDeviceCount; i++) {
		if (s_stCollectSignal.stDevice[i].usSize == 1) {
			pifLog_Printf(LT_enVcd, "$var %s 1 %c %s $end\n", pc_acVarType[s_stCollectSignal.stDevice[i].enVarType], '!' + i,
					s_stCollectSignal.stDevice[i].pcReference);
		}
		else {
			pifLog_Printf(LT_enVcd, "$var %s %u %c %s [%u:0] $end\n", pc_acVarType[s_stCollectSignal.stDevice[i].enVarType],
					s_stCollectSignal.stDevice[i].usSize, '!' + i, s_stCollectSignal.stDevice[i].pcReference,
					s_stCollectSignal.stDevice[i].usSize - 1);
		}
	}
	pifLog_Printf(LT_enVcd, "$upscope $end\n");
	pifLog_Printf(LT_enVcd, "$enddefinitions $end\n");

	pifLog_Printf(LT_enVcd, "$dumpvars\n");
	for (i = 0; i < s_stCollectSignal.ucDeviceCount; i++) {
		if (s_stCollectSignal.stDevice[i].usSize == 1) {
			pifLog_Printf(LT_enVcd, "%u%c\n", s_stCollectSignal.stDevice[i].usInitialValue, '!' + i);
		}
		else {
			pifLog_Printf(LT_enVcd, "b%b %c\n", s_stCollectSignal.stDevice[i].usInitialValue, '!' + i);
		}
	}
	pifLog_Printf(LT_enVcd, "$end\n");
}

/**
 * @fn pifCollectSignal_Init
 * @brief CollectSignal 구조체 초기화한다.
 * @param c_pcModuleName
 */
void pifCollectSignal_Init(const char *c_pcModuleName)
{
	s_stCollectSignal.c_pcModuleName = c_pcModuleName;
	s_stCollectSignal.enMethod = CSM_enRealTime;
	s_stCollectSignal.enStep = CSS_enIdle;
	s_stCollectSignal.pstBuffer = NULL;
	s_stCollectSignal.ucDeviceCount = 0;
	memset(s_stCollectSignal.afnDevice, 0, sizeof(s_stCollectSignal.afnDevice));
}

/**
 * @fn pifCollectSignal_InitHeap
 * @brief CollectSignal 구조체 초기화한다.
 * @param c_pcModuleName
 * @param usSize
 * @return
 */
BOOL pifCollectSignal_InitHeap(const char *c_pcModuleName, uint16_t usSize)
{
	pifCollectSignal_Init(c_pcModuleName);

	s_stCollectSignal.pstBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, usSize);
	if (!s_stCollectSignal.pstBuffer) return FALSE;
	pifRingBuffer_ChopsOffChar(s_stCollectSignal.pstBuffer, '#');
	s_stCollectSignal.enMethod = CSM_enBuffer;
	return TRUE;
}

/**
 * @fn pifCollectSignal_InitStatic
 * @brief CollectSignal 구조체 초기화한다.
 * @param c_pcModuleName
 * @param usSize
 * @param pcBuffer
 * @return
 */
BOOL pifCollectSignal_InitStatic(const char *c_pcModuleName, uint16_t usSize, uint8_t *pucBuffer)
{
	pifCollectSignal_Init(c_pcModuleName);

	s_stCollectSignal.pstBuffer = pifRingBuffer_InitStatic(PIF_ID_AUTO, usSize, pucBuffer);
	if (!s_stCollectSignal.pstBuffer) return FALSE;
	pifRingBuffer_ChopsOffChar(s_stCollectSignal.pstBuffer, '#');
	s_stCollectSignal.enMethod = CSM_enBuffer;
	return TRUE;
}

/**
 * @fn pifCollectSignal_Exit
 * @brief CollectSignal 구조체를 파기하다.
 */
void pifCollectSignal_Exit()
{
	if (s_stCollectSignal.pstBuffer) {
		pifRingBuffer_Exit(s_stCollectSignal.pstBuffer);
	}
}

/**
 * @fn pifCollectSignal_ChangeFlag
 * @brief
 * @param pucFlag
 * @param ucIndex
 * @param ucFlag
 */
void pifCollectSignal_ChangeFlag(uint8_t *pucFlag, uint8_t ucIndex, uint8_t ucFlag)
{
	if (ucIndex & 1) {
		pucFlag[ucIndex / 2] &= 0x0F;
		if (ucFlag) pucFlag[ucIndex / 2] |= ucFlag << 4;
	}
	else {
		pucFlag[ucIndex / 2] &= 0xF0;
		if (ucFlag) pucFlag[ucIndex / 2] |= ucFlag;
	}
}

/**
 * @fn pifCollectSignal_ChangeMethod
 * @brief
 * @param enMethod
 * @return
 */
BOOL pifCollectSignal_ChangeMethod(PIF_enCollectSignalMethod enMethod)
{
	if (s_stCollectSignal.enStep != CSS_enIdle) return FALSE;

	s_stCollectSignal.enMethod = enMethod;
	return TRUE;
}

/**
 * @fn pifCollectSignal_Attach
 * @brief
 * @param enFlag
 * @param fnDevice
 */
void pifCollectSignal_Attach(PIF_enCollectSignalFlag enFlag, PIF_fnCollectSignalDevice fnDevice)
{
	s_stCollectSignal.afnDevice[enFlag] = fnDevice;
}

/**
 * @fn pifCollectSignal_AddDevice
 * @brief
 * @param usPifId
 * @param enVarType
 * @param usSize
 * @param pcReference
 * @param usInitialValue
 * @return
 */
int8_t pifCollectSignal_AddDevice(PIF_usId usPifId, PIF_enCollectSignalVarType enVarType, uint16_t usSize, const char *pcReference,
		uint16_t usInitialValue)
{
	int8_t index = s_stCollectSignal.ucDeviceCount;

	if (s_stCollectSignal.ucDeviceCount >= COLLECT_SIGNAL_DEVICE_MAX) return -1;

	s_stCollectSignal.stDevice[index].usPifId = usPifId;
	s_stCollectSignal.stDevice[index].enVarType = enVarType;
	s_stCollectSignal.stDevice[index].usSize = usSize;
	pif_Printf(s_stCollectSignal.stDevice[index].pcReference, "%s_%x", pcReference, usPifId);
	s_stCollectSignal.stDevice[index].usInitialValue = usInitialValue;
	s_stCollectSignal.ucDeviceCount++;
	return index;
}

/**
 * @fn pifCollectSignal_Start
 * @brief
 */
void pifCollectSignal_Start()
{
	char cBuffer[4];

	for (int i = 0; i < 32; i++) {
		if (s_stCollectSignal.afnDevice[i]) (*s_stCollectSignal.afnDevice[i])();
	}
	s_stCollectSignal.unTimer1ms = 0L;

	switch (s_stCollectSignal.enMethod) {
	case CSM_enRealTime:
		_PrintHeader();

		pifLog_Printf(LT_enVcd, "#0\n");
		break;

	case CSM_enBuffer:
		pif_Printf(cBuffer, "#0\n");
		pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
		break;
	}

	s_stCollectSignal.enStep = CSS_enCollect;
}

/**
 * @fn pifCollectSignal_Stop
 * @brief
 */
void pifCollectSignal_Stop()
{
	char cBuffer[12];

	s_stCollectSignal.enStep = CSS_enIdle;

	switch (s_stCollectSignal.enMethod) {
	case CSM_enRealTime:
		pifLog_Printf(LT_enVcd, "#%u\n", pif_unCumulativeTimer1ms);
		break;

	case CSM_enBuffer:
		pif_Printf(cBuffer, "#%u\n", pif_unCumulativeTimer1ms);
		pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
		break;
	}
}

/**
 * @fn pifCollectSignal_AddSignal
 * @brief
 * @param cIndex
 * @param usState
 */
void pifCollectSignal_AddSignal(int8_t cIndex, uint16_t usState)
{
	char cBuffer[24];

	if (s_stCollectSignal.enStep != CSS_enCollect) return;

	switch (s_stCollectSignal.enMethod) {
	case CSM_enRealTime:
		if (s_stCollectSignal.unTimer1ms != pif_unCumulativeTimer1ms) {
			pifLog_Printf(LT_enVcd, "#%u\n", pif_unCumulativeTimer1ms);
			s_stCollectSignal.unTimer1ms = pif_unCumulativeTimer1ms;
		}
		if (s_stCollectSignal.stDevice[cIndex].usSize == 1) {
			pifLog_Printf(LT_enVcd, "%u%c\n", usState, '!' + cIndex);
		}
		else {
			pifLog_Printf(LT_enVcd, "b%b %c\n", usState, '!' + cIndex);
		}
		break;

	case CSM_enBuffer:
		if (s_stCollectSignal.unTimer1ms != pif_unCumulativeTimer1ms) {
			pif_Printf(cBuffer, "#%u\n", pif_unCumulativeTimer1ms);
			pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
			s_stCollectSignal.unTimer1ms = pif_unCumulativeTimer1ms;
		}
		if (s_stCollectSignal.stDevice[cIndex].usSize == 1) {
			pif_Printf(cBuffer, "%u%c\n", usState, '!' + cIndex);
		}
		else {
			pif_Printf(cBuffer, "b%b %c\n", usState, '!' + cIndex);
		}
		pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
		break;
	}
}

/**
 * @fn pifCollectSignal_PrintLog
 * @brief
 */
void pifCollectSignal_PrintLog()
{
	_PrintHeader();

	pifLog_Disable();
	s_stCollectSignal.enStep = CSS_enSendLog;
}

/**
 * @fn pifCollectSignal_taskAll
 * @brief Task에 연결하는 함수이다.
 * @param pstTask Task에서 결정한다.
 * @return
 */
uint16_t pifCollectSignal_taskAll(PIF_stTask *pstTask)
{
	uint16_t usSize, usLength;
	static uint8_t acTmpBuf[PIF_LOG_LINE_SIZE + 1];

	(void)pstTask;

	if (s_stCollectSignal.enStep == CSS_enSendLog) {
		usSize = pifRingBuffer_GetFillSize(s_stCollectSignal.pstBuffer);
		usLength = PIF_LOG_LINE_SIZE;
		if (usSize > usLength) {
			pifRingBuffer_CopyToArray(acTmpBuf, usLength, s_stCollectSignal.pstBuffer, 0);
			while (usLength) {
				if (acTmpBuf[usLength - 1] == '\n') {
					acTmpBuf[usLength] = 0;
					break;
				}
				else {
					usLength--;
				}
			}
			pifRingBuffer_Remove(s_stCollectSignal.pstBuffer, usLength);
			pifLog_Printf(LT_enVcd, (char *)acTmpBuf);
		}
		else {
			pifRingBuffer_CopyToArray(acTmpBuf, usSize, s_stCollectSignal.pstBuffer, 0);
			acTmpBuf[usSize] = 0;
			pifRingBuffer_Remove(s_stCollectSignal.pstBuffer, usSize);
			pifLog_Printf(LT_enVcd, (char *)acTmpBuf);
			s_stCollectSignal.enStep = CSS_enIdle;
			pifLog_Enable();
		}
	}
	return 0;
}

#endif	// __PIF_COLLECT_SIGNAL__
