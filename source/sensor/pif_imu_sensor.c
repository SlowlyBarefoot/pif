// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_imu_sensor.h"

#include <math.h>


// Mounting orientations as signed axis selections. Entry [align][i] gives the
// sensor axis that feeds output axis i, as (axis + 1), negated when the sign flips.
// CWn: the sensor is turned n degrees clockwise about its Z axis, seen from above.
// FLIP: the sensor is first turned upside down about its Y axis, then turned CWn.
static const int8_t c_mounting[][AXIS_COUNT] = {
	{  1,  2,  3 },		// IMUS_ALIGN_DEFAULT (same as CW0)
	{  1,  2,  3 },		// IMUS_ALIGN_CW0_DEG
	{  2, -1,  3 },		// IMUS_ALIGN_CW90_DEG
	{ -1, -2,  3 },		// IMUS_ALIGN_CW180_DEG
	{ -2,  1,  3 },		// IMUS_ALIGN_CW270_DEG
	{ -1,  2, -3 },		// IMUS_ALIGN_CW0_DEG_FLIP
	{  2,  1, -3 },		// IMUS_ALIGN_CW90_DEG_FLIP
	{  1, -2, -3 },		// IMUS_ALIGN_CW180_DEG_FLIP
	{ -2, -1, -3 },		// IMUS_ALIGN_CW270_DEG_FLIP
};

/**
 * @fn _orient
 * @brief Maps a sensor-frame vector to the body frame: first the mounting
 *        orientation of the sensor on the board, then the board rotation.
 * @param p_owner Pointer to the owner instance.
 * @param p_in Vector in the sensor frame.
 * @param p_out Vector in the body frame.
 * @param align Mounting orientation of the sensor.
 * @return None.
 */
static void _orient(PifImuSensor* p_owner, const float* p_in, float* p_out, PifImuSensorAlign align)
{
	const int8_t* p_map = c_mounting[align < sizeof(c_mounting) / sizeof(c_mounting[0]) ? align : IMUS_ALIGN_DEFAULT];
	float board[AXIS_COUNT];
	int i;

	for (i = 0; i < AXIS_COUNT; i++) {
		board[i] = p_map[i] > 0 ? p_in[p_map[i] - 1] : -p_in[-p_map[i] - 1];
	}

	if (!p_owner->__board_alignment) {
		memcpy(p_out, board, sizeof(board));
		return;
	}
	for (i = 0; i < AXIS_COUNT; i++) {
		p_out[i] = p_owner->__board_matrix[i][0] * board[AXIS_X]
				+ p_owner->__board_matrix[i][1] * board[AXIS_Y]
				+ p_owner->__board_matrix[i][2] * board[AXIS_Z];
	}
}

/**
 * @fn _readSensor
 * @brief Reads one sample from a sensor callback as floats.
 * @param p_info Sensor callback and issuer.
 * @param p_out Destination, AXIS_COUNT values.
 * @return TRUE on success, FALSE on failure.
 */
static BOOL _readSensor(PifImuSensorInfo* p_info, float* p_out)
{
	int16_t data[AXIS_COUNT];
	int i;

	if (!(*p_info->read)(p_info->p_issuer, data)) return FALSE;
	for (i = 0; i < AXIS_COUNT; i++) p_out[i] = data[i];
	return TRUE;
}

static void _updateGyroDeadband(PifImuSensor* p_owner)
{
	int i;

	for (i = 0; i < AXIS_COUNT; i++) {
		p_owner->__gyro_deadband[i] = p_owner->__gyro_calibrated ? p_owner->__gyro_noise[i] * p_owner->__deadband_multiple : 0;
	}
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
	const float to_rad = PIF_PI / 180.0f;
	float cr, sr, cp, sp, cy, sy;

	p_owner->__board_alignment = board_align_roll || board_align_pitch || board_align_yaw;
	if (!p_owner->__board_alignment) return;

	cr = cosf(board_align_roll * to_rad);
	sr = sinf(board_align_roll * to_rad);
	cp = cosf(board_align_pitch * to_rad);
	sp = sinf(board_align_pitch * to_rad);
	cy = cosf(board_align_yaw * to_rad);
	sy = sinf(board_align_yaw * to_rad);

	// Positive angles turn the board clockwise, seen from the positive end of the axis
	// looking back at the origin (from above for yaw), the same sense as IMUS_ALIGN_CWn.
	// A clockwise turn by a is a right-hand turn by -a, so a board-frame vector v maps
	// to the body frame as R * v with R = Rz(-yaw) * Ry(-pitch) * Rx(-roll).
	p_owner->__board_matrix[0][0] = cy * cp;
	p_owner->__board_matrix[0][1] = cy * sp * sr + sy * cr;
	p_owner->__board_matrix[0][2] = sy * sr - cy * sp * cr;
	p_owner->__board_matrix[1][0] = -sy * cp;
	p_owner->__board_matrix[1][1] = cy * cr - sy * sp * sr;
	p_owner->__board_matrix[1][2] = sy * sp * cr + cy * sr;
	p_owner->__board_matrix[2][0] = sp;
	p_owner->__board_matrix[2][1] = -cp * sr;
	p_owner->__board_matrix[2][2] = cp * cr;
}

