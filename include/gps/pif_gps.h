#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "communication/pif_uart.h"
#include "core/pif_timer.h"


#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			32
#endif

#ifndef PIF_GPS_NMEA_TEXT_SIZE
#define PIF_GPS_NMEA_TEXT_SIZE			64
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

#define PIF_GPS_NMEA_MSG_ID_ERR		255

#ifndef PIF_GPS_SV_MAXSATS
#define PIF_GPS_SV_MAXSATS			16
#endif


typedef struct StPifGpsNmeaTxt
{
	uint8_t total;
	uint8_t num;
	uint8_t type;
	char text[PIF_GPS_NMEA_TEXT_SIZE];
} PifGpsNmeaTxt;

typedef uint8_t PifGpsNmeaMsgId;

typedef struct StPifGps PifGps;

typedef BOOL (*PifEvtGpsNmeaReceive)(PifGps* p_owner, PifGpsNmeaMsgId msg_id);
typedef void (*PifEvtGpsReceive)(PifGps* p_owner);
typedef void (*PifEvtGpsTimeout)(PifGps* p_owner);

typedef void (*PifEvtGpsNmeaText)(PifGpsNmeaTxt* p_txt);
typedef void (*PifEvtGpsNmeaFrame)(char* p_frame);

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

    // Private Event Function
    PifEvtGpsNmeaReceive evt_nmea_receive;
    PifEvtGpsReceive evt_receive;
    PifEvtGpsNmeaFrame evt_frame;

    // Read-only Member Variable
	PifId _id;
    PifDateTime _utc;
	PifDeg _coord_deg[2];			// latitude, longitude	- unit: degree
	double _altitude;       		// altitude      		- unit: meter
	double _ground_speed;      		// ground speed         - unit: cm/s
	double _ground_course;			//                   	- unit: degree
	uint32_t _horizontal_acc;       // Horizontal accuracy estimate (mm)
	uint32_t _vertical_acc;         // Vertical accuracy estimate (mm)
	uint32_t _update_rate[2];       // GPS coordinates updating rate (column 0 = last update time, 1 = current update ms)
	uint8_t _num_sat;
	BOOL _fix		: 1;
	BOOL _connect	: 1;
	uint8_t _sv_num_sv;
	uint8_t _sv_chn[PIF_GPS_SV_MAXSATS];     // Channel number
	uint8_t _sv_svid[PIF_GPS_SV_MAXSATS];    // Satellite ID
	uint8_t _sv_quality[PIF_GPS_SV_MAXSATS]; // Bitfield Qualtity
	uint8_t _sv_cno[PIF_GPS_SV_MAXSATS];     // Carrier to Noise Ratio (Signal Strength)
	uint32_t _sv_received_count;

	// Private Member Variable
	PifTimer* __p_timer;
    PifGpsNmeaTxt* __p_txt;
	uint8_t __sv_msg_num;
	PifGpsNmeaMsgId __msg_id;
	uint8_t __param, __offset, __parity;
	char __string[PIF_GPS_NMEA_VALUE_SIZE];
	uint8_t __checksum_param;

    // Private Event Function
	PifEvtGpsTimeout __evt_timeout;
    PifEvtGpsNmeaText __evt_text;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGps_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifGps_Init(PifGps* p_owner, PifId id);

/**
 * @fn pifGps_Clear
 * @brief
 * @param p_owner
 */
void pifGps_Clear(PifGps* p_owner);

/**
 * @fn pifGps_SetTimeout
 * @brief
 * @param p_owner
 * @param p_timer_manager
 * @param timeout
 * @param evt_timeout
 * @return
 */
BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout);

/**
 * @fn pifGps_SetEventNmeaText
 * @brief
 * @param p_owner
 * @param evt_text
 * @return
 */
BOOL pifGps_SetEventNmeaText(PifGps* p_owner, PifEvtGpsNmeaText evt_text);

/**
 * @fn pifGps_SendEvent
 * @brief
 * @param p_owner
 */
void pifGps_SendEvent(PifGps* p_owner);

/**
 * @fn pifGps_ParsingNmea
 * @brief
 * @param p_owner
 * @param c
 */
void pifGps_ParsingNmea(PifGps* p_owner, uint8_t c);

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
