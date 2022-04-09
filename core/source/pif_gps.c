#include "pif_gps.h"


static void _evtTimerFinish(void* p_issuer)
{
    PifGps* p_owner = (PifGps*)p_issuer;

	p_owner->_connect = FALSE;
    p_owner->_fix = FALSE;
    p_owner->_num_sat = 0;
	if (p_owner->__evt_timeout) (*p_owner->__evt_timeout)(p_owner);
}

BOOL pifGps_Init(PifGps* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    memset(p_owner, 0, sizeof(PifGps));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	return TRUE;
}

BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout)
{
	p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
	if (!p_owner->__p_timer) return FALSE;
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner);
    if (!pifTimer_Start(p_owner->__p_timer, timeout)) return FALSE;
    p_owner->__evt_timeout = evt_timeout;
    return TRUE;
}

void pifGps_SendEvent(PifGps* p_owner)
{
	p_owner->_connect = TRUE;
	if (p_owner->evt_receive) (*p_owner->evt_receive)(p_owner);
	if (p_owner->__p_timer) pifTimer_Reset(p_owner->__p_timer);
}

void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[GPS_LAT];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[GPS_LON];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec)
{
	double degree, minute, second;

	degree = p_owner->_coord_deg[GPS_LAT];
	p_deg_min_sec->degree = degree;
	minute = (degree - p_deg_min_sec->degree) * 60;
	p_deg_min_sec->minute = minute;
	second = (minute - p_deg_min_sec->minute) * 60;
	p_deg_min_sec->second = second;
}

void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec)
{
	double degree, minute, second;

	degree = p_owner->_coord_deg[GPS_LON];
	p_deg_min_sec->degree = degree;
	minute = (degree - p_deg_min_sec->degree) * 60;
	p_deg_min_sec->minute = minute;
	second = (minute - p_deg_min_sec->minute) * 60;
	p_deg_min_sec->second = second;
}

double pifGps_ConvertKnots2MpS(double knots)
{
	return knots * 0.514444;		// m/s = 1 knots * 0.514444
}