void pifImuSensor_ResetGyroCalibration(PifImuSensor* p_owner)
{
	p_owner->__gyro_calibrated = FALSE;
	p_owner->__deadband_multiple = 0;
	memset(p_owner->__gyro_bias, 0, sizeof(p_owner->__gyro_bias));
	memset(p_owner->__gyro_noise, 0, sizeof(p_owner->__gyro_noise));
	memset(p_owner->__gyro_deadband, 0, sizeof(p_owner->__gyro_deadband));
}

BOOL pifImuSensor_CalibrateGyro(PifImuSensor* p_owner, uint16_t samples, uint16_t interval_ms)
{
	float sample[AXIS_COUNT];
	float mean[AXIS_COUNT] = { 0, 0, 0 };
	float m2[AXIS_COUNT] = { 0, 0, 0 };
	float delta;
	uint16_t n;
	int i;

	if (!(p_owner->_measure & IMU_MEASURE_GYROSCOPE) || !samples) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Welford's running mean and variance. The sensor has to stay still; the CPU is
	// held for samples * interval_ms.
	for (n = 1; n <= samples; n++) {
		if (!_readSensor(&p_owner->__gyro_info, sample)) return FALSE;
		for (i = 0; i < AXIS_COUNT; i++) {
			delta = sample[i] - mean[i];
			mean[i] += delta / n;
			m2[i] += delta * (sample[i] - mean[i]);
		}
		if (interval_ms) pif_Delay1ms(interval_ms);
	}

	for (i = 0; i < AXIS_COUNT; i++) {
		p_owner->__gyro_bias[i] = mean[i];
		p_owner->__gyro_noise[i] = sqrtf(m2[i] / samples);
	}
	p_owner->__gyro_calibrated = TRUE;
	_updateGyroDeadband(p_owner);
	return TRUE;
}

BOOL pifImuSensor_SetGyroDeadband(PifImuSensor* p_owner, uint8_t multiple)
{
	p_owner->__deadband_multiple = multiple;
	_updateGyroDeadband(p_owner);
	return TRUE;
}

void pifImuSensor_SetGyroAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__gyro_info.align = align;
}

BOOL pifImuSensor_ReadRawGyro(PifImuSensor* p_owner, float* p_gyro)
{
	float raw[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_GYROSCOPE)) return FALSE;
	if (!_readSensor(&p_owner->__gyro_info, raw)) return FALSE;

	_orient(p_owner, raw, p_gyro, p_owner->__gyro_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadGyro(PifImuSensor* p_owner, float* p_gyro)
{
	float rate[AXIS_COUNT];
	int i;

	if (!(p_owner->_measure & IMU_MEASURE_GYROSCOPE)) return FALSE;
	if (!_readSensor(&p_owner->__gyro_info, rate)) return FALSE;

	// Bias and deadband are in raw LSB, so they apply before the gain.
	for (i = 0; i < AXIS_COUNT; i++) {
		rate[i] -= p_owner->__gyro_bias[i];
		if (fabsf(rate[i]) < p_owner->__gyro_deadband[i]) rate[i] = 0;
		rate[i] /= p_owner->_gyro_gain;
	}

	_orient(p_owner, rate, p_gyro, p_owner->__gyro_info.align);
	return TRUE;
}

void pifImuSensor_SetAccelAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__accel_info.align = align;
}

BOOL pifImuSensor_ReadRawAccel(PifImuSensor* p_owner, float* p_accel)
{
	float raw[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_ACCELERO)) return FALSE;
	if (!_readSensor(&p_owner->__accel_info, raw)) return FALSE;

	_orient(p_owner, raw, p_accel, p_owner->__accel_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadAccel(PifImuSensor* p_owner, float* p_accel)
{
	float accel[AXIS_COUNT];
	int i;

	if (!(p_owner->_measure & IMU_MEASURE_ACCELERO)) return FALSE;
	if (!_readSensor(&p_owner->__accel_info, accel)) return FALSE;

	for (i = 0; i < AXIS_COUNT; i++) accel[i] = 9.80665f * accel[i] / p_owner->_accel_gain;	// g to m/s^2

	_orient(p_owner, accel, p_accel, p_owner->__accel_info.align);
	return TRUE;
}

void pifImuSensor_SetMagAlign(PifImuSensor* p_owner, PifImuSensorAlign align)
{
    if (align > IMUS_ALIGN_DEFAULT)
        p_owner->__mag_info.align = align;
}

BOOL pifImuSensor_ReadRawMag(PifImuSensor* p_owner, float* p_mag)
{
	float raw[AXIS_COUNT];

	if (!(p_owner->_measure & IMU_MEASURE_MAGNETO)) return FALSE;
	if (!_readSensor(&p_owner->__mag_info, raw)) return FALSE;

	_orient(p_owner, raw, p_mag, p_owner->__mag_info.align);
	return TRUE;
}

BOOL pifImuSensor_ReadMag(PifImuSensor* p_owner, float* p_mag)
{
	float mag[AXIS_COUNT];
	int i;

	if (!(p_owner->_measure & IMU_MEASURE_MAGNETO)) return FALSE;
	if (!_readSensor(&p_owner->__mag_info, mag)) return FALSE;

	for (i = 0; i < AXIS_COUNT; i++) mag[i] /= p_owner->_mag_gain;

	_orient(p_owner, mag, p_mag, p_owner->__mag_info.align);
	return TRUE;
}
