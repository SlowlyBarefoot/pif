#ifndef PIF_GY86_H
#define PIF_GY86_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"
#include "sensor/pif_hmc5883.h"
#include "sensor/pif_mpu60x0.h"
#include "sensor/pif_ms5611.h"


/**
 * @class StPifGy86Param
 * @brief Defines the st pif gy86 param data structure.
 */
typedef struct StPifGy86Param
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
	PifEvtBaroRead ms5611_evt_read;

	uint8_t disallow_yield_id;
} PifGy86Param;

/**
 * @class StPifGy86
 * @brief Defines the st pif gy86 data structure.
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
 * @brief Performs the gy86 detect operation.
 * @param p_i2c Pointer to i2c.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_Detect(PifI2cPort* p_i2c);

/**
 * @fn pifGy86_Init
 * @brief Initializes gy86 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_i2c Pointer to i2c.
 * @param p_param Pointer to param.
 * @param p_imu_sensor Pointer to imu sensor.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor);

/**
 * @fn pifGy86_Clear
 * @brief Releases resources used by gy86 clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifGy86_Clear(PifGy86* p_owner);

/**
 * @fn pifGy86_ReadMag
 * @brief Reads raw data from gy86 read mag.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Pointer to mag.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_ReadMag(PifGy86* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GY86_H
