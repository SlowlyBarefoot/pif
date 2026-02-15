#ifndef PIF_BMP280_SPI_H
#define PIF_BMP280_SPI_H


#include "sensor/pif_bmp280.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmp280Spi_Detect
 * @brief Performs the bmp280 spi detect operation.
 * @param p_spi Pointer to spi.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280Spi_Detect(PifSpiPort* p_spi);

/**
 * @fn pifBmp280Spi_Init
 * @brief Initializes bmp280 spi init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280Spi_Init(PifBmp280* p_owner, PifId id, PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifBmp280Spi_Clear
 * @brief Releases resources used by bmp280 spi clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifBmp280Spi_Clear(PifBmp280* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_SPI_H
