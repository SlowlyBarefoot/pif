// SPDX-License-Identifier: BSD-3-Clause
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

BOOL pifSensor_ChangeState(PifSensor *p_owner, SWITCH state, PifSensorValueP p_value)
{
	if (state == p_owner->_curr_state) return FALSE;

	p_owner->_curr_state = state;
	if (p_owner->__evt_change) {
		(*p_owner->__evt_change)(p_owner, state, p_value, p_owner->__p_issuer);
	}
	return TRUE;
}

SWITCH pifSensor_Hysteresis(SWITCH state, int32_t level, int32_t low_threshold, int32_t high_threshold)
{
	if (state) return level < low_threshold ? OFF : ON;
	return level >= high_threshold ? ON : OFF;
}
