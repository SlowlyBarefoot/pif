#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "core/pif_comm.h"
#include "gps/pif_gps.h"


/**
 * @class StPifGpsNmea
 * @brief
 */
typedef struct StPifGpsNmea
{
	// Public Member Variable

	// Public Event Variable

	// Read-only Member Variable
    PifGps _gps;

	// Private Member Variable
	PifComm* __p_comm;

	// Private Event Variable
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

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
