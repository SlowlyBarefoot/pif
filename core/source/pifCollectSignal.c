#ifdef __PIF_COLLECT_SIGNAL__

#include <string.h>

#include "pif_list.h"
#include "pifCollectSignal.h"
#include "pifLog.h"


typedef enum _PIF_enCollectSignalStep
{
	CSS_enIdle		= 0,
	CSS_enCollect	= 1,
	CSS_enSendLog	= 2
} PIF_enCollectSignalStep;


typedef struct _PIF_stCollectSignalDevice
{
	PifId usPifId;
	PIF_enCollectSignalVarType enVarType;
	uint8_t index;
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
	PifDList stDevice;
	PIF_fnCollectSignalDevice afnDevice[32];
} PIF_stCollectSignal;


static PIF_stCollectSignal s_stCollectSignal;


static void _PrintHeader()
{
	const char *c_pacVarType[] =
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
	PifDListIterator it;

	pifLog_Printf(LT_enVcd, "\n$date %s %u, %u %2u:%2u:%2u $end\n",
			kPifMonth3[pif_datetime.month - 1], pif_datetime.day, 2000 + pif_datetime.year,
			pif_datetime.hour, pif_datetime.minute, pif_datetime.second);

	pifLog_Printf(LT_enVcd, "$version %u.%u.%u $end\n", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH);

	pifLog_Printf(LT_enVcd, "$timescale 1ms $end\n");

	pifLog_Printf(LT_enVcd, "$scope module %s $end\n", s_stCollectSignal.c_pcModuleName);
	it = pifDList_Begin(&s_stCollectSignal.stDevice);
	while (it) {
		PIF_stCollectSignalDevice* p_device = (PIF_stCollectSignalDevice*)it->data;
		if (p_device->usSize == 1) {
			pifLog_Printf(LT_enVcd, "$var %s 1 %c %s $end\n", c_pacVarType[p_device->enVarType], '!' + p_device->index, p_device->pcReference);
		}
		else {
			pifLog_Printf(LT_enVcd, "$var %s %u %c %s[%u:0] $end\n", c_pacVarType[p_device->enVarType],
					p_device->usSize, '!' + p_device->index, p_device->pcReference, p_device->usSize - 1);
		}

		it = pifDList_Next(it);
	}
	pifLog_Printf(LT_enVcd, "$upscope $end\n");
	pifLog_Printf(LT_enVcd, "$enddefinitions $end\n");

	pifLog_Printf(LT_enVcd, "$dumpvars\n");
	it = pifDList_Begin(&s_stCollectSignal.stDevice);
	while (it) {
		PIF_stCollectSignalDevice* p_device = (PIF_stCollectSignalDevice*)it->data;
		if (p_device->usSize == 1) {
			pifLog_Printf(LT_enVcd, "%u%c\n", p_device->usInitialValue, '!' + p_device->index);
		}
		else {
			pifLog_Printf(LT_enVcd, "b%b %c\n", p_device->usInitialValue, '!' + p_device->index);
		}

		it = pifDList_Next(it);
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
	pifDList_Init(&s_stCollectSignal.stDevice);
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
 * @fn pifCollectSignal_Clear
 * @brief CollectSignal 구조체를 파기하다.
 */
void pifCollectSignal_Clear()
{
	if (s_stCollectSignal.pstBuffer) {
		pifRingBuffer_Exit(s_stCollectSignal.pstBuffer);
		s_stCollectSignal.pstBuffer = NULL;
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
void* pifCollectSignal_AddDevice(PifId usPifId, PIF_enCollectSignalVarType enVarType, uint16_t usSize,
		const char* pcReference, uint16_t usInitialValue)
{
	PIF_stCollectSignalDevice* p_device;

	p_device = pifDList_AddLast(&s_stCollectSignal.stDevice, sizeof(PIF_stCollectSignalDevice));
	if (!p_device) return NULL;

	p_device->usPifId = usPifId;
	p_device->enVarType = enVarType;
	p_device->index = s_stCollectSignal.ucDeviceCount;
	p_device->usSize = usSize;
	pif_Printf(p_device->pcReference, "%s_%x", pcReference, usPifId);
	p_device->usInitialValue = usInitialValue;
	s_stCollectSignal.ucDeviceCount++;
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enInfo, "CS:Add(ID:%u VT:%u SZ:%u) DC:%d", usPifId, enVarType, usSize, p_device->usSize);
#endif
	return p_device;
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
		pifLog_Printf(LT_enVcd, "#%u\n", pif_cumulative_timer1ms);
		break;

	case CSM_enBuffer:
		pif_Printf(cBuffer, "#%u\n", pif_cumulative_timer1ms);
		pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
		break;
	}

	pifDList_Clear(&s_stCollectSignal.stDevice);
	s_stCollectSignal.ucDeviceCount = 0;
}

/**
 * @fn pifCollectSignal_AddSignal
 * @brief
 * @param p_dev
 * @param usState
 */
void pifCollectSignal_AddSignal(void* p_dev, uint16_t usState)
{
	PIF_stCollectSignalDevice* p_device = (PIF_stCollectSignalDevice*)p_dev;
	char cBuffer[24];

	if (s_stCollectSignal.enStep != CSS_enCollect) return;

	switch (s_stCollectSignal.enMethod) {
	case CSM_enRealTime:
		if (s_stCollectSignal.unTimer1ms != pif_cumulative_timer1ms) {
			pifLog_Printf(LT_enVcd, "#%u\n", pif_cumulative_timer1ms);
			s_stCollectSignal.unTimer1ms = pif_cumulative_timer1ms;
		}
		if (p_device->usSize == 1) {
			pifLog_Printf(LT_enVcd, "%u%c\n", usState, '!' + p_device->index);
		}
		else {
			pifLog_Printf(LT_enVcd, "b%b %c\n", usState, '!' + p_device->index);
		}
		break;

	case CSM_enBuffer:
		if (s_stCollectSignal.unTimer1ms != pif_cumulative_timer1ms) {
			pif_Printf(cBuffer, "#%u\n", pif_cumulative_timer1ms);
			pifRingBuffer_PutString(s_stCollectSignal.pstBuffer, cBuffer);
			s_stCollectSignal.unTimer1ms = pif_cumulative_timer1ms;
		}
		if (p_device->usSize == 1) {
			pif_Printf(cBuffer, "%u%c\n", usState, '!' + p_device->index);
		}
		else {
			pif_Printf(cBuffer, "b%b %c\n", usState, '!' + p_device->index);
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

static uint16_t _DoTask(PIF_stTask *pstTask)
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

/**
 * @fn pifCollectSignal_AttachTask
 * @brief Task를 추가한다.
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PIF_stTask *pifCollectSignal_AttachTask(PIF_enTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, &s_stCollectSignal, bStart);
}

#endif	// __PIF_COLLECT_SIGNAL__
