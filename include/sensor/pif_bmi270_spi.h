#ifndef PIF_BMI270_SPI_H
#define PIF_BMI270_SPI_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmi270Spi_Detect
 * @brief Performs the bmi270 spi detect operation.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270Spi_Detect(PifSpiPort *p_spi, void *p_client);

/**
 * @fn pifBmi270Spi_Init
 * @brief Initializes bmi270 spi init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_spi Pointer to spi.
 * @param p_client Pointer to optional client context data.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifBmi270Spi_Init(PifBmi270 *p_owner, PifId id, PifSpiPort *p_spi, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @fn pifBmi270Spi_Clear
 * @brief Releases resources used by bmi270 spi clear.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
void pifBmi270Spi_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_SPI_H
