// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmp280_spi.h
 * @brief Attaches a BMP280 on SPI: detection and initialization of a PifBmp280.
 */
#ifndef PIF_BMP280_SPI_H
#define PIF_BMP280_SPI_H


#include "sensor/pif_bmp280.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a BMP280 answers on the port, by reading its ID register through a temporary device.
 * @param p_spi SPI port the device is on.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280Spi_Detect(PifSpiPort* p_spi);

/**
 * @brief Adds the BMP280 to the SPI port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280Spi_Init(PifBmp280* p_owner, PifId id, PifSpiPort* p_spi, void *p_client);

/**
 * @brief Removes the task of the sensor, if any, and its device from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifBmp280Spi_Clear(PifBmp280* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_SPI_H
