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
    IMUS_ALIGN_DEFAULT,			// driver-provided alignment
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
	int16_t __delta_gyro[AXIS_COUNT];
	int16_t __threshold_gyro[AXIS_COUNT];
	int16_t __threshold[AXIS_COUNT];
	BOOL __use_calibrate;
	float __actual_threshold;
	BOOL __board_alignment;		// board orientation correction
	float __board_rotation[3][3];
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
 * @brief Initializes imu sensor init board alignment and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param board_align_roll Parameter board_align_roll used by this operation.
 * @param board_align_pitch Parameter board_align_pitch used by this operation.
 * @param board_align_yaw Parameter board_align_yaw used by this operation.
 */
void pifImuSensor_InitBoardAlignment(PifImuSensor* p_owner, int16_t board_align_roll, int16_t board_align_pitch, int16_t board_align_yaw);

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
