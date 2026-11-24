// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_gps.h
 * @brief Base of the GPS receivers: position, speed, time, satellites and the NMEA 0183 parser.
 *
 * pif_gps_nmea and pif_gps_ublox embed a PifGps as their first member. The fix is kept in the read-only
 * members and reported through evt_receive, and a timeout marks the receiver disconnected when it stops
 * sending.
 *
 * An NMEA 0183 sentence is "$", a five character address field of a two character talker and a three
 * character sentence formatter, the fields separated by commas, "*", the two hex digits of the checksum
 * and CR LF. The checksum is the exclusive or of every character between "$" and "*".
 *
 * Each field is taken in as it arrives, so the members hold what the sentence said before its checksum
 * is known. What the checksum decides is whether the fix is handed on: a sentence that does not check
 * out is reported as PIF_GPS_NMEA_MSG_ID_ERR, and one that carries no checksum at all is not reported,
 * and neither leads to evt_receive.
 *
 * An empty field leaves the member it would fill alone, which is what a receiver that has lost its fix
 * sends, so the last position stays readable while _fix says it is no longer current.
 */
#ifndef PIF_GPS_H
#define PIF_GPS_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"


/// Longest field of an NMEA sentence. A field is kept up to three characters short of this, which leaves
/// room for the separator and the terminator that evt_frame hands over. What is over the length is
/// dropped, and still counts towards the checksum.
#ifndef PIF_GPS_NMEA_VALUE_SIZE
#define PIF_GPS_NMEA_VALUE_SIZE			64
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
#define PIF_GPS_SV_MAXSATS			16	///< Most satellites kept in the satellite view
#endif


/**
 * @brief One TXT sentence.
 */
