#include <stdarg.h>
#include <string.h>

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

const char *c_apcHexChar[2] = {
		"0123456789abcdef",
		"0123456789ABCDEF"
};


static int _DecToAscii(char *pcBuf, uint32_t unVal, uint16_t usStrCnt)
{
    uint16_t usExpCnt = 0;
    uint16_t usZeroStrCnt = 0;
    int nIdx = 0;
    uint32_t unIdxInv = 0;
    uint32_t unTmpVal;
    char acInvBuf[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    unTmpVal = unVal;
    if (unTmpVal != 0) {
        while (unTmpVal) {
        	usExpCnt++;
            if (unTmpVal >= 10) {
                acInvBuf[unIdxInv++] = (unTmpVal % 10) + '0';
            }
            else {
                acInvBuf[unIdxInv++] = unTmpVal + '0';
                break;
            }
            unTmpVal = unTmpVal / 10;
        }

        if ((usStrCnt != 0) && (usExpCnt < usStrCnt)) {
            usZeroStrCnt = usStrCnt - usExpCnt;
            while (usZeroStrCnt) {
            	pcBuf[nIdx++] = '0';
                usZeroStrCnt--;
            }
        }
        while (unIdxInv) {
            unIdxInv--;
            pcBuf[nIdx++] = acInvBuf[unIdxInv];
        }
    }
    else {
        usZeroStrCnt = usStrCnt;
        do {
        	pcBuf[nIdx++] = '0';
            if (usZeroStrCnt > 0) usZeroStrCnt--;
        }
        while (usZeroStrCnt);
    }
    return nIdx;
}

static int _HexToAscii(char *pcBuf, uint32_t unVal, uint16_t usStrCnt, BOOL bUpper)
{
	int i, nIdx = 0;
	BOOL bFirst;
    uint32_t unTmpVal;

    if (usStrCnt) {
    	for (i = (usStrCnt - 1) * 4; i >= 0; i -= 4) {
    		unTmpVal = (unVal >> i) & 0x0F;
    		pcBuf[nIdx++] = c_apcHexChar[bUpper][unTmpVal];
    	}
    }
    else if (unVal > 0) {
    	bFirst = TRUE;
    	for (i = 28; i >= 0; i -= 4) {
    		unTmpVal = (unVal >> i) & 0x0F;
    		if (!bFirst || unTmpVal) {
    			pcBuf[nIdx++] = c_apcHexChar[bUpper][unTmpVal];
    			bFirst = FALSE;
    		}
    	}
    }
    else {
    	pcBuf[nIdx++] = '0';
    }
    return nIdx;
}

static int _FloatToAscii(char *pcBuf, double dNum, uint16_t usPoint)
{
	uint16_t i, nIdx = 0;
	uint32_t unNum;

	if (dNum < 0.0) {
		pcBuf[nIdx++] = '-';
		dNum *= -1.0;
	}

	unNum = (uint32_t)dNum;
	nIdx += _DecToAscii(pcBuf + nIdx, unNum, 0);
	pcBuf[nIdx++] = '.';

	if (usPoint == 0) usPoint = 6;
	dNum -= unNum;
	for (i = 0; i < usPoint; i++) dNum *= 10;

	nIdx += _DecToAscii(pcBuf + nIdx, (uint32_t)dNum, usPoint);
    return nIdx;
}

static void _PrintTime()
{
	int nOffset = 0;
    static char acTmpBuf[20];

	acTmpBuf[nOffset++] = '\n';
	nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucSec, 2);
	acTmpBuf[nOffset++] = '.';
	nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_usTimer1ms, 3);
	acTmpBuf[nOffset++] = ' ';
	acTmpBuf[nOffset++] = 'T';
	acTmpBuf[nOffset++] = ' ';
	nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucHour, 2);
	acTmpBuf[nOffset++] = ':';
	nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucMinute, 2);
	acTmpBuf[nOffset++] = ' ';

	pifLog_Print(acTmpBuf);
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
BOOL pifLog_InitStatic(uint16_t usSize, char *pcBuffer)
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = pifRingBuffer_InitStatic(PIF_ID_AUTO, usSize, pcBuffer);
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
 * @fn pifLog_Print
 * @brief
 * @param pcString
 */
