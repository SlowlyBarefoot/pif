#include "sensor/pif_imu_sensor.h"

#include <math.h>


/**
 * @fn _alignBoard
 * @brief Internal helper that supports align board logic.
 * @param p_owner Pointer to the owner instance.
 * @param vec Parameter vec used by this operation.
 * @return None.
 */
static void _alignBoard(PifImuSensor* p_owner, float* vec)
{
    float x = vec[AXIS_X];
    float y = vec[AXIS_Y];
    float z = vec[AXIS_Z];

    vec[AXIS_X] = lrintf(p_owner->__board_rotation[0][0] * x + p_owner->__board_rotation[1][0] * y + p_owner->__board_rotation[2][0] * z);
    vec[AXIS_Y] = lrintf(p_owner->__board_rotation[0][1] * x + p_owner->__board_rotation[1][1] * y + p_owner->__board_rotation[2][1] * z);
    vec[AXIS_Z] = lrintf(p_owner->__board_rotation[0][2] * x + p_owner->__board_rotation[1][2] * y + p_owner->__board_rotation[2][2] * z);
}

/**
 * @fn _alignSensors
 * @brief Internal helper that supports align sensors logic.
 * @param p_owner Pointer to the owner instance.
 * @param src Pointer to source vector data.
 * @param dest Pointer to destination vector data.
 * @param rotation Rotation option used for axis alignment.
 * @return None.
 */
static void _alignSensors(PifImuSensor* p_owner, float* src, float* dest, uint8_t rotation)
{
    switch (rotation) {
        case IMUS_ALIGN_CW90_DEG:
            dest[AXIS_X] = src[AXIS_Y];
            dest[AXIS_Y] = -src[AXIS_X];
            dest[AXIS_Z] = src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW180_DEG:
            dest[AXIS_X] = -src[AXIS_X];
            dest[AXIS_Y] = -src[AXIS_Y];
            dest[AXIS_Z] = src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW270_DEG:
            dest[AXIS_X] = -src[AXIS_Y];
            dest[AXIS_Y] = src[AXIS_X];
            dest[AXIS_Z] = src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW0_DEG_FLIP:
            dest[AXIS_X] = -src[AXIS_X];
            dest[AXIS_Y] = src[AXIS_Y];
            dest[AXIS_Z] = -src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW90_DEG_FLIP:
            dest[AXIS_X] = src[AXIS_Y];
            dest[AXIS_Y] = src[AXIS_X];
            dest[AXIS_Z] = -src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW180_DEG_FLIP:
            dest[AXIS_X] = src[AXIS_X];
            dest[AXIS_Y] = -src[AXIS_Y];
            dest[AXIS_Z] = -src[AXIS_Z];
            break;
        case IMUS_ALIGN_CW270_DEG_FLIP:
            dest[AXIS_X] = -src[AXIS_Y];
            dest[AXIS_Y] = -src[AXIS_X];
            dest[AXIS_Z] = -src[AXIS_Z];
            break;
        default:	// IMUS_ALIGN_CW0_DEG:
            dest[AXIS_X] = src[AXIS_X];
            dest[AXIS_Y] = src[AXIS_Y];
            dest[AXIS_Z] = src[AXIS_Z];
            break;
    }

    if (p_owner->__board_alignment)
        _alignBoard(p_owner, dest);
}

void pifImuSensor_Init(PifImuSensor* p_owner)
{
	memset(p_owner, 0, sizeof(PifImuSensor));

	p_owner->_gyro_gain = 1;
	p_owner->_accel_gain = 1;
	p_owner->_mag_gain = 1;
}

void pifImuSensor_InitBoardAlignment(PifImuSensor* p_owner, int16_t board_align_roll, int16_t board_align_pitch, int16_t board_align_yaw)
{
    float roll, pitch, yaw;
    float cosx, sinx, cosy, siny, cosz, sinz;
    float coszcosx, coszcosy, sinzcosx, coszsinx, sinzsinx;

    // standard alignment, nothing to calculate
    if (!board_align_roll && !board_align_pitch && !board_align_yaw) return;

    p_owner->__board_alignment = TRUE;

    // deg2rad
    roll = board_align_roll * PIF_PI / 180.0f;
    pitch = board_align_pitch * PIF_PI / 180.0f;
    yaw = board_align_yaw * PIF_PI / 180.0f;

    cosx = cosf(roll);
    sinx = sinf(roll);
    cosy = cosf(pitch);
    siny = sinf(pitch);
    cosz = cosf(yaw);
    sinz = sinf(yaw);

    coszcosx = cosz * cosx;
    coszcosy = cosz * cosy;
    sinzcosx = sinz * cosx;
    coszsinx = sinx * cosz;
    sinzsinx = sinx * sinz;

    // define rotation matrix
    p_owner->__board_rotation[0][0] = coszcosy;
    p_owner->__board_rotation[0][1] = -cosy * sinz;
    p_owner->__board_rotation[0][2] = siny;

    p_owner->__board_rotation[1][0] = sinzcosx + (coszsinx * siny);
    p_owner->__board_rotation[1][1] = coszcosx - (sinzsinx * siny);
    p_owner->__board_rotation[1][2] = -sinx * cosy;

    p_owner->__board_rotation[2][0] = (sinzsinx) - (coszcosx * siny);
    p_owner->__board_rotation[2][1] = (coszsinx) + (sinzcosx * siny);
    p_owner->__board_rotation[2][2] = cosy * cosx;
}

