// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pif_gy86.h
 * @brief GY-86 module: MPU-6050, HMC5883L and MS5611 on one I2C bus.
 *
 * Initializes the three sensors from one PifGy86Param. The HMC5883L is on the auxiliary I2C bus of the MPU-6050:
 * it is set up through the bypass, then read by the I2C master of the MPU-6050.
 */
#ifndef PIF_GY86_H
#define PIF_GY86_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"
#include "sensor/pif_hmc5883.h"
#include "sensor/pif_mpu60x0_i2c.h"
#include "sensor/pif_ms5611.h"


/**
 * @class StPifGy86Param
 * @brief Settings of the three sensors of a GY-86.
 */
typedef struct StPifGy86Param
{
	PifMpu60x0AfsSel mpu60x0_afs_sel;	///< Accelerometer range of the MPU-6050
	PifMpu60x0Clksel mpu60x0_clksel;	///< Clock source of the MPU-6050
	PifMpu60x0DlpfCfg mpu60x0_dlpf_cfg;	///< Low pass filter of the MPU-6050
	PifMpu60x0FsSel mpu60x0_fs_sel;	///< Gyro range of the MPU-6050
	PifMpu60x0I2cMstClk mpu60x0_i2c_mst_clk;	///< Clock of the auxiliary I2C master of the MPU-6050

	PifHmc5883DataRate hmc5883_data_rate;	///< Data rate of the HMC5883L
	PifHmc5883Gain hmc5883_gain;	///< Gain of the HMC5883L
	PifHmc5883Mode hmc5883_mode;	///< Measurement mode of the HMC5883L
	PifHmc5883Samples hmc5883_samples;	///< Samples averaged by the HMC5883L

	PifMs5611Osr ms5611_osr;	///< Oversampling of the MS5611
	uint16_t ms5611_read_period;	///< Period in ms of the MS5611 readings
	PifEvtBaroRead ms5611_evt_read;	///< Gets the readings of the MS5611. NULL leaves the MS5611 unused.
} PifGy86Param;

/**
 * @class StPifGy86
 * @brief GY-86 module.
 */
typedef struct StPifGy86
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifMpu60x0 _mpu6050;	///< MPU-6050 gyroscope and accelerometer
	PifHmc5883 _hmc5883;	///< HMC5883L magnetometer
	PifMs5611 _ms5611;	///< MS5611 barometer

	// Private Member Variable
	uint8_t __mag_start_reg;
} PifGy86;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether a GY-86 answers on the port, by detecting its MPU-6050 at the default address.
 * @param p_i2c I2C port the device is on.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_Detect(PifI2cPort* p_i2c);

/**
 * @brief Sets up the three sensors of a GY-86 and registers them with an IMU sensor.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param p_param Settings of the sensors, or NULL for their defaults. Its ms5611_evt_read decides whether the
 *                MS5611 is used.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_Init(PifGy86* p_owner, PifId id, PifI2cPort* p_i2c, PifGy86Param* p_param, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the three sensors from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifGy86_Clear(PifGy86* p_owner);

/**
 * @brief Reads the magnetometer of the HMC5883L through the auxiliary I2C master of the MPU-6050.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the raw values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifGy86_ReadMag(PifGy86* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GY86_H
