#include <stdarg.h>
#include <string.h>

#include "pif.h"
#include "pifLog.h"
#include "pifTask.h"


PIF_enError pif_enError = E_enSuccess;

volatile uint16_t pif_usTimer1ms = 0;
volatile uint32_t pif_unTimer1sec = 0L;
volatile PIF_stDateTime pif_stDateTime;

#ifndef __PIF_NO_LOG__
PIF_stLogFlag pif_stLogFlag = { .unFlags = 0L };
#endif

volatile uint32_t pif_unCumulativeTimer1ms = 0L;

PIF_stPerformance pif_stPerformance = { 0, 0 };

PIF_usId pif_usPifId = 1;

PIF_actTimer1us pif_actTimer1us = NULL;

static const uint8_t c_ucDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static const char *c_apcHexChar[2] = {
		"0123456789abcdef",
		"0123456789ABCDEF"
};

static uint8_t ucCrc7;


/**
 * @fn pif_Init
 * @brief pif의 전역 변수를 초기화한다.
 * @param actTimer1us
 */
void pif_Init(PIF_actTimer1us actTimer1us)
{
	pif_actTimer1us = actTimer1us;

#ifndef __PIF_NO_LOG__
    memset(&pif_stLogFlag, 0, sizeof(pif_stLogFlag));
#endif
}

/**
 * @fn pif_Loop
 * @brief Main loop에서 수행해야 하는 pif 함수이다.
 */
void pif_Loop()
{
#ifdef __PIF_DEBUG__
	static uint8_t ucSec = 0;
#endif

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btPerformance) {
		pif_stPerformance._unCount++;
		if (pif_stPerformance.__ucState) {
        	double fValue = (double)1000 / pif_stPerformance._unCount;
        	pifLog_Printf(LT_enInfo, "Performance: %lur/s, %2fus", pif_stPerformance._unCount, fValue);

        	pif_stPerformance._unCount = 0;
    		pif_stPerformance.__ucState = 0;
        }
    }
#endif

#ifdef __PIF_DEBUG__
    if (ucSec != pif_stDateTime.ucSec) {
    	pifTask_Print();
    	ucSec = pif_stDateTime.ucSec;
    }
#endif
}

/**
 * @fn pif_sigTimer1ms
 * @brief 1ms 타이머 인터럽트에서 실행할 pif 함수이다.
 */
void pif_sigTimer1ms()
{
	uint8_t days;
	static uint16_t usTimerPerform = 0;

#ifndef __PIF_NO_LOG__
    if (pif_stLogFlag.btPerformance) {
		usTimerPerform++;
		if (usTimerPerform >= 1000) {
			usTimerPerform = 0;
			pif_stPerformance.__ucState = 1;
		}
    }
#endif

	pif_unCumulativeTimer1ms++;
    pif_usTimer1ms++;
    if (pif_usTimer1ms >= 1000) {
        pif_usTimer1ms = 0;

        pif_unTimer1sec++;
    	pif_stDateTime.ucSec++;
    	if (pif_stDateTime.ucSec >= 60) {
    		pif_stDateTime.ucSec = 0;
    		pif_stDateTime.ucMinute++;
    		if (pif_stDateTime.ucMinute >= 60) {
    			pif_stDateTime.ucMinute = 0;
    			pif_stDateTime.ucHour++;
    			if (pif_stDateTime.ucHour >= 24) {
    				pif_stDateTime.ucHour = 0;
    				pif_stDateTime.ucDay++;
    				days = c_ucDaysInMonth[pif_stDateTime.ucMonth - 1];
    				if (pif_stDateTime.ucMonth == 2) {
    					if (pif_stDateTime.usYear / 4 == 0) {
    						if (pif_stDateTime.usYear / 100 == 0) {
    							if (pif_stDateTime.usYear / 400 == 0) days++;
    						}
    						else days++;
    					}
    				}
    				if (pif_stDateTime.ucDay > days) {
    					pif_stDateTime.ucDay = 1;
    					pif_stDateTime.ucMonth++;
    					if (pif_stDateTime.ucMonth > 12) {
    						pif_stDateTime.ucMonth = 1;
    						pif_stDateTime.usYear++;
    					}
    				}
    			}
    		}
    	}
    }
}

