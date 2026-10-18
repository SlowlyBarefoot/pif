// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmi270_i2c.h
 * @brief Attaches a BMI270 on I2C: detection and initialization of a PifBmi270.
 */
#ifndef PIF_BMI270_I2C_H
#define PIF_BMI270_I2C_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a BMI270 answers at addr, by reading its ID register through a temporary device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270I2c_Detect(PifI2cPort *p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the BMI270 at addr to the I2C port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270I2c_Init(PifBmi270 *p_owner, PifId id, PifI2cPort *p_i2c, uint8_t addr, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifBmi270I2c_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_I2C_H
