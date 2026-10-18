// SPDX-License-Identifier: GPL-3.0-or-later
// Portions derived from Baseflight/Cleanflight compass_hmc5883l.c (MultiWii lineage), GPL-3.0-or-later.
// See THIRD_PARTY_NOTICES.md.
/**
 * @file pif_hmc5883.h
 * @brief Honeywell HMC5883L 3-axis magnetometer on I2C, as the magnetometer of a PifImuSensor.
 *
 * The register map below follows the datasheet.
 */
#ifndef PIF_HMC5883_H
#define PIF_HMC5883_H


#include "communication/pif_i2c.h"
#include "sensor/pif_imu_sensor.h"


#define HMC5883_I2C_ADDR			0x1E	///< I2C address


/**
 * @brief Register addresses.
 */
typedef enum EnPifHmc5883Reg
{
	HMC5883_REG_CONFIG_A			= 0x00,
	HMC5883_REG_CONFIG_B			= 0x01,
	HMC5883_REG_MODE				= 0x02,
	HMC5883_REG_OUT_X_M				= 0x03,
	HMC5883_REG_OUT_X_L				= 0x04,
	HMC5883_REG_OUT_Z_M				= 0x05,
	HMC5883_REG_OUT_Z_L				= 0x06,
	HMC5883_REG_OUT_Y_M				= 0x07,
	HMC5883_REG_OUT_Y_L				= 0x08,
	HMC5883_REG_STATUS				= 0x09,
	HMC5883_REG_IDENT_A				= 0x0A,
	HMC5883_REG_IDENT_B				= 0x0B,
	HMC5883_REG_IDENT_C				= 0x0C,
} PifHmc5883Reg;


// Register : CONFIG_A

/** @name HMC5883 CONFIG_A register
 *  @{
 */
/**
 * @brief Bits 1-0 (MS): measurement configuration, normal or with a bias for the self test.
 */
typedef enum EnPifHmc5883MeasureMode
{
    HMC5883_MEASURE_MODE_NORMAL		= 0,		///< Default
    HMC5883_MEASURE_MODE_POS_BIAS	= 1,
    HMC5883_MEASURE_MODE_NEG_BIAS	= 2,

    HMC5883_MEASURE_MODE_DEFAULT	= 0
} PifHmc5883MeasureMode;

/**
 * @brief Bits 4-2 (DO): output data rate in continuous mode.
 */
typedef enum EnPifHmc5883DataRate
{
    HMC5883_DATARATE_0_75_HZ		= 0 << 2,
    HMC5883_DATARATE_1_5HZ			= 1 << 2,
    HMC5883_DATARATE_3HZ			= 2 << 2,
    HMC5883_DATARATE_7_5HZ			= 3 << 2,
    HMC5883_DATARATE_15HZ			= 4 << 2,	///< Default
    HMC5883_DATARATE_30HZ			= 5 << 2,
    HMC5883_DATARATE_75HZ			= 6 << 2,

	HMC5883_DATARATE_DEFAULT		= 4 << 2
} PifHmc5883DataRate;

/**
 * @brief Bits 6-5 (MA): samples averaged per output.
 */
typedef enum EnPifHmc5883Samples
{
    HMC5883_SAMPLES_1				= 0 << 5,	///< Default
    HMC5883_SAMPLES_2				= 1 << 5,
    HMC5883_SAMPLES_4				= 2 << 5,
    HMC5883_SAMPLES_8				= 3 << 5,

	HMC5883_SAMPLES_DEFAULT			= 0 << 5
} PifHmc5883Samples;

#define HMC5883_MEASURE_MODE_MASK	0x03	///< Mask of the MEASURE_MODE field
#define HMC5883_DATA_RATE_MASK		0x1C	///< Mask of the DATA_RATE field
#define HMC5883_SAMPLES_MASK		0x60	///< Mask of the SAMPLES field
/** @} */


// Register : CONFIG_B

/** @name HMC5883 CONFIG_B register
 *  @{
 */
/**
 * @brief Bits 7-5 (GN): gain, as the full scale range in Gauss.
 */
typedef enum EnPifHmc5883Gain
{
    HMC5883_GAIN_0_88GA		= 0 << 5,
    HMC5883_GAIN_1_3GA		= 1 << 5,	///< Default
    HMC5883_GAIN_1_9GA		= 2 << 5,
	HMC5883_GAIN_2_5GA		= 3 << 5,
	HMC5883_GAIN_4GA		= 4 << 5,
	HMC5883_GAIN_4_7GA		= 5 << 5,
	HMC5883_GAIN_5_6GA		= 6 << 5,
	HMC5883_GAIN_8_1GA		= 7 << 5,

	HMC5883_GAIN_DEFAULT	= 1 << 5
} PifHmc5883Gain;

#define HMC5883_GAIN_MASK	0xE0		///< Use pifHmc5883_SetGain to change this value.
/** @} */


// Register : MODE

/** @name HMC5883 MODE register
 *  @{
 */
/**
 * @brief Bits 1-0 (MD): operating mode.
 */
typedef enum EnPifHmc5883Mode
{
    HMC5883_MODE_CONTINOUS			= 0,
    HMC5883_MODE_SINGLE				= 1,	///< Default
    HMC5883_MODE_IDLE				= 2,

    HMC5883_MODE_DEFAULT			= 1
} PifHmc5883Mode;

#define HMC5883_HIGH_SPEED(N)		((N) << 7)	///< Puts N into the HIGH_SPEED field

#define HMC5883_MODE_MASK			0x03	///< Mask of the MODE field
#define HMC5883_HIGH_SPEED_MASK		0x80	///< Mask of the HIGH_SPEED field
/** @} */


/**
 * @class StPifHmc5883
 * @brief HMC5883L magnetometer.
 */
typedef struct StPifHmc5883
{
	// Public Member Variable
	float scale[3];	///< Multiplier of each axis, from the self test. 1 by default.

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;	///< I2C device of the sensor

	// Private Member Variable
	PifImuSensor* __p_imu_sensor;
} PifHmc5883;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tells whether an HMC5883L answers on the port, by reading its identification registers through a
 *        temporary device.
 * @param p_i2c I2C port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHmc5883_Detect(PifI2cPort* p_i2c, void *p_client);

/**
 * @brief Adds the HMC5883L to the I2C port, calibrates its axes with the self test bias and registers it as the
 *        magnetometer of an IMU sensor. It holds the CPU for some hundreds of ms.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_i2c I2C port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @param p_imu_sensor IMU sensor the driver registers its sensors with.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHmc5883_Init(PifHmc5883* p_owner, PifId id, PifI2cPort* p_i2c, void *p_client, PifImuSensor* p_imu_sensor);

/**
 * @brief Removes the device of the sensor from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifHmc5883_Clear(PifHmc5883* p_owner);

/**
 * @brief Sets the gain of the HMC5883L and updates the magnetometer gain of the IMU sensor to it.
 * @param p_owner Pointer to the owner instance.
 * @param gain Gain configuration value.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHmc5883_SetGain(PifHmc5883* p_owner, PifHmc5883Gain gain);

/**
 * @brief Reads the raw values of the 3 axes, scaled by the calibration of the axes.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHmc5883_ReadMag(PifHmc5883* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_HMC5883_H
