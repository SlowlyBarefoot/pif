// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_PID_CONTROL_H
#define PIF_PID_CONTROL_H


#include "core/pif.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


/*
 * PID controller.
 *
 * pifPidControl_Calculate() is the original, sample-count form: it takes the error alone, sums it
 * without a time step and limits the integral with max_integration.
 *
 * pifPidControl_Update() is the full form, with the time step, and the following, each off until
 * set:
 *   - derivative on measurement instead of on error, so that a step of the setpoint does not kick
 *     the D term (pifPidControl_SetDerivativeOnMeasurement()),
 *   - a first or second-order low-pass filter on the D term (pifPidControl_SetDtermFilter()),
 *   - output limits (pifPidControl_SetOutputLimit()) with anti-windup by conditional integration
 *     or by back-calculation (pifPidControl_SetAntiWindup()); max_integration still limits the
 *     I term,
 *   - feedforward of the setpoint and of its rate of change (pifPidControl_SetFeedforward()).
 * The I term is kept as the integral of ki * error, so ki can be changed while running without a
 * jump of the output. The two forms keep separate state and are not meant to be mixed.
 */


typedef enum EnPifPidAntiWindup
{
	PID_AW_NONE				= 0,	// Integrate always; max_integration still limits the I term
	PID_AW_CLAMP			= 1,	// Stop integrating while the output is saturated in the error's direction
	PID_AW_BACK_CALCULATION	= 2		// Bleed the I term by tracking_gain * (limited - unlimited output)
} PifPidAntiWindup;

#ifdef PIF_COLLECT_SIGNAL

// Every channel is CSVT_REAL. pifPidControl_Calculate() records the error, the output and the
// terms. pifPidControl_Update() records them all.
typedef enum EnPifPidControlCsFlag
{
	PID_CSF_OFF				= 0,

	PID_CSF_SETPOINT_IDX	= 0,
	PID_CSF_MEASUREMENT_IDX	= 1,
	PID_CSF_ERROR_IDX		= 2,
	PID_CSF_OUTPUT_IDX		= 3,
	PID_CSF_P_TERM_IDX		= 4,
	PID_CSF_I_TERM_IDX		= 5,
	PID_CSF_D_TERM_IDX		= 6,

	PID_CSF_SETPOINT_BIT	= 0x01,
	PID_CSF_MEASUREMENT_BIT	= 0x02,
	PID_CSF_ERROR_BIT		= 0x04,
	PID_CSF_OUTPUT_BIT		= 0x08,
	PID_CSF_P_TERM_BIT		= 0x10,
	PID_CSF_I_TERM_BIT		= 0x20,
	PID_CSF_D_TERM_BIT		= 0x40,
	PID_CSF_TERM_BIT		= 0x70,
	PID_CSF_ALL_BIT			= 0x7F,

	PID_CSF_COUNT			= 7
} PifPidControlCsFlag;

#endif	// PIF_COLLECT_SIGNAL


/**
 * @struct StPifPidControl
 * @brief Represents the pid control data structure used by this module.
 */
