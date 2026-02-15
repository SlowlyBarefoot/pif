#ifndef PIF_DPS310_I2C_H
#define PIF_DPS310_I2C_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDps310I2c_Detect
 * @brief Performs the dps310 i2c detect operation.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifDps310I2c_Init
 * @brief Initializes dps310 i2c init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param addr Device address on the bus.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310I2c_Init(PifDps310* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @fn pifDps310I2c_Clear
 * @brief Releases resources used by dps310 i2c clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifDps310I2c_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_I2C_H
