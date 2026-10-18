// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_mpu60x0_i2c.h
 * @brief Attaches an MPU-6000 or MPU-6050 on I2C: detection and initialization of a PifMpu60x0.
 */
#ifndef PIF_MPU60X0_I2C_H
#define PIF_MPU60X0_I2C_H


#include "sensor/pif_mpu60x0.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a MPU-6000/6050 answers at addr, by reading its ID register through a temporary device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the MPU-6000/6050 at addr to the I2C port and sets it up. The chip is reset first.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0I2c_Init(PifMpu60x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifMpu60x0I2c_Clear(PifMpu60x0* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU60X0_I2C_H
