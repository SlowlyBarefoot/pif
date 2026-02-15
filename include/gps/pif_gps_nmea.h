#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "communication/pif_uart.h"
#include "communication/pif_i2c.h"
#include "core/pif_task_manager.h"
#include "gps/pif_gps.h"


/**
 * @class StPifGpsNmea
 * @brief NMEA transport wrapper that connects `PifGps` to UART or I2C input.
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
 * @brief Initializes an NMEA GPS wrapper instance.
 * @param p_owner Pointer to the wrapper instance to initialize.
 * @param usPifId Requested object ID. Use `PIF_ID_AUTO` for automatic allocation.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId usPifId);

/**
 * @fn pifGpsNmea_Clear
 * @brief Clears resources owned by the NMEA wrapper.
 * @param p_owner Pointer to the wrapper instance.
 */
void pifGpsNmea_Clear(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachUart
 * @brief Attaches a UART interface and starts parsing incoming NMEA bytes.
 * @param p_owner Pointer to the wrapper instance.
 * @param p_uart UART device used as the input stream.
 */
void pifGpsNmea_AttachUart(PifGpsNmea* p_owner, PifUart* p_uart);

/**
 * @fn pifGpsNmea_DetachUart
 * @brief Detaches the current UART client from this NMEA wrapper.
 * @param p_owner Pointer to the wrapper instance.
 */
void pifGpsNmea_DetachUart(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachI2c
 * @brief Attaches an I2C GNSS device and creates a periodic polling task.
 * @param p_owner Pointer to the wrapper instance.
 * @param p_i2c I2C port that provides access to the GNSS device.
 * @param addr I2C slave address of the GNSS device.
 * @param p_client Client context forwarded to the I2C device registration.
 * @param period1ms Polling period in milliseconds.
 * @param start If `TRUE`, the polling task starts immediately after creation.
 * @param name Optional task name. If `NULL`, a default name is assigned.
 * @return `TRUE` if device and task attachment succeeds, otherwise `FALSE`.
 */
BOOL pifGpsNmea_AttachI2c(PifGpsNmea* p_owner, PifI2cPort* p_i2c, uint8_t addr, void *p_client, uint16_t period1ms, BOOL start, const char* name);

/**
 * @fn pifGpsNmea_DetachI2c
 * @brief Detaches and removes the currently attached I2C GNSS device.
 * @param p_owner Pointer to the wrapper instance.
 */
void pifGpsNmea_DetachI2c(PifGpsNmea* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
