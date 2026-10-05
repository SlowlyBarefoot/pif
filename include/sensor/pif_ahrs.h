// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_AHRS_H
#define PIF_AHRS_H


#include "core/pif.h"


/*
 * Attitude and heading estimate from a gyro, an accelerometer and optionally a magnetometer: a
 * Mahony complementary filter on a quaternion, after R. Mahony, T. Hamel and J.-M. Pflimlin,
 * "Nonlinear Complementary Filters on the Special Orthogonal Group", IEEE Transactions on
 * Automatic Control, 2008.
 *
 * The gyro rate is integrated into the attitude. The accelerometer, while it reads close to 1 g,
 * points at up; the error between it and the estimated up, a x v, turns the estimate toward it
 * with gain kp and, through the integral with gain ki, learns the gyro bias. The magnetometer is
 * projected onto the horizontal plane and corrects the heading alone, so a disturbed field cannot
 * tilt the estimate. Instead of a magnetometer, an external heading such as the course over ground
 * of a GPS can correct the heading through pifAhrs_UpdateWithHeading().
 *
 * Frames: the body is X forward, Y left, Z up, and the earth is X north, Y west, Z up. A level,
 * still accelerometer reads (0, 0, +1 g), which is how most MEMS chips read lying face up; use
 * pif_imu_sensor alignment to bring a sensor into this frame. Rates follow the right-hand rule
 * about these axes. The angles given out follow the usual conventions instead: roll positive
 * with the right side down, pitch positive with the nose up, heading clockwise from north.
 */


/**
 * @class StPifAhrsConfig
 * @brief Gains of the filter.
 */
typedef struct StPifAhrsConfig
{
	float kp;					// Accelerometer correction, 1/s; larger trusts the accelerometer more
	float ki;					// Gyro bias learning, 1/s^2, or 0 to stop learning and keep the bias learned
	float mag_kp;				// Heading correction from a magnetometer or an external heading, 1/s, or 0 for none
	float bias_limit_dps;		// Largest gyro bias the integral may learn
	float bias_learn_max_rate_dps;	// The bias is learned only below this rotation rate; 0 for any rate
	float accel_tolerance_g;	// Skip the accelerometer while its magnitude is off 1 g by more; 0 for never
} PifAhrsConfig;

// kp 1.0, ki 0.05, mag_kp 0.5, bias limit 5 deg/s at any rate, accelerometer used within 1 g +- 0.15 g.
extern const PifAhrsConfig pif_ahrs_default;


/**
 * @class StPifAhrs
 * @brief Mahony attitude and heading estimate.
 */
typedef struct StPifAhrs
{
	// Read-only Member Variable
	float _q[4];				// Attitude quaternion w, x, y, z, rotating body vectors into the earth frame
	float _bias_dps[3];			// Gyro bias learned by the integral, subtracted from the gyro
	BOOL _accel_used;			// The last update corrected with the accelerometer
	BOOL _mag_used;				// The last update corrected with the magnetometer

	// Private Member Variable
	PifAhrsConfig __config;
	float __integral[3];		// rad/s
	float __declination_deg;
} PifAhrs;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifAhrs_Init
 * @brief Initializes the filter level and facing north.
 * @param p_owner Pointer to the instance.
 * @param p_config Gains, or NULL for pif_ahrs_default. They are copied.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifAhrs_Init(PifAhrs* p_owner, const PifAhrsConfig* p_config);

/**
 * @fn pifAhrs_SetConfig
 * @brief Replaces the gains and keeps the attitude.
 * @param p_owner Pointer to the instance.
 * @param p_config Gains. They are copied.
 * @return TRUE on success, otherwise FALSE for a negative gain or tolerance.
 */
BOOL pifAhrs_SetConfig(PifAhrs* p_owner, const PifAhrsConfig* p_config);

/**
 * @fn pifAhrs_SetDeclination
 * @brief Sets the magnetic declination, added to the heading so that it is from true north.
 * @param p_owner Pointer to the instance.
 * @param declination_deg Declination in degrees, positive east.
 */
void pifAhrs_SetDeclination(PifAhrs* p_owner, float declination_deg);

/**
 * @fn pifAhrs_Reset
 * @brief Sets the attitude level and facing north, and forgets the learned bias.
 * @param p_owner Pointer to the instance.
 */
void pifAhrs_Reset(PifAhrs* p_owner);

/**
 * @fn pifAhrs_ResetBias
 * @brief Forgets the gyro bias learned so far, keeping the attitude.
 * @param p_owner Pointer to the instance.
 */
