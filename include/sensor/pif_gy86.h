#ifndef PIF_GY86_H
#define PIF_GY86_H


#include "core/pif_i2c.h"
#include "core/pif_task.h"
#include "sensor/pif_imu_sensor.h"
#include "sensor/pif_hmc5883.h"
#include "sensor/pif_mpu60x0.h"
#include "sensor/pif_ms5611.h"


/**
 * @class StPifGy86Config
 * @brief
 */
typedef struct StPifGy86Config
{
	PifMpu60x0AfsSel mpu60x0_afs_sel;
	PifMpu60x0Clksel mpu60x0_clksel;
	PifMpu60x0DlpfCfg mpu60x0_dlpf_cfg;
	PifMpu60x0FsSel mpu60x0_fs_sel;
	PifMpu60x0I2cMstClk mpu60x0_i2c_mst_clk;

	PifHmc5883DataRate hmc5883_data_rate;
	PifHmc5883Gain hmc5883_gain;
	PifHmc5883Mode hmc5883_mode;
	PifHmc5883Samples hmc5883_samples;

	PifMs5611Osr ms5611_osr;
	uint16_t ms5611_read_period;
	PifEvtMs5611Read ms5611_evt_read;
} PifGy86Config;

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
 * @fn pifGy86_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_i2c
 * @param p_imu_sensor
 * @param p_config
 * @return
 */
BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifImuSensor* p_imu_sensor, PifGy86Config* p_config);

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
