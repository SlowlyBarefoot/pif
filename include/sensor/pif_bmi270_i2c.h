#ifndef PIF_BMI270_I2C_H
#define PIF_BMI270_I2C_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmi270I2c_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @param p_client
 * @return
 */
BOOL pifBmi270I2c_Detect(PifI2cPort *p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifBmi270I2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_client
 * @param p_imu_sensor
 * @return
 */
BOOL pifBmi270I2c_Init(PifBmi270 *p_owner, PifId id, PifI2cPort *p_i2c, uint8_t addr, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @fn pifBmi270I2c_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifBmi270I2c_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_I2C_H
