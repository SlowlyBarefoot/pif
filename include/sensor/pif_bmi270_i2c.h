#ifndef PIF_BMI270_I2C_H
#define PIF_BMI270_I2C_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmi270I2c_Detect
 * @brief Performs the bmi270 i2c detect operation.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270I2c_Detect(PifI2cPort *p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifBmi270I2c_Init
 * @brief Initializes bmi270 i2c init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270I2c_Init(PifBmi270 *p_owner, PifId id, PifI2cPort *p_i2c, uint8_t addr, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @fn pifBmi270I2c_Clear
 * @brief Releases resources used by bmi270 i2c clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifBmi270I2c_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_I2C_H
