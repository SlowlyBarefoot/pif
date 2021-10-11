#include "pifGps.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


/**
 * @fn pifGps_Create
 * @brief
 * @param id
 * @return
 */
PifGps* pifGps_Create(PifId id)
{
	PifGps *p_owner = calloc(sizeof(PifGps), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

	pifGps_Init(p_owner, id);
    return p_owner;
}

/**
 * @fn pifGps_Destroy
 * @brief
 * @param pp_owner
 */
void pifGps_Destroy(PifGps** pp_owner)
{
	if (*pp_owner) {
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifGps_Init
 * @brief
 * @param p_owner
 * @param id
 */
void pifGps_Init(PifGps* p_owner, PifId id)
{
	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
}

/**
 * @fn pifGps_AttachEvent
 * @brief
 * @param p_owner
 * @param evt_receive
 */
void pifGps_AttachEvent(PifGps* p_owner, PifEvtGpsReceive evt_receive)
{
	p_owner->__evt_receive = evt_receive;
}

/**
 * @fn pifGps_ConvertLatitude2DegMin
 * @brief
 * @param p_owner
 * @param p_deg_min
 */
void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[GPS_LAT];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

/**
 * @fn pifGps_ConvertLongitude2DegMin
 * @brief
 * @param p_owner
 * @param p_deg_min
 */
void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min)
{
	double degree, minute;

	degree = p_owner->_coord_deg[GPS_LON];
	p_deg_min->degree = degree;
	minute = (degree - p_deg_min->degree) * 60;
	p_deg_min->minute = minute;
}

/**
 * @fn pifGps_ConvertLatitude2DegMinSec
 * @brief
 * @param p_owner
 * @param p_deg_min_sec
 */
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

/**
 * @fn pifGps_ConvertLongitude2DegMinSec
 * @brief
 * @param p_owner
 * @param p_deg_min_sec
 */
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

/**
 * @fn pifGps_ConvertKnots2MpS
 * @brief
 * @param knots
 * @return
 */
double pifGps_ConvertKnots2MpS(double knots)
{
	return knots * 0.514444;		// m/s = 1 knots * 0.514444
}
