// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmp280_i2c.h
 * @brief Attaches a BMP280 on I2C: detection and initialization of a PifBmp280.
 */
#ifndef PIF_BMP280_I2C_H
#define PIF_BMP280_I2C_H


#include "sensor/pif_bmp280.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a BMP280 answers at addr, by reading its ID register through a temporary device.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280I2c_Detect(PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Adds the BMP280 at addr to the I2C port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param addr Device address on the bus.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmp280I2c_Init(PifBmp280* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client);

/**
 * @brief Removes the task of the sensor, if any, and its device from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifBmp280I2c_Clear(PifBmp280* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMP280_I2C_H
