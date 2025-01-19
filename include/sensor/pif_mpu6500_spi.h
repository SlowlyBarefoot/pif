#ifndef PIF_MPU6500_SPI_H
#define PIF_MPU6500_SPI_H


#include "sensor/pif_mpu6500.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMpu6500Spi_Detect
 * @brief
 * @param p_spi
 * @return
 */
BOOL pifMpu6500Spi_Detect(PifSpiPort* p_spi);

/**
 * @fn pifMpu6500Spi_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_spi
 * @param p_imu_sensor
 * @return
 */
BOOL pifMpu6500Spi_Init(PifMpu6500* p_owner, PifId id, PifSpiPort* p_spi, PifImuSensor* p_imu_sensor);

/**
 * @fn pifMpu6500Spi_Clear
 * @brief
 * @param p_owner
 * @return
 */
void pifMpu6500Spi_Clear(PifMpu6500* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MPU6500_SPI_H
