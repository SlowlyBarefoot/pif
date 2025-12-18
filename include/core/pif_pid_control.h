#ifndef PIF_PID_CONTROL_H
#define PIF_PID_CONTROL_H


/**
 * @struct StPifPidControl
 * @brief PID Control을 계산하기 위한 구조체
 */
typedef struct StPifPidControl
{
	float kp;				// Proportional gain
	float ki;				// Integral gain
	float kd; 		    	// Derivative gain
    float max_integration;	// Maximum Integration

	float err_sum;		    // Variable: Error Sum
	float err_prev;	   		// History: Previous error
} PifPidControl;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPidControl_Init
 * @brief PID 컨트롤에 사용되는 전역 변수를 초기화한다.
 * @param p_owner PidControl 자신
 * @param kp 비례 계수
 * @param ki 적분 계수
 * @param kd 미분 계수
 * @param max_integration 최대 적분 오차값
 */
void pifPidControl_Init(PifPidControl *p_owner, float kp, float ki, float kd, float max_integration);

/**
 * @fn pifPidControl_Calcurate
 * @brief 입력된 오차로 조정값을 계산한다.
 * @param p_owner PidControl 자신
 * @param err 오차
 * @return 조정값
 */
float pifPidControl_Calcurate(PifPidControl *p_owner, float err);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PID_CONTROL_H
