// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_dps310_i2c.h
 * @brief Attaches a DPS310 on I2C: detection and initialization of a PifDps310.
 */
#ifndef PIF_DPS310_I2C_H
#define PIF_DPS310_I2C_H


#include "sensor/pif_dps310.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a DPS310 answers at addr, by reading its ID register through a temporary device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the DPS310 at addr to the I2C port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifDps310I2c_Init(PifDps310* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Removes the task of the sensor, if any, and its device from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifDps310I2c_Clear(PifDps310* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DPS310_I2C_H
