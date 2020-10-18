/*
 * pifLog.c
 *
 *  Created on: 2020. 7. 19.
 *      Author: wonjh
 */

#include <stdarg.h>
#include <string.h>

#include "pifLog.h"


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
	nIdx += _DecToAscii(pcBuf, unNum, 0);
	pcBuf[nIdx++] = '.';

	dNum -= unNum;
	for (i = 0; i < (usPoint == 0 ? 6 : usPoint); i++) dNum *= 10;

	nIdx += _DecToAscii(pcBuf + nIdx, (uint32_t)dNum, 0);
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
	s_stLog.__bEnable = TRUE;
	pifRingBuffer_Init(&s_stLog.__stBuffer);
#ifndef __PIF_NO_TERMINAL__
	s_stLog.__pstTerminal = NULL;
#endif
	s_stLog.__actPrint = NULL;
}

/**
 * @fn pifLog_Exit
 * @brief Log 구조체를 파기하다.
 */
void pifLog_Exit()
{
	pifRingBuffer_Exit(&s_stLog.__stBuffer);
}

/**
 * @fn pifLog_InitBufferAlloc
 * @brief
 * @param usSize
 * @return
 */
BOOL pifLog_InitBufferAlloc(uint16_t usSize)
{
	return pifRingBuffer_InitAlloc(&s_stLog.__stBuffer, usSize);
}

/**
 * @fn pifLog_InitBufferShare
 * @brief
 * @param usSize
 * @param pcBuffer
 */
void pifLog_InitBufferShare(uint16_t usSize, char *pcBuffer)
{
	pifRingBuffer_InitShare(&s_stLog.__stBuffer, usSize, pcBuffer);
}

#ifndef __PIF_NO_TERMINAL__

/**
 * @fn pifLog_AttachTerminal
 * @brief
 * @param pstTerminal
 */
void pifLog_AttachTerminal(PIF_stTerminal *pstTerminal)
{
	s_stLog.__pstTerminal = pstTerminal;
}

/**
 * @fn pifLog_DetachTerminal
 * @brief
 */
void pifLog_DetachTerminal()
{
	s_stLog.__pstTerminal = NULL;
}

#endif

/**
 * @fn pifLog_Enable
 * @brief
 */
void pifLog_Enable()
{
	s_stLog.__bEnable = TRUE;
}

/**
 * @fn pifLog_Disable
 * @brief
 */
void pifLog_Disable()
{
	s_stLog.__bEnable = FALSE;
}

/**
 * @fn pifLog_Print
 * @brief
 * @param pcString
 */
void pifLog_Print(char *pcString)
{
	if (pifRingBuffer_IsAlloc(&s_stLog.__stBuffer)) {
		pifRingBuffer_PushString(&s_stLog.__stBuffer, pcString);
	}

	if (s_stLog.__bEnable) {
#ifndef __PIF_NO_TERMINAL__
		if (s_stLog.__pstTerminal) {
			pifRingBuffer_PushString(&s_stLog.__pstTerminal->stTxBuffer, pcString);
		}
#endif

		if (s_stLog.__actPrint) {
			(*s_stLog.__actPrint)(pcString);
		}
	}
}

/**
 * @fn pifLog_Printf
 * @brief
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
	float fVal;
	double dVal;
    static char acTmpBuf[128];
    static int nMinute = -1;
    const char cType[4] = { 'I', 'W', 'E' };

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
                	if (bLong) {
						dVal = va_arg(data, double);
						nOffset += _FloatToAscii(acTmpBuf + nOffset, dVal, usNumStrCnt);
                	}
                	else {
						fVal = va_arg(data, double);
						nOffset += _FloatToAscii(acTmpBuf + nOffset, fVal, usNumStrCnt);
                	}
                    break;

                case 's':
                    pcVarStr = va_arg(data, char *);
        			strcpy(acTmpBuf + nOffset, pcVarStr);
        			nOffset += strlen(pcVarStr);
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

	if (!s_stLog.__actPrint || !pifRingBuffer_IsAlloc(&s_stLog.__stBuffer)) return;

	while (!pifRingBuffer_IsEmpty(&s_stLog.__stBuffer)) {
		usLength = pifRingBuffer_CopyToArray(pBuffer, &s_stLog.__stBuffer, 127);
		pBuffer[usLength] = 0;
		(*s_stLog.__actPrint)((char *)pBuffer);
		pifRingBuffer_Remove(&s_stLog.__stBuffer, usLength);
	}
}

/**
 * @fn pifLog_AttachActPrint
 * @brief
 * @param actPrint
 */
void pifLog_AttachActPrint(PIF_actLogPrint actPrint)
{
	s_stLog.__actPrint = actPrint;
}
