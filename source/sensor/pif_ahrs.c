// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_ahrs.h"
#include "core/pif_math.h"

#include <math.h>

// Mahony complementary filter (Mahony, Hamel and Pflimlin, 2008) on a quaternion q that rotates
// body vectors into the earth frame. With R the matching rotation matrix, the estimated up in the
// body frame is v = R^T (0, 0, 1).

// The gyro bias is learned only while the corrections are small (below about 11 degrees), so the
// large error of a filter still converging does not wind the integral up.
#define BIAS_LEARN_MAX_ERROR	0.2f

const PifAhrsConfig pif_ahrs_default = {
	.kp = 1.0f,
	.ki = 0.05f,
	.mag_kp = 0.5f,
	.bias_limit_dps = 5.0f,
	.bias_learn_max_rate_dps = 0.0f,
	.accel_tolerance_g = 0.15f
};


/**
 * @fn _checkConfig
 * @param p_config Gains to check.
 * @return TRUE if none is negative, otherwise FALSE with E_INVALID_PARAM.
 */
static BOOL _checkConfig(const PifAhrsConfig* p_config)
{
	if (!p_config || p_config->kp < 0.0f || p_config->ki < 0.0f || p_config->mag_kp < 0.0f ||
			p_config->bias_limit_dps < 0.0f || p_config->bias_learn_max_rate_dps < 0.0f ||
			p_config->accel_tolerance_g < 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _normalize
 * @brief Scales a vector to unit length.
 * @param p_v Vector of n values.
 * @param n Number of values.
 * @return Length before scaling.
 */
static float _normalize(float* p_v, uint8_t n)
{
	float sum = 0.0f, scale;
	uint8_t i;

	for (i = 0; i < n; i++) sum += p_v[i] * p_v[i];
	if (sum <= 0.0f) return 0.0f;
	sum = sqrtf(sum);
	scale = 1.0f / sum;
	for (i = 0; i < n; i++) p_v[i] *= scale;
	return sum;
}

/**
 * @fn _up
 * @brief Works out the estimated up in the body frame, the third row of R.
 * @param p_q Quaternion.
 * @param p_v Up, 3 values.
 */
static void _up(const float* p_q, float* p_v)
{
	p_v[0] = 2.0f * (p_q[1] * p_q[3] - p_q[0] * p_q[2]);
	p_v[1] = 2.0f * (p_q[0] * p_q[1] + p_q[2] * p_q[3]);
	p_v[2] = p_q[0] * p_q[0] - p_q[1] * p_q[1] - p_q[2] * p_q[2] + p_q[3] * p_q[3];
}

/**
 * @fn _fromEuler
 * @brief Sets the quaternion of a rotation by yaw about Z, then pitch about Y, then roll about X,
 *        in the right-hand sense of the body frame.
 */
static void _fromEuler(float* p_q, float roll, float pitch, float yaw)
{
	float cr = pifMath_CosApprox(roll * 0.5f), sr = pifMath_SinApprox(roll * 0.5f);
	float cp = pifMath_CosApprox(pitch * 0.5f), sp = pifMath_SinApprox(pitch * 0.5f);
	float cy = pifMath_CosApprox(yaw * 0.5f), sy = pifMath_SinApprox(yaw * 0.5f);

	p_q[0] = cy * cp * cr + sy * sp * sr;
	p_q[1] = cy * cp * sr - sy * sp * cr;
	p_q[2] = cy * sp * cr + sy * cp * sr;
	p_q[3] = sy * cp * cr - cy * sp * sr;
}

/**
 * @fn _update
 * @brief Advances the estimate by one sample, with the heading from a magnetometer, from an
 *        external heading, or from neither.
 * @param p_owner Pointer to the instance.
 * @param p_gyro_dps Gyro rate in degrees per second.
 * @param p_accel_g Accelerometer in g, or NULL.
 * @param p_mag Magnetometer, or NULL.
 * @param p_heading_deg Heading clockwise from north in degrees, or NULL.
 * @param dt Time since the previous update in seconds.
 */
static void _update(PifAhrs* p_owner, const float* p_gyro_dps, const float* p_accel_g, const float* p_mag,
		const float* p_heading_deg, float dt)
{
	const PifAhrsConfig* p_config = &p_owner->__config;
	float* q = p_owner->_q;
	float w[3], v[3], a[3], m[3], h[3], error[3] = { 0.0f, 0.0f, 0.0f };
	float norm, hxy, yaw_error, theta2, half, scale, limit, raw;
	BOOL learn = TRUE;
	float q0, q1, q2, q3;
	uint8_t i;

	for (i = 0; i < 3; i++) w[i] = p_gyro_dps[i] * PIF_RAD;
	_up(q, v);
	if (p_config->bias_learn_max_rate_dps > 0.0f) {
		limit = p_config->bias_learn_max_rate_dps * PIF_RAD;
		if (w[0] * w[0] + w[1] * w[1] + w[2] * w[2] > limit * limit) learn = FALSE;
	}

	// Accelerometer: turn the estimated up toward the measured one.
	p_owner->_accel_used = FALSE;
	if (p_accel_g && p_config->kp > 0.0f) {
		a[0] = p_accel_g[0]; a[1] = p_accel_g[1]; a[2] = p_accel_g[2];
		norm = _normalize(a, 3);
		if (norm > 0.0f && (p_config->accel_tolerance_g <= 0.0f ||
				fabsf(norm - 1.0f) <= p_config->accel_tolerance_g)) {
			error[0] = a[1] * v[2] - a[2] * v[1];
			error[1] = a[2] * v[0] - a[0] * v[2];
			error[2] = a[0] * v[1] - a[1] * v[0];
			raw = error[0] * error[0] + error[1] * error[1] + error[2] * error[2];
			if (raw > BIAS_LEARN_MAX_ERROR * BIAS_LEARN_MAX_ERROR) learn = FALSE;
			for (i = 0; i < 3; i++) error[i] *= p_config->kp;
			p_owner->_accel_used = TRUE;
		}
	}

	// Magnetometer: the heading error about earth up, applied about the body's up alone.
	p_owner->_mag_used = FALSE;
	if (p_mag && p_config->mag_kp > 0.0f) {
		m[0] = p_mag[0]; m[1] = p_mag[1]; m[2] = p_mag[2];
		if (_normalize(m, 3) > 0.0f) {
			pifAhrs_BodyToEarth(p_owner, m, h);
			hxy = sqrtf(h[0] * h[0] + h[1] * h[1]);
			if (hxy > 0.0f) {
				// Sine of the angle from north to the horizontal field, counterclockwise.
				raw = -h[1] / hxy;
				if (fabsf(raw) > BIAS_LEARN_MAX_ERROR || h[0] < 0.0f) learn = FALSE;
				yaw_error = p_config->mag_kp * raw;
				for (i = 0; i < 3; i++) error[i] += yaw_error * v[i];
				p_owner->_mag_used = TRUE;
			}
		}
	}

	// External heading: the error about earth up is the sine of the heading error, weighted by how
	// level the nose is, since the heading of a nose pointing up says little.
	if (p_heading_deg && p_config->mag_kp > 0.0f) {
		raw = -(pifMath_SinApprox(*p_heading_deg * PIF_RAD) * (1.0f - 2.0f * (q[2] * q[2] + q[3] * q[3]))
				+ pifMath_CosApprox(*p_heading_deg * PIF_RAD) * 2.0f * (q[1] * q[2] + q[0] * q[3]));
		if (fabsf(raw) > BIAS_LEARN_MAX_ERROR) learn = FALSE;
		yaw_error = p_config->mag_kp * raw;
		for (i = 0; i < 3; i++) error[i] += yaw_error * v[i];
		p_owner->_mag_used = TRUE;
	}

	// Integral: learns the gyro bias, within the limit. Without ki the bias learned so far stays.
	if (p_config->ki > 0.0f && learn) {
		limit = p_config->bias_limit_dps * PIF_RAD;
		for (i = 0; i < 3; i++) {
			p_owner->__integral[i] += p_config->ki / (p_config->kp > 0.0f ? p_config->kp : 1.0f) * error[i] * dt;
			if (p_owner->__integral[i] > limit) p_owner->__integral[i] = limit;
			else if (p_owner->__integral[i] < -limit) p_owner->__integral[i] = -limit;
			p_owner->_bias_dps[i] = -p_owner->__integral[i] / PIF_RAD;
		}
	}
	for (i = 0; i < 3; i++) w[i] += error[i] + p_owner->__integral[i];

	// q = q * (cos(|w| dt / 2), sin(|w| dt / 2) w / |w|), with the sine and cosine to second order.
	theta2 = (w[0] * w[0] + w[1] * w[1] + w[2] * w[2]) * dt * dt;
	half = 1.0f - theta2 / 8.0f;
	scale = 0.5f * dt * (1.0f - theta2 / 24.0f);
	w[0] *= scale; w[1] *= scale; w[2] *= scale;
	q0 = q[0]; q1 = q[1]; q2 = q[2]; q3 = q[3];
	q[0] = q0 * half - q1 * w[0] - q2 * w[1] - q3 * w[2];
	q[1] = q1 * half + q0 * w[0] + q2 * w[2] - q3 * w[1];
	q[2] = q2 * half + q0 * w[1] - q1 * w[2] + q3 * w[0];
	q[3] = q3 * half + q0 * w[2] + q1 * w[1] - q2 * w[0];
	if (_normalize(q, 4) == 0.0f) pifAhrs_Reset(p_owner);
}

BOOL pifAhrs_Init(PifAhrs* p_owner, const PifAhrsConfig* p_config)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_config) p_config = &pif_ahrs_default;
	if (!_checkConfig(p_config)) return FALSE;

	memset(p_owner, 0, sizeof(PifAhrs));
	p_owner->__config = *p_config;
	pifAhrs_Reset(p_owner);
	return TRUE;
}

