#ifndef PIF_MPU6500_I2C_H
#define PIF_MPU6500_I2C_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500I2c_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifMpu6500I2c_Detect(PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifMpu6500I2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_client
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu6500I2c_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500I2c_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifMpu6500I2c_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_I2C_H
