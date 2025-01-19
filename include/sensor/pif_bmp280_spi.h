#ifndef PIF_BMP280_SPI_H
#define PIF_BMP280_SPI_H


#include "sensor/pif_bmp280.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmp280Spi_Detect
 * @brief
 * @param p_spi
 * @return
 */
BOOL pifBmp280Spi_Detect(PifSpiPort* p_spi);

/**
 * @fn pifBmp280Spi_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_spi
 * @return
 */
BOOL pifBmp280Spi_Init(PifBmp280* p_owner, PifId id, PifSpiPort* p_spi);

/**
 * @fn pifBmp280Spi_Clear
 * @brief
 * @param p_owner
 */
void pifBmp280Spi_Clear(PifBmp280* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_SPI_H
