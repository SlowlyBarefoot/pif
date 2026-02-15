#ifndef PIF_MPU6500_I2C_H
#define PIF_MPU6500_I2C_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500I2c_Detect
 * @brief Performs the mpu6500 i2c detect operation.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifMpu6500I2c_Init
 * @brief Initializes mpu6500 i2c init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMpu6500I2c_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500I2c_Clear
 * @brief Releases resources used by mpu6500 i2c clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifMpu6500I2c_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_I2C_H
