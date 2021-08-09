#include "pifGps.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static PIF_stGps *s_pstGps;
static uint8_t s_ucGpsSize;
static uint8_t s_ucGpsPos;


/**
 * @fn pifGps_Init
 * @brief
 * @param ucSize
 * @return
 */
BOOL pifGps_Init(uint8_t ucSize)
{
    if (ucSize == 0) {
		pif_enError = E_enInvalidParam;
		goto fail;
	}

    s_pstGps = calloc(sizeof(PIF_stGps), ucSize);
    if (!s_pstGps) {
		pif_enError = E_enOutOfHeap;
		goto fail;
	}

    s_ucGpsSize = ucSize;
    s_ucGpsPos = 0;
    return TRUE;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPS:%u S:%u EC:%d", __LINE__, ucSize, pif_enError);
#endif
    return FALSE;
}

/**
 * @fn pifGps_Exit
 * @brief
 */
void pifGps_Exit()
{
    if (s_pstGps) {
		for (int i = 0; i < s_ucGpsSize; i++) {
			PIF_stGps *pstOwner = &s_pstGps[i];
			if (pstOwner->_pvChild) {
				if (pstOwner->__evtRemove) {
					(pstOwner->__evtRemove)(pstOwner->_pvChild);
				}
				free(pstOwner->_pvChild);
				pstOwner->_pvChild = NULL;
			}
		}
        free(s_pstGps);
        s_pstGps = NULL;
    }
}

/**
 * @fn pifGps_Add
 * @brief
 * @param usPifId
 * @return
 */
PIF_stGps *pifGps_Add(PIF_usId usPifId)
{
    PIF_stGps *pstOwner = NULL;

    if (s_ucGpsPos >= s_ucGpsSize) {
        pif_enError = E_enOverflowBuffer;
        goto fail;
    }

    pstOwner = &s_pstGps[s_ucGpsPos];

    if (usPifId == PIF_ID_AUTO) usPifId = pif_usPifId++;
    pstOwner->_usPifId = usPifId;

    s_ucGpsPos = s_ucGpsPos + 1;
    return pstOwner;

fail:
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_enError, "GPS:%u(%u) EC:%d", __LINE__, usPifId, pif_enError);
#endif
    return NULL;
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