void pifLog_Print(char *pcString)
{
	if (s_stLog.pstBuffer && pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) {
		pifRingBuffer_PutString(s_stLog.pstBuffer, pcString);
	}

	if (s_stLog.bEnable) {
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

/**
 * @fn pifLog_Printf
 * @brief
 * @param enType
 * @param pcFormat
 */
void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...)
{
	va_list data;
	unsigned int unVal;
	int nSignVal;
	unsigned long unLongVal;
	long nSignLongVal;
	uint16_t usNumStrCnt;
	BOOL bLong;
	char *pcVarStr;
    const char *pcStr;
	int nOffset = 0;
	size_t nSize;
    static char acTmpBuf[PIF_LOG_LINE_SIZE];
    static uint8_t nMinute = 255;
    const char cType[] = { 'I', 'W', 'E', 'C' };

    if (enType) {
        if (nMinute != pif_stDateTime.ucMinute) {
        	_PrintTime();
        	nMinute = pif_stDateTime.ucMinute;
    	}

    	acTmpBuf[nOffset++] = '\n';
		nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_stDateTime.ucSec, 2);
    	acTmpBuf[nOffset++] = '.';
		nOffset += _DecToAscii(acTmpBuf + nOffset, (uint32_t)pif_usTimer1ms, 3);
    	acTmpBuf[nOffset++] = ' ';
    	acTmpBuf[nOffset++] = cType[enType - 1];
    	acTmpBuf[nOffset++] = ' ';
    }

	pcStr = pcFormat;
	va_start(data, pcFormat);

	while (*pcStr) {
        if (*pcStr == '%') {
            usNumStrCnt = 0;
        	bLong = FALSE;
NEXT_STR:
            pcStr = pcStr + 1;
            switch(*pcStr) {
                case '0':
                    goto NEXT_STR;

                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    usNumStrCnt *= 10;
                    usNumStrCnt += *pcStr - '0';
                    goto NEXT_STR;

                case 'l':
					bLong = TRUE;
					goto NEXT_STR;

                case 'd':
                case 'i':
                	if (bLong) {
            			nSignLongVal = va_arg(data, long);
            			if (nSignLongVal < 0) {
            			    acTmpBuf[nOffset++] = '-';
            			    nSignLongVal *= -1;
            			}
            			nOffset += _DecToAscii(acTmpBuf + nOffset, nSignLongVal, usNumStrCnt);
                	}
                	else {
            			nSignVal = va_arg(data, int);
            			if (nSignVal < 0) {
            			    acTmpBuf[nOffset++] = '-';
                			nSignVal *= -1;
            			}
            			nOffset += _DecToAscii(acTmpBuf + nOffset, nSignVal, usNumStrCnt);
                	}
                    break;

                case 'u':
                	if (bLong) {
						unLongVal = va_arg(data, unsigned long);
						nOffset += _DecToAscii(acTmpBuf + nOffset, unLongVal, usNumStrCnt);
                	}
                	else {
						unVal = va_arg(data, unsigned int);
						nOffset += _DecToAscii(acTmpBuf + nOffset, unVal, usNumStrCnt);
                	}
                    break;

                case 'x':
                	if (bLong) {
                		unLongVal = va_arg(data, unsigned long);
						nOffset += _HexToAscii(acTmpBuf + nOffset, unLongVal, usNumStrCnt, FALSE);
                	}
                	else {
						unVal = va_arg(data, unsigned int);
						nOffset += _HexToAscii(acTmpBuf + nOffset, unVal, usNumStrCnt, FALSE);
                	}
                    break;

                case 'X':
                	if (bLong) {
                		unLongVal = va_arg(data, unsigned long);
                		nOffset += _HexToAscii(acTmpBuf + nOffset, unLongVal, usNumStrCnt, TRUE);
                	}
                	else {
                		unVal = va_arg(data, unsigned int);
                		nOffset += _HexToAscii(acTmpBuf + nOffset, unVal, usNumStrCnt, TRUE);
                	}
                    break;

                case 'f':
					nOffset += _FloatToAscii(acTmpBuf + nOffset, va_arg(data, double), usNumStrCnt);
                    break;

                case 's':
                    pcVarStr = va_arg(data, char *);
                    if (pcVarStr) {
						nSize = strlen(pcVarStr);
						if (nOffset + nSize < PIF_LOG_LINE_SIZE - 1) {
							strcpy(acTmpBuf + nOffset, pcVarStr);
						}
						else {
							nSize = PIF_LOG_LINE_SIZE - 1 - nOffset;
							strncpy(acTmpBuf + nOffset, pcVarStr, nSize);
						}
						nOffset += nSize;
                    }
                    break;

                case 'c':
                    acTmpBuf[nOffset++] = va_arg(data, int);
                    break;

                case '%':
                    acTmpBuf[nOffset++] = '%';
                    break;
            }
        }
        else {
            acTmpBuf[nOffset++] = *pcStr;
        }
        pcStr = pcStr + 1;
	}
	acTmpBuf[nOffset] = 0;

	va_end(data);

	pifLog_Print(acTmpBuf);
}

/**
 * @fn pifLog_PrintInBuffer
 * @brief
 */
void pifLog_PrintInBuffer()
{
	uint8_t pBuffer[128];
	uint16_t usLength;

	if (!s_stLog.actPrint || !pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) return;

	while (!pifRingBuffer_IsEmpty(s_stLog.pstBuffer)) {
		usLength = pifRingBuffer_CopyToArray(pBuffer, s_stLog.pstBuffer, 127);
		pBuffer[usLength] = 0;
		(*s_stLog.actPrint)((char *)pBuffer);
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
