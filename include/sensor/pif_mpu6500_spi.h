// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MPU6500_SPI_H
#define PIF_MPU6500_SPI_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500Spi_Detect
 * @brief Performs the mpu6500 spi detect operation.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @fn pifMpu6500Spi_Init
 * @brief Initializes mpu6500 spi init and prepares it for use. The chip is reset first, with
 *        DEVICE_RESET and then SIGNAL_PATH_RESET as the datasheet asks for on SPI, which holds the
 *        CPU for 200ms, and the I2C interface is disabled (I2C_IF_DIS). The port's act_read must
 *        set bit 7 of the register address, which the chip takes as the read flag.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500Spi_Init(PifMpu6500* p_owner, PifId id, PifSpiPort* p_spi, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500Spi_Clear
 * @brief Releases resources used by mpu6500 spi clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifMpu6500Spi_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_SPI_H
