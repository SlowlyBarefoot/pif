#ifndef PIF_BMP280_I2C_H
#define PIF_BMP280_I2C_H


#include "sensor/pif_bmp280.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmp280I2c_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifBmp280I2c_Detect(PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifBmp280I2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @param p_client
 * @return
 */
BOOL pifBmp280I2c_Init(PifBmp280* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifBmp280I2c_Clear
 * @brief
 * @param p_owner
 */
void pifBmp280I2c_Clear(PifBmp280* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_I2C_H
