#include "pif_gps.h"
#ifndef __PIF_NO_LOG__
#include "pif_log.h"
#endif


PifGps* pifGps_Create(PifId id)
{
	PifGps *p_owner = malloc(sizeof(PifGps));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

	if (!pifGps_Init(p_owner, id)) {
		pifGps_Destroy(&p_owner);
	    return NULL;
	}
    return p_owner;
}

void pifGps_Destroy(PifGps** pp_owner)
{
	if (*pp_owner) {
		free(*pp_owner);
		*pp_owner = NULL;
	}
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

void pifGps_AttachEvent(PifGps* p_owner, PifEvtGpsReceive evt_receive)
{
	p_owner->__evt_receive = evt_receive;
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
