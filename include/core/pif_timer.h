#ifndef PIF_TIMER_H
#define PIF_TIMER_H


#include "core/pif.h"


#ifndef PIF_PWM_MAX_DUTY
#define PIF_PWM_MAX_DUTY				1000
#endif


typedef void (*PifActTimerPwm)(SWITCH value);

typedef void (*PifEvtTimerFinish)(PifIssuerP p_issuer);


typedef enum EnPifTimerType
{
    TT_ONCE			= 0,
    TT_REPEAT		= 1,
	TT_PWM			= 2
} PifTimerType;

typedef enum EnPifTimerStep
{
    TS_STOP			= 0,
    TS_RUNNING		= 1,
    TS_REMOVE		= 2
} PifTimerStep;


/**
 * @struct StPifTimer
 * @brief Represents the timer data structure used by this module.
 */
typedef struct StPifTimer
{
	// Public Member Variable
    uint32_t target;

    // Public Action Function
    PifActTimerPwm act_pwm;

	// Read-only Member Variable
    PifTimerType _type;
    PifTimerStep _step;

	// Private Member Variable
    uint32_t __current;
    PifIssuerP __p_finish_issuer;
    uint32_t __pwm_duty;
    BOOL __event_into_int;
	BOOL __event;

    // Private Event Function
    PifEvtTimerFinish __evt_finish;
} PifTimer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifTimer_Start
 * @brief Starts the timer operation using the current timing, trigger, or mode configuration.
 * @param p_owner Pointer to the target object instance.
 * @param target Target tick count to run before completion.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifTimer_Start(PifTimer* p_owner, uint32_t pulse);

/**
 * @fn pifTimer_Stop
 * @brief Stops the timer operation and transitions it into an inactive state.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Stop(PifTimer* p_owner);

/**
 * @fn pifTimer_Reset
 * @brief Resets runtime state in the timer to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 */
void pifTimer_Reset(PifTimer* p_owner);

/**
 * @fn pifTimer_SetPwmDuty
 * @brief Sets configuration or runtime state for the timer based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param duty PWM duty ratio value.
 */
void pifTimer_SetPwmDuty(PifTimer* p_owner, uint16_t duty);

/**
 * @fn pifTimer_Remain
 * @brief Executes the pifTimer_Remain operation for the timer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @return Return value of this API.
 */
uint32_t pifTimer_Remain(PifTimer* p_owner);

/**
 * @fn pifTimer_Elapsed
 * @brief Executes the pifTimer_Elapsed operation for the timer module according to the API contract.
 * @param p_owner Pointer to the target object instance.
 * @return Return value of this API.
 */
uint32_t pifTimer_Elapsed(PifTimer* p_owner);

/**
 * @fn pifTimer_AttachEvtFinish
 * @brief Attaches a callback, device, or external resource to the timer for integration.
 * @param p_owner Pointer to the target object instance.
 * @param evt_finish Callback invoked when timer operation completes.
 * @param p_issuer User context pointer passed to callback functions.
 */
void pifTimer_AttachEvtFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer);

/**
 * @fn pifTimer_AttachEvtIntFinish
 * @brief Attaches a callback, device, or external resource to the timer for integration.
 * @param p_owner Pointer to the target object instance.
 * @param evt_finish Callback invoked when timer operation completes.
 * @param p_issuer User context pointer passed to callback functions.
 */
void pifTimer_AttachEvtIntFinish(PifTimer* p_owner, PifEvtTimerFinish evt_finish, PifIssuerP p_issuer);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TIMER_H
