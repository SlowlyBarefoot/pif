#ifndef PIF_DPS310_I2C_H
#define PIF_DPS310_I2C_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDps310I2c_Detect
 * @brief
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifDps310I2c_Detect(PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifDps310I2c_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param addr
 * @return
 */
BOOL pifDps310I2c_Init(PifDps310* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr);

/**
 * @fn pifDps310I2c_Clear
 * @brief
 * @param p_owner
 */
void pifDps310I2c_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_I2C_H
