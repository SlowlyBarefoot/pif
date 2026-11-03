// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_motion_profile.h
 * @brief Trapezoidal motion profile.
 *
 * Plans a move to a target position within a velocity, an acceleration and a deceleration limit,
 * and gives the position, the velocity and the acceleration at any time of the move. It has no
 * timer, task or output of its own: the caller samples it in its own period and drives a duty
 * motor, a step motor or a servo with the values.
 *
 * The profile is solved in closed form when a move starts, so a sample does not add up error and
 * may be taken at any time, in any order. A move too short to reach the velocity limit becomes a
 * triangle. pifMotionProfile_Move() plans from the state of the last sample, so the target may be
 * changed during a move: a move that would pass the target, or that goes away from it, brakes to
 * rest first and comes back.
 *
 * Every value is a float in units the caller chooses, as mm, degrees or steps, with time in
 * seconds. A float keeps about 7 digits, so positions far from 0 lose resolution.
 */
#ifndef PIF_MOTION_PROFILE_H
#define PIF_MOTION_PROFILE_H


#include "core/pif.h"


#define PIF_MOTION_PROFILE_SEGMENT_COUNT	4	///< Brake to rest, accelerate, cruise and decelerate


/**
 * @struct StPifMotionProfileSegment
 * @brief Part of a move with a constant acceleration.
 */
typedef struct StPifMotionProfileSegment
{
	float time;				///< Start, in seconds from the start of the move
	float position;			///< Position at the start
	float velocity;			///< Velocity at the start
	float acceleration;		///< Acceleration over the segment
} PifMotionProfileSegment;

/**
 * @struct StPifMotionProfile
 * @brief Limits and plan of one motion profile.
 */
typedef struct StPifMotionProfile
{
	// Read-only Member Variable, set by pifMotionProfile_Sample()
	float _position;		///< Position of the last sample
	float _velocity;		///< Velocity of the last sample
	float _acceleration;	///< Acceleration of the last sample
	BOOL _done;				///< The last sample is at or after the end of the move

	// Private Member Variable
	uint8_t __segment_count;
	PifMotionProfileSegment __segments[PIF_MOTION_PROFILE_SEGMENT_COUNT];
	float __v_max;
	float __a_max;
	float __d_max;
	float __target;
	float __duration;
} PifMotionProfile;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the limits of a profile and puts it at rest at position 0.
 * @param p_owner Pointer to the target object instance.
 * @param v_max Largest speed, above 0.
 * @param a_max Acceleration that speeds up, above 0.
 * @param d_max Deceleration that slows down, above 0.
 * @return TRUE on success. FALSE with E_INVALID_PARAM when a limit is not above 0.
 */
BOOL pifMotionProfile_Init(PifMotionProfile *p_owner, float v_max, float a_max, float d_max);

/**
 * @brief Changes the limits. They apply from the next pifMotionProfile_Move() or
 *        pifMotionProfile_Stop(); a move that runs keeps its plan. A move that starts faster than
 *        the new v_max slows down to it first.
 * @param p_owner Pointer to the target object instance.
 * @param v_max Largest speed, above 0.
 * @param a_max Acceleration that speeds up, above 0.
 * @param d_max Deceleration that slows down, above 0.
 * @return TRUE on success. FALSE with E_INVALID_PARAM when a limit is not above 0.
 */
BOOL pifMotionProfile_SetLimits(PifMotionProfile *p_owner, float v_max, float a_max, float d_max);

/**
 * @brief Puts the profile at rest at a position, as after homing, and drops the move.
 * @param p_owner Pointer to the target object instance.
 * @param position New position.
 */
void pifMotionProfile_SetPosition(PifMotionProfile *p_owner, float position);

/**
 * @brief Plans a move to a target from the position and the velocity of the last sample. Time 0
 *        of the new move is that sample.
 * @param p_owner Pointer to the target object instance.
 * @param target Position to stop at.
 */
void pifMotionProfile_Move(PifMotionProfile *p_owner, float target);

/**
 * @brief Plans a stop at d_max from the position and the velocity of the last sample. Time 0 of
 *        the stop is that sample.
 * @param p_owner Pointer to the target object instance.
 */
void pifMotionProfile_Stop(PifMotionProfile *p_owner);

/**
 * @brief Takes the state of the move at a time into _position, _velocity and _acceleration. At or
 *        after the end, the state is the target at rest and _done is TRUE.
 * @param p_owner Pointer to the target object instance.
 * @param time Seconds from the start of the move. A time below 0 is taken as 0.
 */
void pifMotionProfile_Sample(PifMotionProfile *p_owner, float time);

/**
 * @brief Gives the length of the move.
 * @param p_owner Pointer to the target object instance.
 * @return Seconds from the start to the end of the move, 0 for none.
 */
float pifMotionProfile_GetDuration(PifMotionProfile *p_owner);

/**
 * @brief Gives the first time the move is at a position, as the time of each pulse of a step
 *        motor: the time of step n is pifMotionProfile_TimeAtPosition(start + n).
 * @param p_owner Pointer to the target object instance.
 * @param position Position to look for.
 * @return Seconds from the start of the move, or -1 when the move does not reach the position.
 */
float pifMotionProfile_TimeAtPosition(PifMotionProfile *p_owner, float position);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MOTION_PROFILE_H
