#include "pif_sensor.h"

void pifSensor_AttachEvtChange(PifSensor* p_owner, PifEvtSensorChange evt_change)
{
	p_owner->__evt_change = evt_change;
}

void pifSensor_DetachEvtChange(PifSensor* p_owner)
{
	p_owner->__evt_change = NULL;
}
