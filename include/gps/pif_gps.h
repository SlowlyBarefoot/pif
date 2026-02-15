#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"


#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			64
#endif

#ifndef PIF_GPS_NMEA_TEXT_SIZE
#define PIF_GPS_NMEA_TEXT_SIZE			64
#endif

#define PIF_GPS_LAT  	0		// Latitude index
#define PIF_GPS_LON  	1		// Longitude index

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
 * @brief Core GPS state container for NMEA/UBX parsers and event callbacks.
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
 * @brief Initializes a `PifGps` instance and assigns its identifier.
 * @param p_owner Pointer to the GPS instance to initialize.
 * @param id Requested object ID. Use `PIF_ID_AUTO` to allocate automatically.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifGps_Init(PifGps* p_owner, PifId id);

/**
 * @fn pifGps_Clear
 * @brief Releases dynamically allocated resources used by the GPS instance.
 * @param p_owner Pointer to the GPS instance to clear.
 */
void pifGps_Clear(PifGps* p_owner);

/**
 * @fn pifGps_SetTimeout
 * @brief Configures or disables a receive timeout watchdog for GPS input.
 * @param p_owner Pointer to the GPS instance.
 * @param p_timer_manager Timer manager used to create the internal timeout timer.
 * @param timeout Timeout in milliseconds. Set to `0` to disable timeout handling.
 * @param evt_timeout Callback invoked when timeout expires.
 * @return `TRUE` if timer configuration succeeds, otherwise `FALSE`.
 */
BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout);

/**
 * @fn pifGps_SetEventNmeaText
 * @brief Enables TXT sentence parsing callback support.
 * @param p_owner Pointer to the GPS instance.
 * @param evt_text Callback invoked when a complete NMEA TXT message is parsed.
 * @return `TRUE` if the internal TXT buffer is allocated, otherwise `FALSE`.
 */
BOOL pifGps_SetEventNmeaText(PifGps* p_owner, PifEvtGpsNmeaText evt_text);

/**
 * @fn pifGps_SendEvent
 * @brief Marks the GPS as connected and notifies the receive callback.
 * @param p_owner Pointer to the GPS instance.
 */
void pifGps_SendEvent(PifGps* p_owner);

/**
 * @fn pifGps_ParsingNmea
 * @brief Parses one NMEA character and updates GPS fields when a frame completes.
 * @param p_owner Pointer to the GPS instance.
 * @param c One input character from an NMEA stream.
 * @return `TRUE` when a valid NMEA sentence was completed, otherwise `FALSE`.
 */
BOOL pifGps_ParsingNmea(PifGps* p_owner, uint8_t c);

/**
 * @fn pifGps_ConvertLatitude2DegMin
 * @brief Converts latitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving converted degree-minute data.
 */
void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @fn pifGps_ConvertLongitude2DegMin
 * @brief Converts longitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving converted degree-minute data.
 */
void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @fn pifGps_ConvertLatitude2DegMinSec
 * @brief Converts latitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving converted degree-minute-second data.
 */
void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

/**
 * @fn pifGps_ConvertLongitude2DegMinSec
 * @brief Converts longitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving converted degree-minute-second data.
 */
void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
