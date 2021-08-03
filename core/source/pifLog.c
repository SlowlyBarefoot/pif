#include <stdarg.h>

#include "pifLog.h"


typedef struct _PIF_stLog
{
	// Public Member Variable

	// Private Member Variable
	BOOL bEnable;
	PIF_stRingBuffer *pstBuffer;
#ifndef __PIF_NO_TERMINAL__
	BOOL bUseTerminal;
#endif

	// Private Member Function
	PIF_actLogPrint actPrint;
} PIF_stLog;


static PIF_stLog s_stLog;

PIF_stLogFlag pif_stLogFlag = { .unAll = 0L };


static void _PrintLog(char *pcString, BOOL bVcd)
{
	if (!bVcd && s_stLog.pstBuffer && pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) {
		pifRingBuffer_PutString(s_stLog.pstBuffer, pcString);
	}

	if (s_stLog.bEnable || bVcd) {
#ifndef __PIF_NO_TERMINAL__
		if (s_stLog.bUseTerminal) {
			pifRingBuffer_PutString(pifTerminal_GetTxBuffer(), pcString);
		}
#endif

		if (s_stLog.actPrint) {
			(*s_stLog.actPrint)(pcString);
		}
	}
}

static void _PrintTime()
{
	int nOffset = 0;
    static char acTmpBuf[20];

	acTmpBuf[nOffset++] = '\n';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucSec, 2);
	acTmpBuf[nOffset++] = '.';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_usTimer1ms, 3);
	acTmpBuf[nOffset++] = ' ';
	acTmpBuf[nOffset++] = 'T';
	acTmpBuf[nOffset++] = ' ';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucHour, 2);
	acTmpBuf[nOffset++] = ':';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucMinute, 2);
	acTmpBuf[nOffset++] = ' ';

	_PrintLog(acTmpBuf, FALSE);
}

/**
 * @fn pifLog_Init
 * @brief Log 구조체 초기화한다.
 */
void pifLog_Init()
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = NULL;
#ifndef __PIF_NO_TERMINAL__
	s_stLog.bUseTerminal = FALSE;
#endif
	s_stLog.actPrint = NULL;
}

/**
 * @fn pifLog_InitHeap
 * @brief Log 구조체 초기화한다.
 * @param usSize
 * @return
 */
BOOL pifLog_InitHeap(uint16_t usSize)
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, usSize);
	if (!s_stLog.pstBuffer) return FALSE;
#ifndef __PIF_NO_TERMINAL__
	s_stLog.bUseTerminal = FALSE;
#endif
	s_stLog.actPrint = NULL;
	return TRUE;
}

/**
 * @fn pifLog_InitStatic
 * @brief Log 구조체 초기화한다.
 * @param usSize
 * @param pcBuffer
 * @return
 */
BOOL pifLog_InitStatic(uint16_t usSize, uint8_t *pucBuffer)
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = pifRingBuffer_InitStatic(PIF_ID_AUTO, usSize, pucBuffer);
	if (!s_stLog.pstBuffer) return FALSE;
#ifndef __PIF_NO_TERMINAL__
	s_stLog.bUseTerminal = FALSE;
#endif
	s_stLog.actPrint = NULL;
	return TRUE;
}

/**
 * @fn pifLog_Exit
 * @brief Log 구조체를 파기하다.
 */
void pifLog_Exit()
{
	if (s_stLog.pstBuffer) {
		pifRingBuffer_Exit(s_stLog.pstBuffer);
	}
}

#ifndef __PIF_NO_TERMINAL__

/**
 * @fn pifLog_UseTerminal
 * @brief
 * @param bUse
 */
void pifLog_UseTerminal(BOOL bUse)
{
	s_stLog.bUseTerminal = bUse;
}

#endif

/**
 * @fn pifLog_Enable
 * @brief
 */
void pifLog_Enable()
{
	s_stLog.bEnable = TRUE;
}

/**
 * @fn pifLog_Disable
 * @brief
 */
void pifLog_Disable()
{
	s_stLog.bEnable = FALSE;
}

/**
 * @fn pifLog_IsEmpty
 * @brief
 * @return
 */
BOOL pifLog_IsEmpty()
{
	return pifRingBuffer_IsEmpty(s_stLog.pstBuffer);
}

/**
 * @fn pifLog_Printf
 * @brief
 * @param enType
 * @param pcFormat
 */
void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...)
{
	va_list data;
	int nOffset = 0;
    static char acTmpBuf[PIF_LOG_LINE_SIZE];
    static uint8_t nMinute = 255;
    const char cType[] = { 'I', 'W', 'E', 'C' };
    extern void _PrintFormat(char *pcBuffer, va_list *pstData, const char *pcFormat);

    if (enType >= LT_enInfo) {
        if (nMinute != pif_stDateTime.ucMinute) {
        	_PrintTime();
        	nMinute = pif_stDateTime.ucMinute;
    	}

    	acTmpBuf[nOffset++] = '\n';
		nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucSec, 2);
    	acTmpBuf[nOffset++] = '.';
		nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_usTimer1ms, 3);
    	acTmpBuf[nOffset++] = ' ';
    	acTmpBuf[nOffset++] = cType[enType - LT_enInfo];
    	acTmpBuf[nOffset++] = ' ';
    }

	va_start(data, pcFormat);
	_PrintFormat(acTmpBuf + nOffset, &data, pcFormat);
	va_end(data);

	_PrintLog(acTmpBuf, enType == LT_enVcd);
}

/**
 * @fn pifLog_PrintInBuffer
 * @brief
 */
void pifLog_PrintInBuffer()
{
	uint8_t aucBuffer[128];
	uint16_t usLength;

	if (!s_stLog.actPrint || !pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) return;

	while (!pifRingBuffer_IsEmpty(s_stLog.pstBuffer)) {
		usLength = pifRingBuffer_CopyToArray(aucBuffer, 127, s_stLog.pstBuffer, 0);
		aucBuffer[usLength] = 0;
		(*s_stLog.actPrint)((char *)aucBuffer);
		pifRingBuffer_Remove(s_stLog.pstBuffer, usLength);
	}
}

/**
 * @fn pifLog_AttachActPrint
 * @brief
 * @param actPrint
 */
void pifLog_AttachActPrint(PIF_actLogPrint actPrint)
{
	s_stLog.actPrint = actPrint;
}

/**
 * @fn pifLog_DetachActPrint
 * @brief
 */
void pifLog_DetachActPrint()
{
	s_stLog.actPrint = NULL;
}
