#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "pifComm.h"


#define GPS_LAT  0		// Latitude (위도)
#define GPS_LON  1		// Longitude (경도)


typedef struct _PIF_stGps PIF_stGps;

typedef void (*PIF_evtGpsRemove)(void *pvChild);
typedef void (*PIF_evtGpsReceive)(PIF_stGps *pstOwner);

typedef double PIF_dDeg;

typedef struct _PIF_stDegMin {
	uint16_t usDegree;
	double dMinute;
} PIF_stDegMin;

typedef struct _PIF_stDegMinSec {
	uint16_t usDegree;
	uint16_t usMinute;
	double dSecond;
} PIF_stDegMinSec;

/**
 * @class _PIF_stGps
 * @brief
 */
struct _PIF_stGps
{
	// Public Member Variable

    // Read-only Member Variable
    PIF_usId _usPifId;
    PIF_stDateTime _stDateTime;
	PIF_dDeg _dCoordDeg[2];		// latitude, longitude	- unit: degree
	double _dAltitude;       	// altitude      		- unit: meter
	double _dSpeedN;          	// speed         		- unit: knots
	double _dSpeedK;          	// speed         		- unit: km/h
	double _dGroundCourse;		//                   	- unit: degree
	uint8_t  _ucNumSat;
	uint8_t  _ucFix;
    void *_pvChild;
    PIF_stComm *_pstComm;

	// Private Member Variable

    // Private Event Function
	PIF_evtGpsRemove __evtRemove;
	PIF_evtGpsReceive __evtReceive;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifGps_Init(uint8_t ucSize);
void pifGps_Exit();

PIF_stGps *pifGps_Add(PIF_usId usPifId);

void pifGps_AttachEvent(PIF_stGps *pstOwner, PIF_evtGpsReceive evtReceive);

void pifGps_ConvertLatitude2DegMin(PIF_stGps *pstOwner, PIF_stDegMin *pstDegMin);
void pifGps_ConvertLongitude2DegMin(PIF_stGps *pstOwner, PIF_stDegMin *pstDegMin);

void pifGps_ConvertLatitude2DegMinSec(PIF_stGps *pstOwner, PIF_stDegMinSec *pstDegMinSec);
void pifGps_ConvertLongitude2DegMinSec(PIF_stGps *pstOwner, PIF_stDegMinSec *pstDegMinSec);

double pifGps_ConvertKnots2MpS(double dKnots);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
