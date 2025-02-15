#ifndef PIF_BMI270_SPI_H
#define PIF_BMI270_SPI_H


#include "sensor/pif_bmi270.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBmi270Spi_Detect
 * @brief
 * @param p_spi
 * @param p_client
 * @return
 */
BOOL pifBmi270Spi_Detect(PifSpiPort *p_spi, void *p_client);

/**
 * @fn pifBmi270Spi_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_spi
 * @param p_client
 * @param p_imu_sensor
 * @return
 */
BOOL pifBmi270Spi_Init(PifBmi270 *p_owner, PifId id, PifSpiPort *p_spi, void *p_client, PifImuSensor *p_imu_sensor);

/**
 * @fn pifBmi270Spi_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifBmi270Spi_Clear(PifBmi270 *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BMI270_SPI_H
