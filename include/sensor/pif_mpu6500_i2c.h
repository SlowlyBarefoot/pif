// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu6500_i2c.h
 * @brief Attaches an MPU-6500 on I2C: detection and initialization of a PifMpu6500.
 */
#ifndef PIF_MPU6500_I2C_H
#define PIF_MPU6500_I2C_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a MPU-6500 answers at addr, by reading its ID register through a temporary device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the MPU-6500 at addr to the I2C port and sets it up. The chip is reset first, which
 *        holds the CPU for 100ms.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500I2c_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifMpu6500I2c_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_I2C_H