void pifImuSensor_SetGyroAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__gyro_info.align = align;
}

BOOL pifImuSensor_ReadRawGyro(PifImuSensor* p_owner, float* p_gyro)
{
	int16_t data[AXIS_COUNT];
    float gyro[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_GYROSCOPE)) return FALSE;

	if (!(*p_owner->__gyro_info.read)(p_owner->__gyro_info.p_issuer, data)) return FALSE;

	gyro[AXIS_X] = data[AXIS_X];
	gyro[AXIS_Y] = data[AXIS_Y];
	gyro[AXIS_Z] = data[AXIS_Z];

	_alignSensors(p_owner, gyro, p_gyro, p_owner->__gyro_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadGyro(PifImuSensor* p_owner, float* p_gyro)
{
	int16_t data[AXIS_COUNT];
    float gyro[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_GYROSCOPE)) return FALSE;

	if (!(*p_owner->__gyro_info.read)(p_owner->__gyro_info.p_issuer, data)) return FALSE;

	if (p_owner->__use_calibrate) {
		gyro[AXIS_X] = (data[AXIS_X] - p_owner->__delta_gyro[AXIS_X]) / p_owner->_gyro_gain;
		gyro[AXIS_Y] = (data[AXIS_Y] - p_owner->__delta_gyro[AXIS_Y]) / p_owner->_gyro_gain;
		gyro[AXIS_Z] = (data[AXIS_Z] - p_owner->__delta_gyro[AXIS_Z]) / p_owner->_gyro_gain;
	}
	else {
		gyro[AXIS_X] = data[AXIS_X] / p_owner->_gyro_gain;
		gyro[AXIS_Y] = data[AXIS_Y] / p_owner->_gyro_gain;
		gyro[AXIS_Z] = data[AXIS_Z] / p_owner->_gyro_gain;
	}

	if (p_owner->__actual_threshold) {
		if (fabs(gyro[AXIS_X]) < p_owner->__threshold_gyro[AXIS_X]) gyro[AXIS_X] = 0;
		if (fabs(gyro[AXIS_Y]) < p_owner->__threshold_gyro[AXIS_Y]) gyro[AXIS_Y] = 0;
		if (fabs(gyro[AXIS_Z]) < p_owner->__threshold_gyro[AXIS_Z]) gyro[AXIS_Z] = 0;
	}

	_alignSensors(p_owner, gyro, p_gyro, p_owner->__gyro_info.align);
	return TRUE;
}

void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__accel_info.align = align;
}

BOOL pifImuSensor_ReadRawAccel(PifImuSensor* p_owner, float* p_accel)
{
	int16_t data[AXIS_COUNT];
    float accel[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_ACCELERO)) return FALSE;

	if (!(*p_owner->__accel_info.read)(p_owner->__accel_info.p_issuer, data)) return FALSE;

	accel[AXIS_X] = data[AXIS_X];
	accel[AXIS_Y] = data[AXIS_Y];
	accel[AXIS_Z] = data[AXIS_Z];

	_alignSensors(p_owner, accel, p_accel, p_owner->__accel_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadAccel(PifImuSensor* p_owner, float* p_accel)
{
	int16_t data[AXIS_COUNT];
    float accel[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_ACCELERO)) return FALSE;

	if (!(*p_owner->__accel_info.read)(p_owner->__accel_info.p_issuer, data)) return FALSE;

	accel[AXIS_X] = 9.80665f * data[AXIS_X] / p_owner->_accel_gain;
	accel[AXIS_Y] = 9.80665f * data[AXIS_Y] / p_owner->_accel_gain;
	accel[AXIS_Z] = 9.80665f * data[AXIS_Z] / p_owner->_accel_gain;

	_alignSensors(p_owner, accel, p_accel, p_owner->__accel_info.align);
	return TRUE;
}

void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__mag_info.align = align;
}

BOOL pifImuSensor_ReadRawMag(PifImuSensor* p_owner, float* p_mag)
{
	int16_t data[AXIS_COUNT];
    float mag[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_MAGNETO)) return FALSE;

	if (!(*p_owner->__mag_info.read)(p_owner->__mag_info.p_issuer, data)) return FALSE;

	mag[AXIS_X] = data[AXIS_X];
	mag[AXIS_Y] = data[AXIS_Y];
	mag[AXIS_Z] = data[AXIS_Z];

	_alignSensors(p_owner, mag, p_mag, p_owner->__mag_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadMag(PifImuSensor* p_owner, float* p_mag)
{
	int16_t data[AXIS_COUNT];
    float mag[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_MAGNETO)) return FALSE;

	if (!(*p_owner->__mag_info.read)(p_owner->__mag_info.p_issuer, data)) return FALSE;

	mag[AXIS_X] = data[AXIS_X] / p_owner->_mag_gain;
	mag[AXIS_Y] = data[AXIS_Y] / p_owner->_mag_gain;
	mag[AXIS_Z] = data[AXIS_Z] / p_owner->_mag_gain;

	_alignSensors(p_owner, mag, p_mag, p_owner->__mag_info.align);
	return TRUE;
}
