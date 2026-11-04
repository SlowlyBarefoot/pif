// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_imu_sensor.h
 * @brief Common front of the IMU drivers: gyro, accelerometer and magnetometer in physical units and one frame.
 *
 * A driver such as pif_mpu6500 or pif_bmi270 registers its read functions and gains here. The application then
 * reads every sensor the same way, with the axes aligned to the board, the board aligned to the body and the
 * gyro calibrated at rest.
 */
#ifndef PIF_IMU_SENSOR_H
#define PIF_IMU_SENSOR_H

#include "core/pif.h"


#define AXIS_X		0	///< Index of the X axis
#define AXIS_Y		1	///< Index of the Y axis
#define AXIS_Z		2	///< Index of the Z axis
#define AXIS_COUNT	3	///< Number of axes

#define IMU_MEASURE_GYROSCOPE			0x01	///< A gyroscope is attached
#define IMU_MEASURE_ACCELERO			0x02	///< An accelerometer is attached
#define IMU_MEASURE_MAGNETO				0x04	///< A magnetometer is attached


/**
 * @brief Mounting of a sensor on the board, as a rotation about Z and an optional flip.
 */
typedef enum EnPifImuSensorAlign
{
    IMUS_ALIGN_DEFAULT,			///< Keep the orientation the driver set
    IMUS_ALIGN_CW0_DEG,			///< Not rotated
    IMUS_ALIGN_CW90_DEG,		///< Rotated 90 degrees clockwise
    IMUS_ALIGN_CW180_DEG,		///< Rotated 180 degrees
    IMUS_ALIGN_CW270_DEG,		///< Rotated 270 degrees clockwise
    IMUS_ALIGN_CW0_DEG_FLIP,	///< Flipped, not rotated
    IMUS_ALIGN_CW90_DEG_FLIP,	///< Flipped and rotated 90 degrees clockwise
    IMUS_ALIGN_CW180_DEG_FLIP,	///< Flipped and rotated 180 degrees
    IMUS_ALIGN_CW270_DEG_FLIP	///< Flipped and rotated 270 degrees clockwise
} PifImuSensorAlign;


/**
 * @brief Reads the raw values of the 3 axes of a sensor into p_data. Returns FALSE on failure.
 */
typedef BOOL (*PifImuSensorRead)(void* p_owner, int16_t* p_data);


/**
 * @class StPifImuSensorInfo
 * @brief One sensor attached by a driver.
 */
typedef struct StPifImuSensorInfo
{
	PifImuSensorAlign align;	///< Mounting of the sensor
	PifImuSensorRead read;	///< Reads its raw values
	PifIssuerP p_issuer;	///< Driver object given to read
} PifImuSensorInfo;

/**
 * @class StPifImuSensor
 * @brief Gyroscope, accelerometer and magnetometer of an IMU.
 */
typedef struct StPifImuSensor
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t	_measure;			///< IMU_MEASURE_XXX
	float _gyro_gain;			///< LSB/degree/s
	float _accel_gain;			///< LSB/g
	float _mag_gain;			///< LSB/Gauss

	// Private Member Variable
	PifImuSensorInfo __gyro_info;
	PifImuSensorInfo __accel_info;
	PifImuSensorInfo __mag_info;
	BOOL __gyro_calibrated;
	uint8_t __deadband_multiple;		///< Deadband width in multiples of the gyro noise
	float __gyro_bias[AXIS_COUNT];		///< Mean raw gyro reading at rest (LSB)
	float __gyro_noise[AXIS_COUNT];		///< Standard deviation of the raw gyro reading at rest (LSB)
	float __gyro_deadband[AXIS_COUNT];	///< Bias-corrected raw readings below this read as zero (LSB)
	BOOL __board_alignment;
	float __board_matrix[3][3];		///< Board frame to body frame, Rz(-yaw) * Ry(-pitch) * Rx(-roll)
} PifImuSensor;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clears an IMU sensor before the drivers register their sensors with it.
 * @param p_owner Pointer to the owner instance.
 */
void pifImuSensor_Init(PifImuSensor* p_owner);

/**
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
 * @brief Clears the gyro bias, noise and deadband.
 * @param p_owner Pointer to the owner instance.
 */
void pifImuSensor_ResetGyroCalibration(PifImuSensor* p_owner);

/**
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
 * @brief Sets a deadband around the gyro bias, as a multiple of the measured noise.
 *
 * Takes effect once the gyro is calibrated. 0 turns the deadband off.
 * @param p_owner Pointer to the owner instance.
 * @param multiple Deadband width in multiples of the noise standard deviation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifImuSensor_SetGyroDeadband(PifImuSensor* p_owner, uint8_t multiple);

/**
 * @brief Sets how the gyro is mounted on the board. IMUS_ALIGN_DEFAULT keeps what the driver set.
 * @param p_owner Pointer to the owner instance.
 * @param align Mounting of the gyro.
 */
void pifImuSensor_SetGyroAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @brief Reads the gyro in raw LSB, turned into the body frame by the mountings.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no gyro or the read failed.
 */
BOOL pifImuSensor_ReadRawGyro(PifImuSensor* p_owner, float* p_gyro);

/**
 * @brief Reads the gyro in degrees per second, with the calibrated bias and deadband removed, in the body frame.
 * @param p_owner Pointer to the owner instance.
 * @param p_gyro Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no gyro or the read failed.
 */
BOOL pifImuSensor_ReadGyro(PifImuSensor* p_owner, float* p_gyro);

/**
 * @brief Sets how the accelerometer is mounted on the board. IMUS_ALIGN_DEFAULT keeps what the driver set.
 * @param p_owner Pointer to the owner instance.
 * @param align Mounting of the accelerometer.
 */
void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @brief Reads the accelerometer in raw LSB, turned into the body frame by the mountings.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no accelerometer or the read failed.
 */
BOOL pifImuSensor_ReadRawAccel(PifImuSensor* p_owner, float* p_accel);

/**
 * @brief Reads the accelerometer in g, in the body frame.
 * @param p_owner Pointer to the owner instance.
 * @param p_accel Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no accelerometer or the read failed.
 */
BOOL pifImuSensor_ReadAccel(PifImuSensor* p_owner, float* p_accel);

/**
 * @brief Sets how the magnetometer is mounted on the board. IMUS_ALIGN_DEFAULT keeps what the driver set.
 * @param p_owner Pointer to the owner instance.
 * @param align Mounting of the magnetometer.
 */
void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @brief Reads the magnetometer in raw LSB, turned into the body frame by the mountings.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no magnetometer or the read failed.
 */
BOOL pifImuSensor_ReadRawMag(PifImuSensor* p_owner, float* p_mag);

/**
 * @brief Reads the magnetometer in Gauss, in the body frame.
 * @param p_owner Pointer to the owner instance.
 * @param p_mag Receives the values of the 3 axes.
 * @return TRUE on success, FALSE when there is no magnetometer or the read failed.
 */
BOOL pifImuSensor_ReadMag(PifImuSensor* p_owner, float* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_IMU_SENSOR_H
