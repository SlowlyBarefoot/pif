// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_MPU60X0_I2C_H
#define PIF_MPU60X0_I2C_H


#include "sensor/pif_mpu60x0.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu60x0I2c_Detect
 * @brief Performs the mpu60x0 i2c detect operation.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifMpu60x0I2c_Init
 * @brief Initializes mpu60x0 i2c init and prepares it for use. The chip is reset first.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu60x0I2c_Init(PifMpu60x0* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu60x0I2c_Clear
 * @brief Releases resources used by mpu60x0 i2c clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifMpu60x0I2c_Clear(PifMpu60x0* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU60X0_I2C_H