/**
 * @fn pif_Delay1ms
 * @brief
 * @param usDelay
 */
void pif_Delay1ms(uint16_t usDelay)
{
	uint32_t unLimit = pif_unCumulativeTimer1ms + usDelay;
	while (pif_unCumulativeTimer1ms != unLimit);
}

/**
 * @fn pif_ClearError
 * @brief Error를 정리하다.
 */
void pif_ClearError()
{
	pif_enError = E_enSuccess;
}

/**
 * @fn pif_BinToString
 * @brief
 * @param pcBuf
 * @param unVal
 * @param usStrCnt
 * @return
 */
int pif_BinToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt)
{
	int i, nIdx = 0;
	BOOL bFirst;
    uint32_t unTmpVal;

    if (usStrCnt) {
    	for (i = usStrCnt - 1; i >= 0; i--) {
    		pcBuf[nIdx++] = '0' + ((unVal >> i) & 1);
    	}
    }
    else if (unVal > 0) {
    	bFirst = TRUE;
    	for (i = 31; i >= 0; i--) {
    		unTmpVal = (unVal >> i) & 1;
    		if (!bFirst || unTmpVal) {
    			pcBuf[nIdx++] = '0' + unTmpVal;
    			bFirst = FALSE;
    		}
    	}
    }
    else {
    	pcBuf[nIdx++] = '0';
    }
    return nIdx;
}

/**
 * @fn pif_DecToString
 * @brief
 * @param pcBuf
 * @param unVal
 * @param usStrCnt
 * @return
 */
int pif_DecToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt)
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

/**
 * @fn pif_HexToString
 * @brief
 * @param pcBuf
 * @param unVal
 * @param usStrCnt
 * @param bUpper
 * @return
 */
int pif_HexToString(char *pcBuf, uint32_t unVal, uint16_t usStrCnt, BOOL bUpper)
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

/**
 * @fn pif_FloatToString
 * @brief
 * @param pcBuf
 * @param dNum
 * @param usPoint
 * @return
 */
int pif_FloatToString(char *pcBuf, double dNum, uint16_t usPoint)
{
	uint16_t i, nIdx = 0;
	uint32_t unNum;

	if (dNum < 0.0) {
		pcBuf[nIdx++] = '-';
		dNum *= -1.0;
	}

	unNum = (uint32_t)dNum;
	nIdx += pif_DecToString(pcBuf + nIdx, unNum, 0);
	pcBuf[nIdx++] = '.';

	if (usPoint == 0) usPoint = 6;
	dNum -= unNum;
	for (i = 0; i < usPoint; i++) dNum *= 10;

	nIdx += pif_DecToString(pcBuf + nIdx, (uint32_t)dNum, usPoint);
    return nIdx;
}

/**
 * @fn _PrintFormat
 * @brief
 * @param pcBuffer
 * @param pstData
 * @param pcStr
 */
