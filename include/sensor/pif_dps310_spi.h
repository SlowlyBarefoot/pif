#ifndef PIF_DPS310_SPI_H
#define PIF_DPS310_SPI_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDps310Spi_Detect
 * @brief
 * @param p_spi
 * @param p_client
 * @return
 */
BOOL pifDps310Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifDps310Spi_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_spi
 * @param p_client
 * @return
 */
BOOL pifDps310Spi_Init(PifDps310* p_owner, PifId id, PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifDps310Spi_Clear
 * @brief
 * @param p_owner
 */
void pifDps310Spi_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_SPI_H
