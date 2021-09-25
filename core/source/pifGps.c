#include "pifGps.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


/**
 * @fn pifGps_Create
 * @brief
 * @param usPifId
 * @return
 */
PIF_stGps *pifGps_Create(PIF_usId usPifId)
{
	PIF_stGps *pstOwner = calloc(sizeof(PIF_stGps), 1);
	if (!pstOwner) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

	pifGps_Init(pstOwner, usPifId);
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPS:%u EC:%d", __LINE__, pif_enError);
#endif
    return NULL;
}

/**
 * @fn pifGps_Destroy
 * @brief
 * @param ppstOwner
 */
void pifGps_Destroy(PIF_stGps **ppstOwner)
{
	if (*ppstOwner) {
		free(*ppstOwner);
		*ppstOwner = NULL;
	}
}

/**
 * @fn pifGps_Init
 * @brief
 * @param pstOwner
 * @param usPifId
 */
void pifGps_Init(PIF_stGps *pstOwner, PIF_usId usPifId)
{
	if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
	pstOwner->_usPifId = usPifId;
}

/**
 * @fn pifGps_AttachEvent
 * @brief
 * @param pstOwner
 * @param evtReceive
 */
void pifGps_AttachEvent(PIF_stGps *pstOwner, PIF_evtGpsReceive evtReceive)
{
	pstOwner->__evtReceive = evtReceive;
}

/**
 * @fn pifGps_ConvertLatitude2DegMin
 * @brief
 * @param pstOwner
 * @param pstDegMin
 */
void pifGps_ConvertLatitude2DegMin(PIF_stGps *pstOwner, PIF_stDegMin *pstDegMin)
{
	double degree, minute;

	degree = pstOwner->_dCoordDeg[GPS_LAT];
	pstDegMin->usDegree = degree;
	minute = (degree - pstDegMin->usDegree) * 60;
	pstDegMin->dMinute = minute;
}

/**
 * @fn pifGps_ConvertLongitude2DegMin
 * @brief
 * @param pstOwner
 * @param pstDegMin
 */
void pifGps_ConvertLongitude2DegMin(PIF_stGps *pstOwner, PIF_stDegMin *pstDegMin)
{
	double degree, minute;

	degree = pstOwner->_dCoordDeg[GPS_LON];
	pstDegMin->usDegree = degree;
	minute = (degree - pstDegMin->usDegree) * 60;
	pstDegMin->dMinute = minute;
}

/**
 * @fn pifGps_ConvertLatitude2DegMinSec
 * @brief
 * @param pstOwner
 * @param pstDegMinSec
 */
void pifGps_ConvertLatitude2DegMinSec(PIF_stGps *pstOwner, PIF_stDegMinSec *pstDegMinSec)
{
	double degree, minute, second;

	degree = pstOwner->_dCoordDeg[GPS_LAT];
	pstDegMinSec->usDegree = degree;
	minute = (degree - pstDegMinSec->usDegree) * 60;
	pstDegMinSec->usMinute = minute;
	second = (minute - pstDegMinSec->usMinute) * 60;
	pstDegMinSec->dSecond = second;
}

/**
 * @fn pifGps_ConvertLongitude2DegMinSec
 * @brief
 * @param pstOwner
 * @param pstDegMinSec
 */
void pifGps_ConvertLongitude2DegMinSec(PIF_stGps *pstOwner, PIF_stDegMinSec *pstDegMinSec)
{
	double degree, minute, second;

	degree = pstOwner->_dCoordDeg[GPS_LON];
	pstDegMinSec->usDegree = degree;
	minute = (degree - pstDegMinSec->usDegree) * 60;
	pstDegMinSec->usMinute = minute;
	second = (minute - pstDegMinSec->usMinute) * 60;
	pstDegMinSec->dSecond = second;
}

/**
 * @fn pifGps_ConvertKnots2MpS
 * @brief
 * @param dKnots
 * @return
 */
double pifGps_ConvertKnots2MpS(double dKnots)
{
	return dKnots * 0.514444;		// m/s = 1 knots * 0.514444
}
