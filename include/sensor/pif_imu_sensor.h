// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_IMU_SENSOR_H
#define PIF_IMU_SENSOR_H

#include "core/pif.h"


#define AXIS_X		0
#define AXIS_Y		1
#define AXIS_Z		2
#define AXIS_COUNT	3

#define IMU_MEASURE_GYROSCOPE			0x01
#define IMU_MEASURE_ACCELERO			0x02
#define IMU_MEASURE_MAGNETO				0x04


typedef enum EnPifImuSensorAlign
{
    IMUS_ALIGN_DEFAULT,			// Keep the orientation the driver set
    IMUS_ALIGN_CW0_DEG,
    IMUS_ALIGN_CW90_DEG,
    IMUS_ALIGN_CW180_DEG,
    IMUS_ALIGN_CW270_DEG,
    IMUS_ALIGN_CW0_DEG_FLIP,
    IMUS_ALIGN_CW90_DEG_FLIP,
    IMUS_ALIGN_CW180_DEG_FLIP,
    IMUS_ALIGN_CW270_DEG_FLIP
} PifImuSensorAlign;


typedef BOOL (*PifImuSensorRead)(void* p_owner, int16_t* p_data);


/**
 * @class StPifImuSensorInfo
 * @brief Defines the st pif imu sensor info data structure.
 */
typedef struct StPifImuSensorInfo
{
	PifImuSensorAlign align;
	PifImuSensorRead read;
	PifIssuerP p_issuer;
} PifImuSensorInfo;

/**
 * @class StPifImuSensor
 * @brief Defines the st pif imu sensor data structure.
 */
typedef struct StPifImuSensor
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t	_measure;			// IMU_MEASURE_XXX
	float _gyro_gain;			// LSB/degree/s
	float _accel_gain;			// LSB/g
	float _mag_gain;			// LSB/Gauss

	// Private Member Variable
	PifImuSensorInfo __gyro_info;
	PifImuSensorInfo __accel_info;
	PifImuSensorInfo __mag_info;
	BOOL __gyro_calibrated;
	uint8_t __deadband_multiple;		// Deadband width in multiples of the gyro noise
	float __gyro_bias[AXIS_COUNT];		// Mean raw gyro reading at rest (LSB)
	float __gyro_noise[AXIS_COUNT];		// Standard deviation of the raw gyro reading at rest (LSB)
	float __gyro_deadband[AXIS_COUNT];	// Bias-corrected raw readings below this read as zero (LSB)
	BOOL __board_alignment;
	float __board_matrix[3][3];		// Board frame to body frame, Rz(-yaw) * Ry(-pitch) * Rx(-roll)
} PifImuSensor;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifImuSensor_Init
 * @brief Initializes imu sensor init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 */
void pifImuSensor_Init(PifImuSensor* p_owner);

/**
 * @fn pifImuSensor_InitBoardAlignment
 * @brief Sets the rotation of the board relative to the airframe.
 *
 * The board is rotated by yaw about Z, then by pitch about the new Y, then by roll
 * about the new X (Z-Y-X order). Positive angles turn the board clockwise, seen from the
 * positive end of the axis (from above for yaw), so a yaw of 90 matches
 * IMUS_ALIGN_CW90_DEG, as Betaflight's board_align_* does. Readings
 * are first corrected for the sensor mounting (IMUS_ALIGN_XXX), then for this rotation.
 * All three angles at 0 turn board alignment off.
 * @param p_owner Pointer to the owner instance.
 * @param board_align_roll Roll angle of the board in degrees.
 * @param board_align_pitch Pitch angle of the board in degrees.
 * @param board_align_yaw Yaw angle of the board in degrees.
 */
void pifImuSensor_InitBoardAlignment(PifImuSensor* p_owner, int16_t board_align_roll, int16_t board_align_pitch, int16_t board_align_yaw);

/**
 * @fn pifImuSensor_ResetGyroCalibration
 * @brief Clears the gyro bias, noise and deadband.
 * @param p_owner Pointer to the owner instance.
 */
void pifImuSensor_ResetGyroCalibration(PifImuSensor* p_owner);

/**
 * @fn pifImuSensor_CalibrateGyro
 * @brief Measures the gyro bias and noise while the sensor is at rest.
 *
 * Blocks for samples * interval_ms. pifImuSensor_ReadGyro() subtracts the bias afterwards.
 * @param p_owner Pointer to the owner instance.
 * @param samples Number of samples to average. Must be at least 1.
 * @param interval_ms Delay between samples in milliseconds.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_CalibrateGyro(PifImuSensor* p_owner, uint16_t samples, uint16_t interval_ms);

/**
 * @fn pifImuSensor_SetGyroDeadband
 * @brief Sets a deadband around the gyro bias, as a multiple of the measured noise.
 *
 * Takes effect once the gyro is calibrated. 0 turns the deadband off.
 * @param p_owner Pointer to the owner instance.
 * @param multiple Deadband width in multiples of the noise standard deviation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_SetGyroDeadband(PifImuSensor* p_owner, uint8_t multiple);

/**
 * @fn pifImuSensor_SetGyroAlign
 * @brief Sets configuration values required by imu sensor set gyro align.
 * @param p_owner Pointer to the owner instance.
 * @param align Parameter align used by this operation.
 */
void pifImuSensor_SetGyroAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadRawGyro
 * @brief Reads raw data from imu sensor read raw gyro.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Pointer to gyro.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadRawGyro(PifImuSensor* p_owner, float* p_gyro);

/**
 * @fn pifImuSensor_ReadGyro
 * @brief Reads raw data from imu sensor read gyro.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Pointer to gyro.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadGyro(PifImuSensor* p_owner, float* p_gyro);

/**
 * @fn pifImuSensor_SetAccelAlign
 * @brief Sets configuration values required by imu sensor set accel align.
 * @param p_owner Pointer to the owner instance.
 * @param align Parameter align used by this operation.
 */
void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadRawAccel
 * @brief Reads raw data from imu sensor read raw accel.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Pointer to accel.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadRawAccel(PifImuSensor* p_owner, float* p_accel);

/**
 * @fn pifImuSensor_ReadAccel
 * @brief Reads raw data from imu sensor read accel.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Pointer to accel.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadAccel(PifImuSensor* p_owner, float* p_accel);

/**
 * @fn pifImuSensor_SetMagAlign
 * @brief Sets configuration values required by imu sensor set mag align.
 * @param p_owner Pointer to the owner instance.
 * @param align Parameter align used by this operation.
 */
void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadRawMag
 * @brief Reads raw data from imu sensor read raw mag.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Pointer to mag.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadRawMag(PifImuSensor* p_owner, float* p_mag);

/**
 * @fn pifImuSensor_ReadMag
 * @brief Reads raw data from imu sensor read mag.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Pointer to mag.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_ReadMag(PifImuSensor* p_owner, float* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_IMU_SENSOR_H
