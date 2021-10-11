#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensor.h"


/**
 * @fn pifSensor_AttachAction
 * @brief
 * @param p_owner
 * @param act_acquire
 */
void pifSensor_AttachAction(PifSensor* p_owner, PifActSensorAcquire act_acquire)
{
	p_owner->__act_acquire = act_acquire;
}

/**
 * @fn pifSensor_AttachEvtChange
 * @brief
 * @param p_owner
 * @param evt_change
 * @param p_issuer
 */
void pifSensor_AttachEvtChange(PifSensor* p_owner, PifEvtSensorChange evt_change, void* p_issuer)
{
	p_owner->__evt_change = evt_change;
	p_owner->__p_change_issuer = p_issuer;
}

/**
 * @fn pifSensor_DetachEvtChange
 * @brief
 * @param p_owner
 */
void pifSensor_DetachEvtChange(PifSensor* p_owner)
{
	p_owner->__evt_change = NULL;
	p_owner->__p_change_issuer = NULL;
}
