#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensor.h"


void pifSensor_AttachAction(PifSensor* p_owner, PifActSensorAcquire act_acquire)
{
	p_owner->__act_acquire = act_acquire;
}

void pifSensor_AttachEvtChange(PifSensor* p_owner, PifEvtSensorChange evt_change, void* p_issuer)
{
	p_owner->__evt_change = evt_change;
	p_owner->__p_change_issuer = p_issuer;
}

void pifSensor_DetachEvtChange(PifSensor* p_owner)
{
	p_owner->__evt_change = NULL;
	p_owner->__p_change_issuer = NULL;
}
