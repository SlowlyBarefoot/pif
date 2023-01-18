#include "sensor/pif_imu_sensor.h"

#include <math.h>


static void _alignBoard2(PifImuSensor* p_owner, int16_t *vec)
{
    int16_t x = vec[AXIS_X];
    int16_t y = vec[AXIS_Y];
    int16_t z = vec[AXIS_Z];

    vec[AXIS_X] = lrintf(p_owner->__board_rotation[0][0] * x + p_owner->__board_rotation[1][0] * y + p_owner->__board_rotation[2][0] * z);
    vec[AXIS_Y] = lrintf(p_owner->__board_rotation[0][1] * x + p_owner->__board_rotation[1][1] * y + p_owner->__board_rotation[2][1] * z);
    vec[AXIS_Z] = lrintf(p_owner->__board_rotation[0][2] * x + p_owner->__board_rotation[1][2] * y + p_owner->__board_rotation[2][2] * z);
}

static void _alignSensors2(PifImuSensor* p_owner, int16_t* src, int16_t* dest, uint8_t rotation)
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
        _alignBoard2(p_owner, dest);
}

static void _alignBoard4(PifImuSensor* p_owner, int32_t *vec)
{
    int32_t x = vec[AXIS_X];
    int32_t y = vec[AXIS_Y];
    int32_t z = vec[AXIS_Z];

    vec[AXIS_X] = lrintf(p_owner->__board_rotation[0][0] * x + p_owner->__board_rotation[1][0] * y + p_owner->__board_rotation[2][0] * z);
    vec[AXIS_Y] = lrintf(p_owner->__board_rotation[0][1] * x + p_owner->__board_rotation[1][1] * y + p_owner->__board_rotation[2][1] * z);
    vec[AXIS_Z] = lrintf(p_owner->__board_rotation[0][2] * x + p_owner->__board_rotation[1][2] * y + p_owner->__board_rotation[2][2] * z);
}

static void _alignSensors4(PifImuSensor* p_owner, int16_t* src, int32_t* dest, uint8_t rotation)
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
        _alignBoard4(p_owner, dest);
}

void pifImuSensor_Init(PifImuSensor* p_owner)
{
	memset(p_owner, 0, sizeof(PifImuSensor));
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

BOOL pifImuSensor_ReadGyro2(PifImuSensor* p_owner, int16_t* p_gyro)
{
	int16_t gyro[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_GYROSCOPE) {
		if (!(*p_owner->__gyro_info.read)(p_owner->__gyro_info.p_issuer, gyro)) return FALSE;

		_alignSensors2(p_owner, gyro, p_gyro, p_owner->__gyro_info.align);
		return TRUE;
	}
	return FALSE;
}

BOOL pifImuSensor_ReadGyro4(PifImuSensor* p_owner, int32_t* p_gyro)
{
	int16_t gyro[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_GYROSCOPE) {
		if (!(*p_owner->__gyro_info.read)(p_owner->__gyro_info.p_issuer, gyro)) return FALSE;

		_alignSensors4(p_owner, gyro, p_gyro, p_owner->__gyro_info.align);
		return TRUE;
	}
	return FALSE;
}

BOOL pifImuSensor_ReadNormalizeGyro2(PifImuSensor* p_owner, float* p_gyro)
{
	int16_t gyro[AXIS_COUNT];

	if (!pifImuSensor_ReadGyro2(p_owner, gyro)) return FALSE;

	if (p_owner->__use_calibrate) {
		p_gyro[AXIS_X] = (gyro[AXIS_X] - p_owner->__delta_gyro[AXIS_X]) / p_owner->_gyro_gain;
		p_gyro[AXIS_Y] = (gyro[AXIS_Y] - p_owner->__delta_gyro[AXIS_Y]) / p_owner->_gyro_gain;
		p_gyro[AXIS_Z] = (gyro[AXIS_Z] - p_owner->__delta_gyro[AXIS_Z]) / p_owner->_gyro_gain;
	}
	else {
		p_gyro[AXIS_X] = gyro[AXIS_X] / p_owner->_gyro_gain;
		p_gyro[AXIS_Y] = gyro[AXIS_Y] / p_owner->_gyro_gain;
		p_gyro[AXIS_Z] = gyro[AXIS_Z] / p_owner->_gyro_gain;
	}

	if (p_owner->__actual_threshold) {
		if (abs(p_gyro[AXIS_X]) < p_owner->__threshold_gyro[AXIS_X]) p_gyro[AXIS_X] = 0;
		if (abs(p_gyro[AXIS_Y]) < p_owner->__threshold_gyro[AXIS_Y]) p_gyro[AXIS_Y] = 0;
		if (abs(p_gyro[AXIS_Z]) < p_owner->__threshold_gyro[AXIS_Z]) p_gyro[AXIS_Z] = 0;
	}
	return TRUE;
}

BOOL pifImuSensor_ReadNormalizeGyro4(PifImuSensor* p_owner, float* p_gyro)
{
	int32_t gyro[AXIS_COUNT];

	if (!pifImuSensor_ReadGyro4(p_owner, gyro)) return FALSE;

	if (p_owner->__use_calibrate) {
		p_gyro[AXIS_X] = (gyro[AXIS_X] - p_owner->__delta_gyro[AXIS_X]) / p_owner->_gyro_gain;
		p_gyro[AXIS_Y] = (gyro[AXIS_Y] - p_owner->__delta_gyro[AXIS_Y]) / p_owner->_gyro_gain;
		p_gyro[AXIS_Z] = (gyro[AXIS_Z] - p_owner->__delta_gyro[AXIS_Z]) / p_owner->_gyro_gain;
	}
	else {
		p_gyro[AXIS_X] = gyro[AXIS_X] / p_owner->_gyro_gain;
		p_gyro[AXIS_Y] = gyro[AXIS_Y] / p_owner->_gyro_gain;
		p_gyro[AXIS_Z] = gyro[AXIS_Z] / p_owner->_gyro_gain;
	}

	if (p_owner->__actual_threshold) {
		if (abs(p_gyro[AXIS_X]) < p_owner->__threshold_gyro[AXIS_X]) p_gyro[AXIS_X] = 0;
		if (abs(p_gyro[AXIS_Y]) < p_owner->__threshold_gyro[AXIS_Y]) p_gyro[AXIS_Y] = 0;
		if (abs(p_gyro[AXIS_Z]) < p_owner->__threshold_gyro[AXIS_Z]) p_gyro[AXIS_Z] = 0;
	}
	return TRUE;
}

void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__accel_info.align = align;
}

