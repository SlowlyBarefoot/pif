// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PIF_GPS_NMEA_H
#define PIF_GPS_NMEA_H


#include "communication/pif_transport.h"
#include "gps/pif_gps.h"


/**
 * @class StPifGpsNmea
 * @brief NMEA client of a PifTransport, such as a PifUart or a PifDeviceStream, that feeds `PifGps`.
 */
typedef struct StPifGpsNmea
{
	// Read-only Member Variable
    PifGps _gps;

	// Private Member Variable
	PifTransport *__p_transport;
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
 * @brief Detaches the transport and clears resources owned by the NMEA wrapper.
 * @param p_owner Pointer to the wrapper instance.
 */
void pifGpsNmea_Clear(PifGpsNmea* p_owner);

/**
 * @fn pifGpsNmea_AttachTransport
 * @brief Attaches the wrapper to the transport the receiver sends through, as its only client, and starts
 *        parsing the NMEA bytes that arrive.
 * @param p_owner Pointer to the wrapper instance.
 * @param p_transport Transport the receiver is on, for example &uart.parent or &device_stream.parent.
 * @return TRUE on success, otherwise FALSE with pif_error set to E_INVALID_PARAM.
 */
BOOL pifGpsNmea_AttachTransport(PifGpsNmea *p_owner, PifTransport *p_transport);

/**
 * @fn pifGpsNmea_DetachTransport
 * @brief Detaches the wrapper from its transport. Nothing happens when none is attached.
 * @param p_owner Pointer to the wrapper instance.
 */
void pifGpsNmea_DetachTransport(PifGpsNmea *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GPS_NMEA_H
