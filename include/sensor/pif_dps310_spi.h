#ifndef PIF_DPS310_SPI_H
#define PIF_DPS310_SPI_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDps310Spi_Detect
 * @brief Performs the dps310 spi detect operation.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifDps310Spi_Init
 * @brief Initializes dps310 spi init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310Spi_Init(PifDps310* p_owner, PifId id, PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifDps310Spi_Clear
 * @brief Releases resources used by dps310 spi clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifDps310Spi_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_SPI_H
