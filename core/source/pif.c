#include <math.h>
#include <string.h>

#include "pif.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


PIF_enError pif_enError = E_enSuccess;

volatile uint16_t pif_usTimer1ms = 0;
volatile uint32_t pif_unTimer1sec = 0;
volatile PIF_stDateTime pif_stDateTime;

PIF_stLogFlag pif_stLogFlag;

static volatile uint8_t s_ucPerformaceStep = 0;
static volatile BOOL s_bPerformaceLock = FALSE;
static volatile uint32_t s_unPerformanceCount = 0;
static volatile uint32_t s_unPerformanceMeasure = 0;

static uint8_t ucCrc7;

const uint8_t c_ucDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };


/**
 * @fn pif_Init
 * @brief pif의 전역 변수를 초기화한다.
 */
void pif_Init()
{
    memset(&pif_stLogFlag, 0, sizeof(pif_stLogFlag));
    s_ucPerformaceStep = 1;
}

/**
 * @fn pif_Loop
 * @brief Main loop에서 수행해야 하는 pif 함수이다.
 */
void pif_Loop()
{
    if (s_ucPerformaceStep == 2) {
        s_ucPerformaceStep = 1;
#ifndef __PIF_NO_LOG__
        if (pif_stLogFlag.btPerformance) {
        	double fValue = 1000000.0 / s_unPerformanceMeasure;
        	pifLog_Printf(LT_enInfo, "Performance: %u, %2fus", s_unPerformanceMeasure, fValue);
        }
#endif
    }
    else {
        s_bPerformaceLock = TRUE;
        s_unPerformanceCount++;
        s_bPerformaceLock = FALSE;
    }
}

/**
 * @fn pif_sigTimer1ms
 * @brief 1ms 타이머 인터럽트에서 실행할 pif 함수이다.
 */
void pif_sigTimer1ms()
{
	uint8_t days;

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

    	if (s_ucPerformaceStep == 1 && !s_bPerformaceLock) {
            s_unPerformanceMeasure = s_unPerformanceCount;
            s_ucPerformaceStep = 2;
            s_unPerformanceCount = 0;
        }
    }
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
