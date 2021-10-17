#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "pifComm.h"


#define GPS_LAT  0		// Latitude (위도)
#define GPS_LON  1		// Longitude (경도)


typedef struct StPifGps PifGps;

typedef void (*PifEvtGpsReceive)(PifGps* p_owner);

typedef double PifDeg;

typedef struct StPifDegMin
{
	uint16_t degree;
	double minute;
} PifDegMin;

typedef struct StPifDegMinSec
{
	uint16_t degree;
	uint16_t minute;
	double second;
} PifDegMinSec;

/**
 * @class StPifGps
 * @brief
 */
struct StPifGps
{
	// Public Member Variable

    // Read-only Member Variable
	PifId _id;
    PifDateTime _date_time;
	PifDeg _coord_deg[2];		// latitude, longitude	- unit: degree
	double _altitude;       	// altitude      		- unit: meter
	double _speed_n;          	// speed         		- unit: knots
	double _speed_k;          	// speed         		- unit: km/h
	double _ground_course;		//                   	- unit: degree
	uint8_t _num_sat;
	uint8_t _fix;

	// Private Member Variable

    // Private Event Function
	PifEvtGpsReceive __evt_receive;
};


#ifdef __cplusplus
extern "C" {
#endif

PifGps* pifGps_Create(PifId id);
void pifGps_Destroy(PifGps** pp_owner);

BOOL pifGps_Init(PifGps* p_owner, PifId id);

void pifGps_AttachEvent(PifGps* p_owner, PifEvtGpsReceive evt_receive);

void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);
void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);
void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

double pifGps_ConvertKnots2MpS(double knots);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
