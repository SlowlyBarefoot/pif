#include "sensor/pif_sensor.h"


void pifSensor_AttachEvtChange(PifSensor *p_owner, PifEvtSensorChange evt_change, PifIssuerP p_issuer)
{
	p_owner->__evt_change = evt_change;
	p_owner->__p_issuer = p_issuer;
}

void pifSensor_DetachEvtChange(PifSensor *p_owner)
{
	p_owner->__evt_change = NULL;
	p_owner->__p_issuer = NULL;
}
