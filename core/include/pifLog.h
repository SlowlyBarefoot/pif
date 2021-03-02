#ifndef PIF_LOG_H
#define PIF_LOG_H


#include "pifRingBuffer.h"
#ifndef __PIF_NO_TERMINAL__
#include "pifTerminal.h"
#endif


#ifndef PIF_LOG_LINE_SIZE
#define PIF_LOG_LINE_SIZE	128
#endif


typedef enum _PIF_enLogType
{
	LT_enNone	= 0,
	LT_enVcd	= 1,			// Collect Signal : VCD file
	LT_enInfo	= 2,
	LT_enWarn	= 3,
	LT_enError	= 4,
	LT_enComm	= 5
} PIF_enLogType;


typedef void (*PIF_actLogPrint)(char *pcString);


#ifdef __cplusplus
extern "C" {
#endif

void pifLog_Init();
BOOL pifLog_InitHeap(uint16_t usSize);
BOOL pifLog_InitStatic(uint16_t usSize, char *pcBuffer);
void pifLog_Exit();

#ifndef __PIF_NO_TERMINAL__
void pifLog_UseTerminal(BOOL bUse);
#endif

void pifLog_Enable();
void pifLog_Disable();

BOOL pifLog_IsEmpty();

void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...);

void pifLog_PrintInBuffer();

// Attach Action Function
void pifLog_AttachActPrint(PIF_actLogPrint actPrint);
void pifLog_DetachActPrint();

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
