// SPDX-License-Identifier: GPL-3.0-or-later
// Portions derived from Betaflight/Cleanflight gps.c and gps_conversion.c (MultiWii lineage), GPL-3.0-or-later.
// See THIRD_PARTY_NOTICES.md.
/**
 * @file pif_gps.h
 * @brief Base of the GPS receivers: position, speed, time, satellites and the NMEA parser.
 *
 * pif_gps_nmea and pif_gps_ublox embed a PifGps as their first member. The fix is kept in the read-only members
 * and reported through evt_receive, and a timeout marks the receiver disconnected when it stops sending.
 */
#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"


#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			64	///< Longest field of an NMEA sentence
#endif

#ifndef PIF_GPS_NMEA_TEXT_SIZE
#define PIF_GPS_NMEA_TEXT_SIZE			64	///< Longest text of a TXT sentence
#endif

#define PIF_GPS_LAT  	0		///< Latitude index
#define PIF_GPS_LON  	1		///< Longitude index

/** @name NMEA sentence IDs
 *  Sentences the parser recognizes, given to evt_nmea_receive.
 *  @{
 */
#define PIF_GPS_NMEA_MSG_ID_NONE	0	///< No sentence

#define PIF_GPS_NMEA_MSG_ID_DTM		1	///< Datum reference
#define PIF_GPS_NMEA_MSG_ID_GBS		2	///< Fault detection
#define PIF_GPS_NMEA_MSG_ID_GGA		3	///< Fix data
#define PIF_GPS_NMEA_MSG_ID_GLL		4	///< Latitude and longitude
#define PIF_GPS_NMEA_MSG_ID_GNS		5	///< Fix data of several systems
#define PIF_GPS_NMEA_MSG_ID_GRS		6	///< Range residuals
#define PIF_GPS_NMEA_MSG_ID_GSA		7	///< DOP and active satellites
#define PIF_GPS_NMEA_MSG_ID_GST		8	///< Pseudo range error statistics
#define PIF_GPS_NMEA_MSG_ID_GSV		9	///< Satellites in view
#define PIF_GPS_NMEA_MSG_ID_RMC		10	///< Recommended minimum data
#define PIF_GPS_NMEA_MSG_ID_THS		11	///< True heading
#define PIF_GPS_NMEA_MSG_ID_TXT		12	///< Text message
#define PIF_GPS_NMEA_MSG_ID_VLW		13	///< Dual ground and water distance
#define PIF_GPS_NMEA_MSG_ID_VTG		14	///< Course over ground and ground speed
#define PIF_GPS_NMEA_MSG_ID_ZDA		15	///< Time and date

#define PIF_GPS_NMEA_MSG_ID_ERR		255	///< A sentence with a bad checksum
/** @} */

#ifndef PIF_GPS_SV_MAXSATS
#define PIF_GPS_SV_MAXSATS			16	///< Most satellites kept from GSV sentences
#endif


/**
 * @brief One TXT sentence.
 */
typedef struct StPifGpsNmeaTxt
{
	uint8_t total;	///< Number of sentences of the message
	uint8_t num;	///< Number of this sentence
	uint8_t type;	///< Type: 0 error, 1 warning, 2 notice, 7 user
	char text[PIF_GPS_NMEA_TEXT_SIZE];	///< Text
} PifGpsNmeaTxt;

/**
 * @brief ID of an NMEA sentence, one of PIF_GPS_NMEA_MSG_ID_.
 */
typedef uint8_t PifGpsNmeaMsgId;

typedef struct StPifGps PifGps;

/**
 * @brief Called after each NMEA sentence is parsed.
 *
 * Returns TRUE to have evt_receive called for it. Gets PIF_GPS_NMEA_MSG_ID_ERR for a sentence with a bad
 * checksum.
 */
typedef BOOL (*PifEvtGpsNmeaReceive)(PifGps* p_owner, PifGpsNmeaMsgId msg_id);
/**
 * @brief Called when a new fix is in the read-only members.
 */
typedef void (*PifEvtGpsReceive)(PifGps* p_owner);
/**
 * @brief Called when no data came for the timeout. The receiver is marked disconnected.
 */
typedef void (*PifEvtGpsTimeout)(PifGps* p_owner);

/**
 * @brief Gets a TXT sentence.
 */
typedef void (*PifEvtGpsNmeaText)(PifGpsNmeaTxt* p_txt);
/**
 * @brief Gets each raw NMEA sentence, for logging.
 */
typedef void (*PifEvtGpsNmeaFrame)(char* p_frame);

/**
 * @brief Angle in degrees.
 */
typedef double PifDeg;

/**
 * @brief Angle in degrees and minutes.
 */
typedef struct StPifDegMin
{
	uint16_t degree;	///< Degrees
	double minute;	///< Minutes
} PifDegMin;

/**
 * @brief Angle in degrees, minutes and seconds.
 */
