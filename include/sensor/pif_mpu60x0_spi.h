// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MPU60X0_SPI_H
#define PIF_MPU60X0_SPI_H


#include "sensor/pif_mpu60x0.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu60x0Spi_Detect
 * @brief Performs the mpu60x0 spi detect operation. Only the MPU-6000 has a SPI interface, so
 *        besides WHO_AM_I the product ID has to be one of its known revisions.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifMpu60x0Spi_Init
 * @brief Initializes mpu60x0 spi init and prepares it for use. The chip is reset first, with
 *        DEVICE_RESET and then SIGNAL_PATH_RESET as the datasheet asks for on SPI, which holds the
 *        CPU for 200ms.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0Spi_Init(PifMpu60x0* p_owner, PifId id, PifSpiPort* p_spi, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu60x0Spi_Clear
 * @brief Releases resources used by mpu60x0 spi clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifMpu60x0Spi_Clear(PifMpu60x0* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU60X0_SPI_H
