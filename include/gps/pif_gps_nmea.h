#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "communication/pif_uart.h"
#include "communication/pif_i2c.h"
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
	PifI2cDevice* _p_i2c_device;

	// Private Member Variable
	PifUart* __p_uart;
	PifI2cPort* __p_i2c_port;

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
 * @fn pifGpsNmea_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifGpsNmea_AttachUart(PifGpsNmea* p_owner, PifUart* p_uart);

/**
 * @fn pifGpsNmea_DetachUart
 * @brief
 * @param p_owner
 */
void pifGpsNmea_DetachUart(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachI2c
 * @brief
 * @param p_owner
 * @param p_i2c
 * @param addr
 * @param p_client
 * @param period1ms
 * @param start
 * @param name
 * @return
 */
BOOL pifGpsNmea_AttachI2c(PifGpsNmea* p_owner, PifI2cPort* p_i2c, uint8_t addr, void *p_client, uint16_t period1ms, BOOL start, const char* name);

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
