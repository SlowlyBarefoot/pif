#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "pif_comm.h"
#include "pif_gps.h"


#ifndef PIF_GPS_NMEA_TEXT_SIZE
#define PIF_GPS_NMEA_TEXT_SIZE			64
#endif


typedef struct StPifGpsNmeaTxt
{
	uint8_t total;
	uint8_t num;
	uint8_t type;
	char text[PIF_GPS_NMEA_TEXT_SIZE];
} PifGpsNmeaTxt;

typedef void (*PifEvtGpsNmeaText)(PifGpsNmeaTxt* p_txt);
typedef void (*PifEvtGpsNmeaFrame)(char* p_frame);

/**
 * @class StPifGpsNmea
 * @brief
 */
typedef struct StPifGpsNmea
{
	// Public Member Variable

	// Public Event Variable
    PifEvtGpsNmeaFrame evt_frame;

	// Read-only Member Variable
    PifGps _gps;

	// Private Member Variable
	PifComm* __p_comm;
    PifGpsNmeaTxt* __p_txt;

	// Private Event Variable
    PifEvtGpsNmeaText __evt_text;
} PifGpsNmea;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGpsNmea_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId usPifId);

/**
 * @fn pifGpsNmea_Clear
 * @brief
 * @param p_owner
 */
void pifGpsNmea_Clear(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifGpsNmea_AttachComm(PifGpsNmea* p_owner, PifComm* pstComm);

/**
 * @fn pifGpsNmea_DetachComm
 * @brief
 * @param p_owner
 */
void pifGpsNmea_DetachComm(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_SetEventText
 * @brief
 * @param p_owner
 * @param evt_text
 * @return
 */
BOOL pifGpsNmea_SetEventText(PifGpsNmea* p_owner, PifEvtGpsNmeaText evt_text);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
