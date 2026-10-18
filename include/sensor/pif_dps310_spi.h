// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_dps310_spi.h
 * @brief Attaches a DPS310 on SPI: detection and initialization of a PifDps310.
 */
#ifndef PIF_DPS310_SPI_H
#define PIF_DPS310_SPI_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a DPS310 answers on the port, by reading its ID register through a temporary device.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @brief Adds the DPS310 to the SPI port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310Spi_Init(PifDps310* p_owner, PifId id, PifSpiPort* p_spi, void *p_client);

/**
 * @brief Removes the task of the sensor, if any, and its device from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifDps310Spi_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_SPI_H
