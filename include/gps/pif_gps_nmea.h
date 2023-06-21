#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "core/pif_comm.h"
#include "core/pif_i2c.h"
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
    PifTask* _p_task;

	// Private Member Variable
	PifComm* __p_comm;
	PifI2cPort* __p_i2c_port;
	PifI2cDevice* __p_i2c_device;

	// Private Event Variable
    uint16_t __length;
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
 * @fn pifGpsNmea_AttachI2c
 * @brief
 * @param p_owner
 * @param p_i2c
 * @param addr
 * @param period
 * @param start
 * @param name
 * @return
 */
BOOL pifGpsNmea_AttachI2c(PifGpsNmea* p_owner, PifI2cPort* p_i2c, uint8_t addr, uint16_t period, BOOL start, const char* name);

/**
 * @fn pifGpsNmea_DetachI2c
 * @brief
 * @param p_owner
 */
void pifGpsNmea_DetachI2c(PifGpsNmea* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
