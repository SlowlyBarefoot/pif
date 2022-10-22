#ifndef PIF_IMU_SENSOR_H
#define PIF_IMU_SENSOR_H


#include "core/pif_i2c.h"


#define AXIS_X		0
#define AXIS_Y		1
#define AXIS_Z		2

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
 * @brief
 */
typedef struct StPifImuSensorInfo
{
	PifImuSensorAlign align;
	PifImuSensorRead read;
	PifIssuerP p_issuer;
} PifImuSensorInfo;

/**
 * @class StPifImuSensor
 * @brief
 */
typedef struct StPifImuSensor
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t	_measure;			// IMU_MEASURE_XXX
	double _gyro_gain;			// LSB/degree/s
	uint16_t _accel_gain;		// LSB/g
	uint16_t _mag_gain;			// LSB/Gauss

	// Private Member Variable
	PifImuSensorInfo __gyro_info;
	PifImuSensorInfo __accel_info;
	PifImuSensorInfo __mag_info;
	PifImuSensorInfo __temp_info;
	int16_t __delta_gyro[3];
	int16_t __threshold_gyro[3];
	int16_t __threshold[3];
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
 * @brief
 * @param p_owner
 */
void pifImuSensor_Init(PifImuSensor* p_owner);

/**
 * @fn pifImuSensor_InitBoardAlignment
 * @brief
 * @param p_owner
 * @param board_align_roll
 * @param board_align_pitch
 * @param board_align_yaw
 */
void pifImuSensor_InitBoardAlignment(PifImuSensor* p_owner, int16_t board_align_roll, int16_t board_align_pitch, int16_t board_align_yaw);

/**
 * @fn pifImuSensor_SetGyroAlign
 * @brief
 * @param p_owner
 * @param align
 */
void pifImuSensor_SetGyroAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadGyro
 * @brief
 * @param p_owner
 * @param p_gyro
 * @return
 */
BOOL pifImuSensor_ReadGyro(PifImuSensor* p_owner, int16_t* p_gyro);

/**
 * @fn pifImuSensor_ReadNormalizeGyro
 * @brief
 * @param p_owner
 * @param p_gyro
 * @return
 */
BOOL pifImuSensor_ReadNormalizeGyro(PifImuSensor* p_owner, float* p_gyro);

/**
 * @fn pifImuSensor_SetAccelAlign
 * @brief
 * @param p_owner
 * @param align
 */
void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadAccel
 * @brief
 * @param p_owner
 * @param p_accel
 * @return
 */
BOOL pifImuSensor_ReadAccel(PifImuSensor* p_owner, int16_t* p_accel);

/**
 * @fn pifImuSensor_ReadNormalizeAccel
 * @brief
 * @param p_owner
 * @param p_accel
 * @return
 */
BOOL pifImuSensor_ReadNormalizeAccel(PifImuSensor* p_owner, float* p_accel);

/**
 * @fn pifImuSensor_SetMagAlign
 * @brief
 * @param p_owner
 * @param align
 */
void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align);

/**
 * @fn pifImuSensor_ReadMag
 * @brief
 * @param p_owner
 * @param p_mag
 * @return
 */
BOOL pifImuSensor_ReadMag(PifImuSensor* p_owner, int16_t* p_mag);

#ifdef __cplusplus
}
#endif


#endif  // PIF_IMU_SENSOR_H
