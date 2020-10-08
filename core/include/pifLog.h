/*
 * pifLog.h
 *
 *  Created on: 2020. 7. 19.
 *      Author: wonjh
 */

#ifndef PIF_LOG_H
#define PIF_LOG_H


#include "pifRingBuffer.h"
#ifndef __PIF_NO_TERMINAL__
#include "pifTerminal.h"
#endif


typedef enum _PIF_enLogType
{
	LT_enNone	= 0,
	LT_enInfo	= 1,
	LT_enWarn	= 2,
	LT_enError	= 3
} PIF_enLogType;


typedef void (*PIF_actLogPrint)(char *pcString);


/**
 * @class _PIF_stLog
 * @author wonjh
 * @date 11/06/20
 * @file pifLog.h
 * @brief
 */
typedef struct _PIF_stLog
{
	// Public Member Variable

	// Private Member Variable
	PIF_stRingBuffer *__pstBuffer;
#ifndef __PIF_NO_TERMINAL__
	PIF_stTerminal *__pstTerminal;
#endif

	// Private Member Function
	PIF_actLogPrint __actPrint;
} PIF_stLog;


#ifdef __cplusplus
extern "C" {
#endif

void pifLog_AttachBuffer(PIF_stRingBuffer *pstBuffer);
#ifndef __PIF_NO_TERMINAL__
void pifLog_AttachTerminal(PIF_stTerminal *pstTerminal);
void pifLog_DetachTerminal();
#endif

void pifLog_Print(char *pcString);
void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...);

// Attach Action Function
void pifLog_AttachActPrint(PIF_actLogPrint actPrint);

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