typedef struct StPifPidControl
{
	float kp;				// Proportional gain
	float ki;				// Integral gain (per second in pifPidControl_Update())
	float kd; 		    	// Derivative gain (seconds in pifPidControl_Update())
    float max_integration;	// Maximum Integration: largest absolute I term, 0 for no limit in pifPidControl_Update()

	float err_sum;		    // Variable: Error Sum
	float err_prev;	   		// History: Previous error

	// Read-only Member Variable, pifPidControl_Update() only
	float _p_term;
	float _i_term;
	float _d_term;
	float _f_term;
	float _output;
	BOOL _saturated;		// The last output was limited

	// Private Member Variable
	BOOL __primed;			// The history below holds a previous sample
	float __prev_error;
	float __prev_measurement;
	float __prev_setpoint;
	BOOL __d_on_measurement;
	uint8_t __d_order;		// 0 for no D-term filter
	float __d_cutoff_hz;
	float __d_state[2];
	float __out_min;
	float __out_max;
	BOOL __limited;
	PifPidAntiWindup __anti_windup;
	float __tracking_gain;
	float __kf;
	float __kfd;
	float __f_cutoff_hz;
	float __f_state;
#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[PID_CSF_COUNT];
#endif
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
 * @fn pifPidControl_Clear
 * @brief Releases what the instance holds. Call it before the instance is freed or goes out of
 *        scope.
 * @param p_owner Pointer to the target object instance.
 */
void pifPidControl_Clear(PifPidControl *p_owner);

/**
 * @fn pifPidControl_Calculate
 * @brief Executes the pifPidControl_Calculate operation for the pid control module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @param err Current control error value.
 * @return Result value returned by this API.
 */
float pifPidControl_Calculate(PifPidControl *p_owner, float err);

/**
 * @fn pifPidControl_SetDerivativeOnMeasurement
 * @brief Takes the D term from the change of the measurement instead of the error, for
 *        pifPidControl_Update(). The two differ only by the change of the setpoint, which is what
 *        kicks the output when the setpoint steps.
 * @param p_owner Pointer to the target object instance.
 * @param enable TRUE for derivative on measurement, FALSE for derivative on error.
 */
void pifPidControl_SetDerivativeOnMeasurement(PifPidControl *p_owner, BOOL enable);

/**
 * @fn pifPidControl_SetDtermFilter
 * @brief Sets a low-pass filter on the D term, for pifPidControl_Update().
 * @param p_owner Pointer to the target object instance.
 * @param cutoff_hz Cutoff frequency in Hz, or 0 for no filter.
 * @param order 1 or 2 identical first-order stages, 3 dB down at cutoff_hz either way.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPidControl_SetDtermFilter(PifPidControl *p_owner, float cutoff_hz, uint8_t order);

/**
 * @fn pifPidControl_SetOutputLimit
 * @brief Limits the output of pifPidControl_Update().
 * @param p_owner Pointer to the target object instance.
 * @param min Lowest output.
 * @param max Highest output, above min.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPidControl_SetOutputLimit(PifPidControl *p_owner, float min, float max);

/**
 * @fn pifPidControl_ClearOutputLimit
 * @brief Removes the output limits.
 * @param p_owner Pointer to the target object instance.
 */
void pifPidControl_ClearOutputLimit(PifPidControl *p_owner);

/**
 * @fn pifPidControl_SetAntiWindup
 * @brief Chooses how the I term is kept from winding up while the output is limited.
 * @param p_owner Pointer to the target object instance.
 * @param mode Anti-windup method.
 * @param tracking_gain For PID_AW_BACK_CALCULATION, how fast the I term follows the limit, per
 *        second. Around ki / kp is a common start. Ignored otherwise.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPidControl_SetAntiWindup(PifPidControl *p_owner, PifPidAntiWindup mode, float tracking_gain);

/**
 * @fn pifPidControl_SetFeedforward
 * @brief Adds kf * setpoint + kfd * (rate of change of the setpoint) to the output of
 *        pifPidControl_Update(), so that the output moves with the command before an error builds
 *        up.
 * @param p_owner Pointer to the target object instance.
 * @param kf Static feedforward gain, 0 for none.
 * @param kfd Rate feedforward gain in seconds, 0 for none.
 * @param cutoff_hz Low-pass cutoff on the setpoint rate in Hz, or 0 for none.
 */
void pifPidControl_SetFeedforward(PifPidControl *p_owner, float kf, float kfd, float cutoff_hz);

/**
 * @fn pifPidControl_Reset
 * @brief Clears the I term and the history of pifPidControl_Update(), and the error sum of
 *        pifPidControl_Calculate().
 * @param p_owner Pointer to the target object instance.
 */
void pifPidControl_Reset(PifPidControl *p_owner);

/**
 * @fn pifPidControl_Update
 * @brief Runs one step of the full PID.
 * @param p_owner Pointer to the target object instance.
 * @param setpoint Desired value.
 * @param measurement Measured value.
 * @param dt Time since the previous step in seconds. The D term and the setpoint rate are 0 on the
 *        first step after an init or reset, and whenever dt is not positive.
 * @return Output, within the limits if set.
 */
float pifPidControl_Update(PifPidControl *p_owner, float setpoint, float measurement, float dt);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifPidControl_SetCsFlag
 * @brief Adds the selected signals of the controller to pifCollectSignal as channels.
 * @details A controller runs every control period, so a capture fills fast. Keep it short, or
 *          use CSO_KEEP_LAST to keep the end of it.
 * @param p_owner Pointer to the controller.
 * @param flag Bit mask of the signals to add.
 * @param id Appended to the names to tell controllers apart, or PIF_ID_AUTO for none.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPidControl_SetCsFlag(PifPidControl *p_owner, PifPidControlCsFlag flag, PifId id);

/**
 * @fn pifPidControl_ResetCsFlag
 * @brief Removes the selected signals of the controller from pifCollectSignal.
 * @param p_owner Pointer to the controller.
 * @param flag Bit mask of the signals to remove.
 */
void pifPidControl_ResetCsFlag(PifPidControl *p_owner, PifPidControlCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL


#ifdef __cplusplus
}
#endif


#endif  // PIF_PID_CONTROL_H