BOOL pifImuSensor_ReadAccel2(PifImuSensor* p_owner, int16_t* p_accel)
{
	int16_t accel[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_ACCELERO) {
		if (!(*p_owner->__accel_info.read)(p_owner->__accel_info.p_issuer, accel)) return FALSE;

		_alignSensors2(p_owner, accel, p_accel, p_owner->__accel_info.align);
		return TRUE;
	}
	return FALSE;
}

BOOL pifImuSensor_ReadAccel4(PifImuSensor* p_owner, int32_t* p_accel)
{
	int16_t accel[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_ACCELERO) {
		if (!(*p_owner->__accel_info.read)(p_owner->__accel_info.p_issuer, accel)) return FALSE;

		_alignSensors4(p_owner, accel, p_accel, p_owner->__accel_info.align);
		return TRUE;
	}
	return FALSE;
}

BOOL pifImuSensor_ReadNormalizeAccel2(PifImuSensor* p_owner, float* p_accel)
{
	int16_t accel[AXIS_COUNT];

	if (!pifImuSensor_ReadAccel2(p_owner, accel)) return FALSE;

	p_accel[AXIS_X] = 9.80665f * accel[AXIS_X] / p_owner->_accel_gain;
	p_accel[AXIS_Y] = 9.80665f * accel[AXIS_Y] / p_owner->_accel_gain;
	p_accel[AXIS_Z] = 9.80665f * accel[AXIS_Z] / p_owner->_accel_gain;
	return TRUE;
}

BOOL pifImuSensor_ReadNormalizeAccel4(PifImuSensor* p_owner, float* p_accel)
{
	int32_t accel[AXIS_COUNT];

	if (!pifImuSensor_ReadAccel4(p_owner, accel)) return FALSE;

	p_accel[AXIS_X] = 9.80665f * accel[AXIS_X] / p_owner->_accel_gain;
	p_accel[AXIS_Y] = 9.80665f * accel[AXIS_Y] / p_owner->_accel_gain;
	p_accel[AXIS_Z] = 9.80665f * accel[AXIS_Z] / p_owner->_accel_gain;
	return TRUE;
}

void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__mag_info.align = align;
}

BOOL pifImuSensor_ReadMag2(PifImuSensor* p_owner, int16_t* p_mag)
{
	int16_t mag[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_MAGNETO) {
		if (!(*p_owner->__mag_info.read)(p_owner->__mag_info.p_issuer, mag)) return FALSE;

		_alignSensors2(p_owner, mag, p_mag, p_owner->__mag_info.align);
		return TRUE;
	}
	return FALSE;
}

BOOL pifImuSensor_ReadMag4(PifImuSensor* p_owner, int32_t* p_mag)
{
	int16_t mag[AXIS_COUNT];

	if (p_owner->_measure & IMU_MEASURE_MAGNETO) {
		if (!(*p_owner->__mag_info.read)(p_owner->__mag_info.p_issuer, mag)) return FALSE;

		_alignSensors4(p_owner, mag, p_mag, p_owner->__mag_info.align);
		return TRUE;
	}
	return FALSE;
}
