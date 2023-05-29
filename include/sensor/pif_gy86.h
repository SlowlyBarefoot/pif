#ifndef PIF_GY86_H
#define PIF_GY86_H


#include "core/pif_i2c.h"
#include "core/pif_task.h"
#include "sensor/pif_imu_sensor.h"
#include "sensor/pif_hmc5883.h"
#include "sensor/pif_mpu60x0.h"
#include "sensor/pif_ms5611.h"


/**
 * @class StPifGy86Param
 * @brief
 */
typedef struct StPifGy86Param
{
	PifMpu60x0Param mpu60x0;
	PifMpu60x0I2cMstClk mpu60x0_i2c_mst_clk;

	PifHmc5883Param hmc5883;

	PifMs5611Param ms5611;
} PifGy86Param;

/**
 * @class StPifGy86
 * @brief
 */
typedef struct StPifGy86
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
	PifMpu60x0 _mpu6050;
	PifHmc5883 _hmc5883;
	PifMs5611 _ms5611;

	// Private Member Variable
	uint8_t __mag_start_reg;
} PifGy86;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifGy86_Detect
 * @brief
 * @param p_i2c
 * @return
 */
BOOL pifGy86_Detect(PifI2cPort* p_i2c);

/**
 * @fn pifGy86_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param p_config
 * @param p_imu_sensor
 * @return
 */
BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor);

/**
 * @fn pifGy86_Clear
 * @brief
 * @param p_owner
 */
void pifGy86_Clear(PifGy86* p_owner);

/**
 * @fn pifGy86_ReadMag
 * @brief
 * @param p_owner
 * @param p_mag
 * @return
 */
BOOL pifGy86_ReadMag(PifGy86* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GY86_H