typedef struct StPifGpsNmeaTxt
{
	uint8_t total;						///< Number of sentences of the message
	uint8_t num;						///< Number of this sentence
	uint8_t type;						///< Type: 0 error, 1 warning, 2 notice, 7 user
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
typedef BOOL (*PifEvtGpsNmeaReceive)(PifGps *p_owner, PifGpsNmeaMsgId msg_id);
/**
 * @brief Called when a new fix is in the read-only members.
 */
typedef void (*PifEvtGpsReceive)(PifGps *p_owner);
/**
 * @brief Called when no data came for the timeout. The receiver is marked disconnected.
 */
typedef void (*PifEvtGpsTimeout)(PifGps *p_owner);

/**
 * @brief Gets a TXT sentence.
 */
typedef void (*PifEvtGpsNmeaText)(PifGpsNmeaTxt *p_txt);
/**
 * @brief Gets each raw NMEA sentence, for logging.
 *
 * It comes piece by piece as the parser takes the sentence in: "$", then the address field and every
 * field after it with the separator that ended it, then the end of the line.
 */
typedef void (*PifEvtGpsNmeaFrame)(char *p_frame);

/**
 * @brief Angle in degrees.
 */
typedef double PifDeg;

/**
 * @brief Angle in degrees and minutes. It holds the magnitude of a coordinate: the hemisphere stays with
 *        the sign of _coord_deg.
 */
typedef struct StPifDegMin
{
	uint16_t degree;	///< Degrees
	double minute;		///< Minutes
} PifDegMin;

/**
 * @brief Angle in degrees, minutes and seconds. It holds the magnitude of a coordinate: the hemisphere
 *        stays with the sign of _coord_deg.
 */
typedef struct StPifDegMinSec
{
	uint16_t degree;	///< Degrees
	uint16_t minute;	///< Minutes
	double second;		///< Seconds
} PifDegMinSec;

/**
 * @class StPifGps
 * @brief The fix of a GPS receiver, and the NMEA parser that fills it.
 */
struct StPifGps
{
	// Public Member Variable

    // Private Event Function
    PifEvtGpsNmeaReceive evt_nmea_receive;	///< May be NULL
    PifEvtGpsReceive evt_receive;			///< May be NULL
    PifEvtGpsNmeaFrame evt_frame;			///< May be NULL

    // Read-only Member Variable
	PifId _id;						///< Instance ID
    PifDateTime _utc;				///< UTC date and time of the fix
	PifDeg _coord_deg[2];			///< latitude, longitude	- unit: degree
	double _altitude;       		///< altitude above mean sea level - unit: meter
	double _ground_speed;      		///< speed over the ground - unit: cm/s
	double _ground_course;			///< course over ground	- unit: degree
	uint32_t _horizontal_acc;       ///< Horizontal accuracy estimate (mm). Only a UBX receiver reports it.
	uint32_t _vertical_acc;         ///< Vertical accuracy estimate (mm). Only a UBX receiver reports it.
	uint16_t _hdop;					///< Horizontal dilution of precision x 100
	uint32_t _update_rate[2];       ///< When the last two positions arrived, in pif_cumulative_timer1ms
	uint8_t _num_sat;				///< Satellites used in the fix
	BOOL _fix		: 1;			///< TRUE while the receiver has a fix
	BOOL _connect	: 1;			///< TRUE while data comes from the receiver
	uint8_t _sv_num_sv;							///< Satellites kept in the _sv_ arrays
	uint8_t _sv_chn[PIF_GPS_SV_MAXSATS];    	///< Channel number, which over NMEA is the satellite number
	uint8_t _sv_svid[PIF_GPS_SV_MAXSATS];   	///< Satellite ID
	uint8_t _sv_quality[PIF_GPS_SV_MAXSATS];	///< Quality bit field. Only a UBX receiver reports it.
	uint8_t _sv_cno[PIF_GPS_SV_MAXSATS];    	///< Carrier to noise ratio (signal strength) in dBHz
	uint32_t _sv_received_count;				///< Times the satellite view was updated, to tell a new one from the last

	// Private Member Variable
	PifTimer *__p_timer;
    PifGpsNmeaTxt *__p_txt;
	uint8_t __sv_msg_num;
	PifGpsNmeaMsgId __msg_id;
	uint8_t __param, __offset, __parity;
	char __string[PIF_GPS_NMEA_VALUE_SIZE];
	BOOL __in_checksum;

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
BOOL pifGps_Init(PifGps *p_owner, PifId id);

/**
 * @brief Releases the timeout timer and the TXT buffer of the GPS instance.
 * @param p_owner Pointer to the GPS instance to clear.
 */
void pifGps_Clear(PifGps *p_owner);

/**
 * @brief Configures or disables a receive timeout watchdog for GPS input.
 * @param p_owner Pointer to the GPS instance.
 * @param p_timer_manager Timer manager used to create the internal timeout timer.
 * @param timeout Timeout in milliseconds. Set to `0` to disable timeout handling.
 * @param evt_timeout Callback invoked when timeout expires.
 * @return `TRUE` if timer configuration succeeds, otherwise `FALSE`.
 */
BOOL pifGps_SetTimeout(PifGps *p_owner, PifTimerManager *p_timer_manager, uint32_t timeout, PifEvtGpsTimeout evt_timeout);

/**
 * @brief Enables TXT sentence parsing callback support. Calling it again only changes the callback.
 * @param p_owner Pointer to the GPS instance.
 * @param evt_text Callback invoked when a complete NMEA TXT message is parsed.
 * @return `TRUE` if the internal TXT buffer is allocated, otherwise `FALSE`.
 */
BOOL pifGps_SetEventNmeaText(PifGps *p_owner, PifEvtGpsNmeaText evt_text);

/**
 * @brief Marks the GPS as connected, hands the fix to evt_receive and restarts the timeout.
 * @param p_owner Pointer to the GPS instance.
 */
void pifGps_SendEvent(PifGps *p_owner);

/**
 * @brief Parses one NMEA character and updates GPS fields when a sentence completes.
 * @param p_owner Pointer to the GPS instance.
 * @param c One input character from an NMEA stream.
 * @return `TRUE` when a sentence with a matching checksum was completed, otherwise `FALSE`.
 */
BOOL pifGps_ParsingNmea(PifGps *p_owner, uint8_t c);

/**
 * @brief Converts latitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving the magnitude in degrees and minutes.
 */
void pifGps_ConvertLatitude2DegMin(PifGps *p_owner, PifDegMin *p_deg_min);

/**
 * @brief Converts longitude in decimal degrees to degree-minute format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min Output structure receiving the magnitude in degrees and minutes.
 */
void pifGps_ConvertLongitude2DegMin(PifGps *p_owner, PifDegMin *p_deg_min);

/**
 * @brief Converts latitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving the magnitude in degrees, minutes and seconds.
 */
void pifGps_ConvertLatitude2DegMinSec(PifGps *p_owner, PifDegMinSec *p_deg_min_sec);

/**
 * @brief Converts longitude in decimal degrees to degree-minute-second format.
 * @param p_owner Pointer to the GPS instance.
 * @param p_deg_min_sec Output structure receiving the magnitude in degrees, minutes and seconds.
 */
void pifGps_ConvertLongitude2DegMinSec(PifGps *p_owner, PifDegMinSec *p_deg_min_sec);

#ifdef __cplusplus
}
#endif


#endif	// PIF_GPS_H
