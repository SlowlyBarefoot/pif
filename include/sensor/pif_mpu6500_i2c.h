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
 * @param max_transfer_size
 * @return
 */
BOOL pifMpu6500I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, uint16_t max_transfer_size);

/**
 * @fn pifMpu6500I2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param max_transfer_size
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu6500I2c_Init(PifMpu6500* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, uint16_t max_transfer_size, PifImuSensor* p_imu_sensor);

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