void _PrintFormat(char *pcBuffer, va_list *pstData, const char *pcFormat)
{
	unsigned int unVal;
	int nSignVal;
	unsigned long unLongVal;
	long nSignLongVal;
	uint16_t usNumStrCnt;
	BOOL bLong;
	char *pcVarStr;
	int nOffset = 0;
	size_t nSize;

	while (*pcFormat) {
        if (*pcFormat == '%') {
            usNumStrCnt = 0;
        	bLong = FALSE;
NEXT_STR:
			pcFormat = pcFormat + 1;
            switch(*pcFormat) {
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
                    usNumStrCnt += *pcFormat - '0';
                    goto NEXT_STR;

                case 'l':
					bLong = TRUE;
					goto NEXT_STR;

                case 'b':
                	if (bLong) {
                		unLongVal = va_arg(*pstData, unsigned long);
						nOffset += pif_BinToString(pcBuffer + nOffset, unLongVal, usNumStrCnt);
                	}
                	else {
						unVal = va_arg(*pstData, unsigned int);
						nOffset += pif_BinToString(pcBuffer + nOffset, unVal, usNumStrCnt);
                	}
                    break;

                case 'd':
                case 'i':
                	if (bLong) {
            			nSignLongVal = va_arg(*pstData, long);
            			if (nSignLongVal < 0) {
            				pcBuffer[nOffset++] = '-';
            			    nSignLongVal *= -1;
            			}
            			nOffset += pif_DecToString(pcBuffer + nOffset, nSignLongVal, usNumStrCnt);
                	}
                	else {
            			nSignVal = va_arg(*pstData, int);
            			if (nSignVal < 0) {
            				pcBuffer[nOffset++] = '-';
                			nSignVal *= -1;
            			}
            			nOffset += pif_DecToString(pcBuffer + nOffset, nSignVal, usNumStrCnt);
                	}
                    break;

                case 'u':
                	if (bLong) {
						unLongVal = va_arg(*pstData, unsigned long);
						nOffset += pif_DecToString(pcBuffer + nOffset, unLongVal, usNumStrCnt);
                	}
                	else {
						unVal = va_arg(*pstData, unsigned int);
						nOffset += pif_DecToString(pcBuffer + nOffset, unVal, usNumStrCnt);
                	}
                    break;

                case 'x':
                	if (bLong) {
                		unLongVal = va_arg(*pstData, unsigned long);
						nOffset += pif_HexToString(pcBuffer + nOffset, unLongVal, usNumStrCnt, FALSE);
                	}
                	else {
						unVal = va_arg(*pstData, unsigned int);
						nOffset += pif_HexToString(pcBuffer + nOffset, unVal, usNumStrCnt, FALSE);
                	}
                    break;

                case 'X':
                	if (bLong) {
                		unLongVal = va_arg(*pstData, unsigned long);
                		nOffset += pif_HexToString(pcBuffer + nOffset, unLongVal, usNumStrCnt, TRUE);
                	}
                	else {
                		unVal = va_arg(*pstData, unsigned int);
                		nOffset += pif_HexToString(pcBuffer + nOffset, unVal, usNumStrCnt, TRUE);
                	}
                    break;

                case 'f':
					nOffset += pif_FloatToString(pcBuffer + nOffset, va_arg(*pstData, double), usNumStrCnt);
                    break;

                case 's':
                    pcVarStr = va_arg(*pstData, char *);
                    if (pcVarStr) {
						nSize = strlen(pcVarStr);
						if (nOffset + nSize < PIF_LOG_LINE_SIZE - 1) {
							strcpy(pcBuffer + nOffset, pcVarStr);
						}
						else {
							nSize = PIF_LOG_LINE_SIZE - 1 - nOffset;
							strncpy(pcBuffer + nOffset, pcVarStr, nSize);
						}
						nOffset += nSize;
                    }
                    break;

                case 'c':
                	pcBuffer[nOffset++] = va_arg(*pstData, int);
                    break;

                case '%':
                	pcBuffer[nOffset++] = '%';
                    break;
            }
        }
        else {
        	pcBuffer[nOffset++] = *pcFormat;
        }
        pcFormat = pcFormat + 1;
	}
	pcBuffer[nOffset] = 0;
}

/**
 * @fn pif_Printf
 * @brief
 * @param pcBuffer
 * @param pcFormat
 */
void pif_Printf(char *pcBuffer, const char *pcFormat, ...)
{
	va_list data;

	va_start(data, pcFormat);
	_PrintFormat(pcBuffer, &data, pcFormat);
	va_end(data);
}

/**
 * @fn pifCrc7_Init
 * @brief 7비트 CRC 계산을 위한 변수를 초기화한다.
 */
void pifCrc7_Init()
{
	ucCrc7 = 0;
}

/**
 * @fn pifCrc7_Calcurate
 * @brief 7비트 CRC를 계산한다.
 * @param unData 7비트 CRC 계산에 이 데이터를 추가한다.
 */
void pifCrc7_Calcurate(uint8_t ucData)
{
	for (int i = 0; i < 8; i++) {
		ucCrc7 <<= 1;
		if ((ucData & 0x80) ^ (ucCrc7 & 0x80)) ucCrc7 ^=0x09;
		ucData <<= 1;
	}
}

/**
 * @fn pifCrc7_Result
 * @brief 계산된 7비트 CRC 결과를 반환한다.
 * @return 7비트 CRC 결과
 */
uint8_t pifCrc7_Result()
{
	ucCrc7 = (ucCrc7 << 1) | 1;
	return ucCrc7;
}