void pifAhrs_ResetBias(PifAhrs* p_owner);

/**
 * @fn pifAhrs_Align
 * @brief Sets the attitude straight from the sensors of a vehicle at rest, instead of waiting for
 *        the filter to converge.
 * @param p_owner Pointer to the instance.
 * @param p_accel Accelerometer, 3 values in any unit.
 * @param p_mag Magnetometer, 3 values in any unit, or NULL to face north.
 * @return TRUE on success, otherwise FALSE for a zero accelerometer reading.
 */
BOOL pifAhrs_Align(PifAhrs* p_owner, const float* p_accel, const float* p_mag);

/**
 * @fn pifAhrs_Update
 * @brief Advances the estimate by one sample.
 * @param p_owner Pointer to the instance.
 * @param p_gyro_dps Gyro rate, 3 values in degrees per second.
 * @param p_accel_g Accelerometer, 3 values in g, or NULL to skip the correction.
 * @param p_mag Magnetometer, 3 values in any unit, or NULL to skip the heading correction.
 * @param dt Time since the previous update in seconds.
 */
void pifAhrs_Update(PifAhrs* p_owner, const float* p_gyro_dps, const float* p_accel_g, const float* p_mag, float dt);

/**
 * @fn pifAhrs_UpdateWithHeading
 * @brief Advances the estimate by one sample, correcting the heading from an external heading,
 *        such as the course over ground of a GPS, instead of a magnetometer. The correction is
 *        weighted by how level the nose is, and has the gain mag_kp.
 * @param p_owner Pointer to the instance.
 * @param p_gyro_dps Gyro rate, 3 values in degrees per second.
 * @param p_accel_g Accelerometer, 3 values in g, or NULL to skip the correction.
 * @param heading_deg Heading in degrees, clockwise from north.
 * @param dt Time since the previous update in seconds.
 */
void pifAhrs_UpdateWithHeading(PifAhrs* p_owner, const float* p_gyro_dps, const float* p_accel_g, float heading_deg,
		float dt);

/**
 * @fn pifAhrs_SetQuaternion
 * @brief Sets the attitude, for example from a simulator.
 * @param p_owner Pointer to the instance.
 * @param w Quaternion w, x, y, z rotating body vectors into the earth frame. It is normalized.
 * @return TRUE on success, otherwise FALSE for a zero quaternion.
 */
BOOL pifAhrs_SetQuaternion(PifAhrs* p_owner, float w, float x, float y, float z);

/**
 * @fn pifAhrs_GetEuler
 * @brief Returns the attitude as angles.
 * @param p_owner Pointer to the instance.
 * @param p_roll Roll in degrees, -180 to 180, positive with the right side down. May be NULL.
 * @param p_pitch Pitch in degrees, -90 to 90, positive with the nose up. May be NULL.
 * @param p_heading Heading in degrees, 0 to 360, clockwise from north. May be NULL.
 */
void pifAhrs_GetEuler(const PifAhrs* p_owner, float* p_roll, float* p_pitch, float* p_heading);

/**
 * @fn pifAhrs_GetRotationMatrix
 * @brief Returns the rotation matrix from the body frame into the earth frame.
 * @param p_owner Pointer to the instance.
 * @param p_matrix 3 x 3 matrix, row-major: earth = matrix * body.
 */
void pifAhrs_GetRotationMatrix(const PifAhrs* p_owner, float p_matrix[3][3]);

/**
 * @fn pifAhrs_BodyToEarth
 * @brief Rotates a vector from the body frame into the earth frame, for example an acceleration
 *        to get its vertical part.
 * @param p_owner Pointer to the instance.
 * @param p_in Body-frame vector.
 * @param p_out Earth-frame vector. May be p_in.
 */
void pifAhrs_BodyToEarth(const PifAhrs* p_owner, const float* p_in, float* p_out);

/**
 * @fn pifAhrs_EarthToBody
 * @brief Rotates a vector from the earth frame into the body frame.
 * @param p_owner Pointer to the instance.
 * @param p_in Earth-frame vector.
 * @param p_out Body-frame vector. May be p_in.
 */
void pifAhrs_EarthToBody(const PifAhrs* p_owner, const float* p_in, float* p_out);

/**
 * @fn pifAhrs_GetCosTilt
 * @brief Returns the cosine of the angle between the body Z axis and up, 1 when level, 0 on the side.
 * @param p_owner Pointer to the instance.
 * @return Cosine of the tilt, -1 to 1.
 */
float pifAhrs_GetCosTilt(const PifAhrs* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_AHRS_H
