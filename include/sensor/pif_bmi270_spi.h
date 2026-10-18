// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bmi270_spi.h
 * @brief Attaches a BMI270 on SPI: detection and initialization of a PifBmi270.
 */
#ifndef PIF_BMI270_SPI_H
#define PIF_BMI270_SPI_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a BMI270 answers on the port, by reading its ID register through a temporary device.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270Spi_Detect(PifSpiPort *p_spi, void *p_client);

/**
 * @brief Adds the BMI270 to the SPI port and sets it up.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_spi SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270Spi_Init(PifBmi270 *p_owner, PifId id, PifSpiPort *p_spi, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifBmi270Spi_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_SPI_H
