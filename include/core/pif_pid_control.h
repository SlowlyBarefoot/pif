#ifndef PIF_PID_CONTROL_H
#define PIF_PID_CONTROL_H


/**
 * @struct StPifPidControl
 * @brief Represents the pid control data structure used by this module.
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
 * @brief Initializes the pid control instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param kp Proportional gain coefficient.
 * @param ki Integral gain coefficient.
 * @param kd Derivative gain coefficient.
 * @param max_integration Maximum absolute value allowed for the integral term.
 */
void pifPidControl_Init(PifPidControl *p_owner, float kp, float ki, float kd, float max_integration);

/**
 * @fn pifPidControl_Calcurate
 * @brief Executes the pifPidControl_Calcurate operation for the pid control module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param err Current control error value.
 * @return Result value returned by this API.
 */
float pifPidControl_Calcurate(PifPidControl *p_owner, float err);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PID_CONTROL_H