BOOL pifAhrs_SetConfig(PifAhrs* p_owner, const PifAhrsConfig* p_config)
{
	if (!_checkConfig(p_config)) return FALSE;
	p_owner->__config = *p_config;
	return TRUE;
}

void pifAhrs_SetDeclination(PifAhrs* p_owner, float declination_deg)
{
	p_owner->__declination_deg = declination_deg;
}

void pifAhrs_Reset(PifAhrs* p_owner)
{
	p_owner->_q[0] = 1.0f;
	p_owner->_q[1] = p_owner->_q[2] = p_owner->_q[3] = 0.0f;
	pifAhrs_ResetBias(p_owner);
	p_owner->_accel_used = FALSE;
	p_owner->_mag_used = FALSE;
}

void pifAhrs_ResetBias(PifAhrs* p_owner)
{
	uint8_t i;

	for (i = 0; i < 3; i++) {
		p_owner->__integral[i] = 0.0f;
		p_owner->_bias_dps[i] = 0.0f;
	}
}

BOOL pifAhrs_Align(PifAhrs* p_owner, const float* p_accel, const float* p_mag)
{
	float a[3] = { p_accel[0], p_accel[1], p_accel[2] };
	float roll, pitch, yaw = 0.0f, h[3];

	if (_normalize(a, 3) == 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// The accelerometer reads up, which for these angles is (-sin pitch, sin roll cos pitch,
	// cos roll cos pitch).
	roll = pifMath_Atan2Approx(a[1], a[2]);
	pitch = pifMath_Atan2Approx(-a[0], sqrtf(a[1] * a[1] + a[2] * a[2]));
	_fromEuler(p_owner->_q, roll, pitch, 0.0f);

	// Turn so that the horizontal part of the field points north.
	if (p_mag && (p_mag[0] != 0.0f || p_mag[1] != 0.0f || p_mag[2] != 0.0f)) {
		pifAhrs_BodyToEarth(p_owner, p_mag, h);
		if (h[0] != 0.0f || h[1] != 0.0f) yaw = -pifMath_Atan2Approx(h[1], h[0]);
	}
	_fromEuler(p_owner->_q, roll, pitch, yaw);
	return TRUE;
}

void pifAhrs_Update(PifAhrs* p_owner, const float* p_gyro_dps, const float* p_accel_g, const float* p_mag, float dt)
{
	_update(p_owner, p_gyro_dps, p_accel_g, p_mag, NULL, dt);
}

void pifAhrs_UpdateWithHeading(PifAhrs* p_owner, const float* p_gyro_dps, const float* p_accel_g, float heading_deg,
		float dt)
{
	_update(p_owner, p_gyro_dps, p_accel_g, NULL, &heading_deg, dt);
}

BOOL pifAhrs_SetQuaternion(PifAhrs* p_owner, float w, float x, float y, float z)
{
	float q[4] = { w, x, y, z };

	if (_normalize(q, 4) == 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	memcpy(p_owner->_q, q, sizeof(q));
	return TRUE;
}

void pifAhrs_GetEuler(const PifAhrs* p_owner, float* p_roll, float* p_pitch, float* p_heading)
{
	const float* q = p_owner->_q;
	float s, heading;

	// Angles of the right-hand rotations about the body axes, yaw then pitch then roll. Pitch
	// about Y (left) and yaw about Z (up) are turned into nose-up and clockwise.
	if (p_roll) {
		*p_roll = pifMath_Atan2Approx(2.0f * (q[0] * q[1] + q[2] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2])) / PIF_RAD;
	}
	if (p_pitch) {
		s = 2.0f * (q[0] * q[2] - q[3] * q[1]);
		if (s > 1.0f) s = 1.0f;
		else if (s < -1.0f) s = -1.0f;
		*p_pitch = -(PIF_PI / 2 - pifMath_AcosApprox(s)) / PIF_RAD;
	}
	if (p_heading) {
		heading = -pifMath_Atan2Approx(2.0f * (q[0] * q[3] + q[1] * q[2]), 1.0f - 2.0f * (q[2] * q[2] + q[3] * q[3])) / PIF_RAD;
		heading += p_owner->__declination_deg;
		while (heading < 0.0f) heading += 360.0f;
		while (heading >= 360.0f) heading -= 360.0f;
		*p_heading = heading;
	}
}

void pifAhrs_GetRotationMatrix(const PifAhrs* p_owner, float p_matrix[3][3])
{
	const float* q = p_owner->_q;

	p_matrix[0][0] = 1.0f - 2.0f * (q[2] * q[2] + q[3] * q[3]);
	p_matrix[0][1] = 2.0f * (q[1] * q[2] - q[0] * q[3]);
	p_matrix[0][2] = 2.0f * (q[1] * q[3] + q[0] * q[2]);
	p_matrix[1][0] = 2.0f * (q[1] * q[2] + q[0] * q[3]);
	p_matrix[1][1] = 1.0f - 2.0f * (q[1] * q[1] + q[3] * q[3]);
	p_matrix[1][2] = 2.0f * (q[2] * q[3] - q[0] * q[1]);
	p_matrix[2][0] = 2.0f * (q[1] * q[3] - q[0] * q[2]);
	p_matrix[2][1] = 2.0f * (q[2] * q[3] + q[0] * q[1]);
	p_matrix[2][2] = 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]);
}

void pifAhrs_BodyToEarth(const PifAhrs* p_owner, const float* p_in, float* p_out)
{
	float r[3][3], x = p_in[0], y = p_in[1], z = p_in[2];
	uint8_t i;

	pifAhrs_GetRotationMatrix(p_owner, r);
	for (i = 0; i < 3; i++) p_out[i] = r[i][0] * x + r[i][1] * y + r[i][2] * z;
}

void pifAhrs_EarthToBody(const PifAhrs* p_owner, const float* p_in, float* p_out)
{
	float r[3][3], x = p_in[0], y = p_in[1], z = p_in[2];
	uint8_t i;

	pifAhrs_GetRotationMatrix(p_owner, r);
	for (i = 0; i < 3; i++) p_out[i] = r[0][i] * x + r[1][i] * y + r[2][i] * z;
}

float pifAhrs_GetCosTilt(const PifAhrs* p_owner)
{
	const float* q = p_owner->_q;

	return q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
}
