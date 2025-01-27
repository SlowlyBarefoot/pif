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
 * @param max_transfer_size
 * @return
 */
BOOL pifDps310Spi_Detect(PifSpiPort* p_spi, uint16_t max_transfer_size);

/**
 * @fn pifDps310Spi_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_spi
 * @param max_transfer_size
 * @return
 */
BOOL pifDps310Spi_Init(PifDps310* p_owner, PifId id, PifSpiPort* p_spi, uint16_t max_transfer_size);

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
