#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "pif_comm.h"
#include "pif_timer.h"


#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			32
#endif

#define PIF_GPS_LAT  	0		// Latitude (위도)
#define PIF_GPS_LON  	1		// Longitude (경도)

#define PIF_GPS_NMEA_MSG_ID_NONE	0

#define PIF_GPS_NMEA_MSG_ID_DTM		1
#define PIF_GPS_NMEA_MSG_ID_GBS		2
#define PIF_GPS_NMEA_MSG_ID_GGA		3
#define PIF_GPS_NMEA_MSG_ID_GLL		4
#define PIF_GPS_NMEA_MSG_ID_GNS		5
#define PIF_GPS_NMEA_MSG_ID_GRS		6
#define PIF_GPS_NMEA_MSG_ID_GSA		7
#define PIF_GPS_NMEA_MSG_ID_GST		8
#define PIF_GPS_NMEA_MSG_ID_GSV		9
#define PIF_GPS_NMEA_MSG_ID_RMC		10
#define PIF_GPS_NMEA_MSG_ID_THS		11
#define PIF_GPS_NMEA_MSG_ID_TXT		12
#define PIF_GPS_NMEA_MSG_ID_VLW		13
#define PIF_GPS_NMEA_MSG_ID_VTG		14
#define PIF_GPS_NMEA_MSG_ID_ZDA		15

#define PIF_GPS_NMEA_MSG_ID_MAX		15


typedef uint8_t PifGpsNmeaMsgId;

typedef struct StPifGps PifGps;

typedef void (*PifEvtGpsReceive)(PifGps* p_owner);
typedef void (*PifEvtGpsTimeout)(PifGps* p_owner);

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
    PifGpsNmeaMsgId evt_nmea_msg_id;

    // Private Event Function
	PifEvtGpsReceive evt_receive;

    // Read-only Member Variable
	PifId _id;
    PifDateTime _date_time;
	PifDeg _coord_deg[2];			// latitude, longitude	- unit: degree
	double _altitude;       		// altitude      		- unit: meter
	double _ground_speed;      		// ground speed         - unit: cm/s
	double _ground_course;			//                   	- unit: degree
	uint32_t _horizontal_acc;       // Horizontal accuracy estimate (mm)
	uint32_t _vertical_acc;         // Vertical accuracy estimate (mm)
	uint8_t _numCh;                 // Number of channels
	uint8_t _svinfo_chn[32];        // Channel number
	uint8_t _svinfo_svid[32];       // Satellite ID
	uint8_t _svinfo_quality[32];	// Bitfield Qualtity
	uint8_t _svinfo_cno[32];        // Carrier to Noise Ratio (Signal Strength)
	uint32_t _update_rate[2];       // GPS coordinates updating rate (column 0 = last update time, 1 = current update ms)
	uint32_t _svinfo_rate[2];       // GPS svinfo updating rate (column 0 = last update time, 1 = current update ms)
	uint8_t _num_sat;
	BOOL _fix		: 1;
	BOOL _connect	: 1;

	// Private Member Variable
	PifTimer* __p_timer;

    // Private Event Function
	PifEvtGpsTimeout __evt_timeout;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGps_Init
 * @brief
 * @param p_owner
 * @param id
 */
BOOL pifGps_Init(PifGps* p_owner, PifId id);

/**
 * @fn pifGps_SetTimeout
 * @brief
 * @param p_owner
 * @param p_timer_manager
 * @param timeout
 * @param evt_timeout
 */
BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout);

/**
 * @fn pifGps_SendEvent
 * @brief
 * @param p_owner
 */
void pifGps_SendEvent(PifGps* p_owner);

/**
 * @fn pifGps_ConvertLatitude2DegMin
 * @brief
 * @param p_owner
 * @param p_deg_min
 */
void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @fn pifGps_ConvertLongitude2DegMin
 * @brief
 * @param p_owner
 * @param p_deg_min
 */
void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @fn pifGps_ConvertLatitude2DegMinSec
 * @brief
 * @param p_owner
 * @param p_deg_min_sec
 */
void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

/**
 * @fn pifGps_ConvertLongitude2DegMinSec
 * @brief
 * @param p_owner
 * @param p_deg_min_sec
 */
void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