typedef struct StPifDegMinSec
{
	uint16_t degree;	///< Degrees
	uint16_t minute;	///< Minutes
	double second;	///< Seconds
} PifDegMinSec;

/**
 * @class StPifGps
 * @brief Core GPS state container for NMEA/UBX parsers and event callbacks.
 */
struct StPifGps
{
	// Public Member Variable

    // Private Event Function
    PifEvtGpsNmeaReceive evt_nmea_receive;	///< May be NULL
    PifEvtGpsReceive evt_receive;	///< May be NULL
    PifEvtGpsNmeaFrame evt_frame;	///< May be NULL

    // Read-only Member Variable
	PifId _id;						///< Instance ID
    PifDateTime _utc;				///< UTC date and time of the fix
	PifDeg _coord_deg[2];			///< latitude, longitude	- unit: degree
	double _altitude;       		///< altitude      		- unit: meter
	double _ground_speed;      		///< ground speed         - unit: cm/s
	double _ground_course;			///<                   	- unit: degree
	uint32_t _horizontal_acc;       ///< Horizontal accuracy estimate (mm)
	uint32_t _vertical_acc;         ///< Vertical accuracy estimate (mm)
	uint16_t _hdop;					///< Horizontal dilution of precision x 100 (NMEA GGA)
	uint32_t _update_rate[2];       ///< GPS coordinates updating rate (column 0 = last update time, 1 = current update ms)
	uint8_t _num_sat;				///< Satellites used in the fix
	BOOL _fix		: 1;			///< TRUE while the receiver has a fix
	BOOL _connect	: 1;						///< TRUE while data comes from the receiver
	uint8_t _sv_num_sv;							///< Satellites kept in the _sv_ arrays
	uint8_t _sv_chn[PIF_GPS_SV_MAXSATS];     ///< Channel number
	uint8_t _sv_svid[PIF_GPS_SV_MAXSATS];    ///< Satellite ID
	uint8_t _sv_quality[PIF_GPS_SV_MAXSATS]; ///< Bitfield Qualtity
	uint8_t _sv_cno[PIF_GPS_SV_MAXSATS];     ///< Carrier to Noise Ratio (Signal Strength)
	uint32_t _sv_received_count;				///< Times the satellite view was updated, to tell a new one from the last

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
 * @brief Initializes a `PifGps` instance and assigns its identifier.
 * @param p_owner Pointer to the GPS instance to initialize.
 * @param id Requested object ID. Use `PIF_ID_AUTO` to allocate automatically.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifGps_Init(PifGps* p_owner, PifId id);

/**
 * @brief Releases dynamically allocated resources used by the GPS instance.
 * @param p_owner Pointer to the GPS instance to clear.
 */
void pifGps_Clear(PifGps* p_owner);

/**
 * @brief Configures or disables a receive timeout watchdog for GPS input.
 * @param p_owner Pointer to the GPS instance.
 * @param p_timer_manager Timer manager used to create the internal timeout timer.
 * @param timeout Timeout in milliseconds. Set to `0` to disable timeout handling.
 * @param evt_timeout Callback invoked when timeout expires.
 * @return `TRUE` if timer configuration succeeds, otherwise `FALSE`.
 */
BOOL pifGps_SetTimeout(PifGps* p_owner, PifTimerManager* p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout);

/**
 * @brief Enables TXT sentence parsing callback support.
 * @param p_owner Pointer to the GPS instance.
 * @param evt_text Callback invoked when a complete NMEA TXT message is parsed.
 * @return `TRUE` if the internal TXT buffer is allocated, otherwise `FALSE`.
 */
BOOL pifGps_SetEventNmeaText(PifGps* p_owner, PifEvtGpsNmeaText evt_text);

/**
 * @brief Marks the GPS as connected and notifies the receive callback.
 * @param p_owner Pointer to the GPS instance.
 */
void pifGps_SendEvent(PifGps* p_owner);

/**
 * @brief Parses one NMEA character and updates GPS fields when a frame completes.
 * @param p_owner Pointer to the GPS instance.
 * @param c One input character from an NMEA stream.
 * @return `TRUE` when a valid NMEA sentence was completed, otherwise `FALSE`.
 */
BOOL pifGps_ParsingNmea(PifGps* p_owner, uint8_t c);

/**
 * @brief Converts latitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving converted degree-minute data.
 */
void pifGps_ConvertLatitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @brief Converts longitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving converted degree-minute data.
 */
void pifGps_ConvertLongitude2DegMin(PifGps* p_owner, PifDegMin* p_deg_min);

/**
 * @brief Converts latitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving converted degree-minute-second data.
 */
void pifGps_ConvertLatitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

/**
 * @brief Converts longitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving converted degree-minute-second data.
 */
void pifGps_ConvertLongitude2DegMinSec(PifGps* p_owner, PifDegMinSec* p_deg_min_sec);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
