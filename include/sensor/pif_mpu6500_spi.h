// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu6500_spi.h
 * @brief Attaches an MPU-6500 on SPI: detection and initialization of a PifMpu6500.
 */
#ifndef PIF_MPU6500_SPI_H
#define PIF_MPU6500_SPI_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a MPU-6500 answers on the port, by reading its ID register through a temporary device.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500Spi_Detect(PifSpiPort* p_spi, void *p_client);

/**
 * @brief Adds the MPU-6500 to the SPI port and sets it up. The chip is reset first, with
 *        DEVICE_RESET and then SIGNAL_PATH_RESET as the datasheet asks for on SPI, which holds the
 *        CPU for 200ms, and the I2C interface is disabled (I2C_IF_DIS). The port's act_read must
 *        set bit 7 of the register address, which the chip takes as the read flag.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500Spi_Init(PifMpu6500* p_owner, PifId id, PifSpiPort* p_spi, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifMpu6500Spi_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_SPI_H