/**
 * @fn pifCrc16
 * @brief 16비트 CRC를 계산한 결과를 반환한다.
 * @param pucData 16비트 CRC 계산할 데이터.
 * @param usLength 16비트 CRC 계산할 데이터의 크기.
 */
uint16_t pifCrc16(uint8_t *pucData, uint16_t usLength)
{
	uint16_t i;
	uint32_t unCrc16 = 0;
	uint32_t temp;

	for (i = 0; i < usLength; i++) {
		unCrc16 ^= (uint16_t)pucData[i] << 8;
		for (int i = 0; i < 8; i++) {
			temp = unCrc16 << 1;
			if (unCrc16 & 0x8000) temp ^= 0x1021;
			unCrc16 = temp;
		}
	}
	return unCrc16 & 0xFFFF;
}

/**
 * @fn pifCheckSum
 * @brief Byte 단위로 합산한 결과를 반환한다.
 * @param pucData Checksum할 데이터.
 * @param usLength Checksum할 데이터의 크기.
 */
uint8_t pifCheckSum(uint8_t *pucData, uint16_t usLength)
{
	uint16_t i;
	uint8_t ucSum = 0;

	for (i = 0; i < usLength; i++) {
		ucSum += pucData[i];
	}
	return ucSum & 0xFF;
}

/**
 * @fn pifCheckXor
 * @brief Byte 단위로 Xor한 결과를 반환한다.
 * @param pucData CheckXor할 데이터.
 * @param usLength CheckXor할 데이터의 크기.
 */
uint8_t pifCheckXor(uint8_t *pucData, uint16_t usLength)
{
	uint16_t i;
	uint8_t ucXor = 0;

	for (i = 0; i < usLength; i++) {
		ucXor ^= pucData[i];
	}
	return ucXor & 0xFF;
}

/**
 * @fn pifPidControl_Init
 * @brief PID 컨트롤에 사용되는 전역 변수를 초기화한다.
 * @param pstOwner PidControl 자신
 * @param fFsKp 비례 계수
 * @param fFsKi 적분 계수
 * @param fFsKd 미분 계수
 * @param fMaxIntegration 최대 적분 오차값
 */
void pifPidControl_Init(PIF_stPidControl *pstOwner, float fFsKp, float fFsKi, float fFsKd, float fMaxIntegration)
{
	pstOwner->fFsKp = fFsKp;
	pstOwner->fFsKi = fFsKi;
	pstOwner->fFsKd = fFsKd;
	pstOwner->fMaxIntegration = fMaxIntegration;
	pstOwner->_stPrivate.fErrSum = 0;
	pstOwner->_stPrivate.fErrPrev = 0;
}

/**
 * @fn pifPidControl_Calcurate
 * @brief 입력된 오차로 조정값을 계산한다.
 * @param pstOwner PidControl 자신
 * @param fErr 오차
 * @return 조정값
 */
float pifPidControl_Calcurate(PIF_stPidControl *pstOwner, float fErr)
{
	float Up;			// Variable: Proportional output
	float Ui;			// Variable: Integral output
	float Ud;			// Variable: Derivative output
	float Ed;
	float Out;   		// Output: PID output

	// Compute the error sum
	pstOwner->_stPrivate.fErrSum = pstOwner->_stPrivate.fErrSum + fErr;

	// Compute the proportional output
	Up = pstOwner->fFsKp * fErr;

	// Compute the integral output
	Ui = pstOwner->fFsKi * pstOwner->_stPrivate.fErrSum;
	if (Ui > pstOwner->fMaxIntegration) 				Ui = pstOwner->fMaxIntegration;
	else if (Ui < (-1.0 * pstOwner->fMaxIntegration))	Ui = -1.0 * pstOwner->fMaxIntegration;

	// Compute the derivative output
	Ed = fErr - pstOwner->_stPrivate.fErrPrev;
	Ud = ((fErr > 0 && Ed > 0) || (fErr < 0 && Ed < 0)) ? pstOwner->fFsKd * Ed : 0;

	// Compute the pre-saturated output
	Out = Up + Ui + Ud;

	pstOwner->_stPrivate.fErrPrev = fErr;

	return Out;
}
