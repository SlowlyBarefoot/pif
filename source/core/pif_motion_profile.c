// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_motion_profile.h"

#include <math.h>

// Trapezoidal motion profile planned in closed form.

/**
 * @fn _checkLimits
 * @return TRUE when every limit is above 0, otherwise FALSE with E_INVALID_PARAM.
 */
static BOOL _checkLimits(float v_max, float a_max, float d_max)
{
	if (!(v_max > 0.0f) || !(a_max > 0.0f) || !(d_max > 0.0f)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _addSegment
 * @brief Adds a segment that starts at the state given and moves the state to its end.
 * @param p_owner Pointer to the target object instance.
 * @param p_time Time of the start, set to the end.
 * @param p_position Position of the start, set to the end.
 * @param p_velocity Velocity of the start, set to the end.
 * @param duration Length of the segment. A segment of no length is not added.
 * @param acceleration Acceleration over the segment.
 */
static void _addSegment(PifMotionProfile *p_owner, float *p_time, float *p_position, float *p_velocity,
		float duration, float acceleration)
{
	PifMotionProfileSegment *p_segment;

	if (!(duration > 0.0f)) return;
	p_segment = &p_owner->__segments[p_owner->__segment_count++];
	p_segment->time = *p_time;
	p_segment->position = *p_position;
	p_segment->velocity = *p_velocity;
	p_segment->acceleration = acceleration;
	*p_time += duration;
	*p_position += (*p_velocity + 0.5f * acceleration * duration) * duration;
	*p_velocity += acceleration * duration;
}

/**
 * @fn _plan
 * @brief Plans the move from the state of the last sample to __target.
 * @param p_owner Pointer to the target object instance.
 */
static void _plan(PifMotionProfile *p_owner)
{
	float time = 0.0f;
	float position = p_owner->_position;
	float velocity = p_owner->_velocity;
	float a = p_owner->__a_max, d = p_owner->__d_max, v_max = p_owner->__v_max;
	float dir, speed, distance, peak, rate, dist_change, dist_stop;

	p_owner->__segment_count = 0;

	// Moving away from the target, or too fast to stop at it: brake to rest, then start over from there.
	distance = p_owner->__target - position;
	if (velocity * distance < 0.0f || (velocity != 0.0f && velocity * velocity / (2.0f * d) > fabsf(distance))) {
		_addSegment(p_owner, &time, &position, &velocity, fabsf(velocity) / d, velocity > 0.0f ? -d : d);
		velocity = 0.0f;
		distance = p_owner->__target - position;
	}

	// The rest goes one way: work with the speed and the distance along it.
	if (distance != 0.0f) {
		dir = distance > 0.0f ? 1.0f : -1.0f;
		speed = dir * velocity;
		distance = fabsf(distance);

		peak = v_max;
		rate = speed > peak ? d : a;
		dist_change = fabsf(peak * peak - speed * speed) / (2.0f * rate);
		dist_stop = peak * peak / (2.0f * d);
		if (dist_change + dist_stop > distance) {
			// Too short to reach v_max, which only a move that starts below it can be: a triangle.
			peak = sqrtf((2.0f * a * d * distance + d * speed * speed) / (a + d));
			if (peak < speed) peak = speed;
			dist_change = (peak * peak - speed * speed) / (2.0f * a);
			dist_stop = peak * peak / (2.0f * d);
		}

		_addSegment(p_owner, &time, &position, &velocity, fabsf(peak - speed) / rate,
				dir * (peak >= speed ? a : -d));
		velocity = dir * peak;
		_addSegment(p_owner, &time, &position, &velocity, (distance - dist_change - dist_stop) / peak, 0.0f);
		_addSegment(p_owner, &time, &position, &velocity, peak / d, -dir * d);
	}

	p_owner->__duration = time;
	p_owner->_done = p_owner->__segment_count == 0;
}

BOOL pifMotionProfile_Init(PifMotionProfile *p_owner, float v_max, float a_max, float d_max)
{
	if (!_checkLimits(v_max, a_max, d_max)) return FALSE;

	memset(p_owner, 0, sizeof(PifMotionProfile));
	p_owner->__v_max = v_max;
	p_owner->__a_max = a_max;
	p_owner->__d_max = d_max;
	p_owner->_done = TRUE;
	return TRUE;
}

BOOL pifMotionProfile_SetLimits(PifMotionProfile *p_owner, float v_max, float a_max, float d_max)
{
	if (!_checkLimits(v_max, a_max, d_max)) return FALSE;

	p_owner->__v_max = v_max;
	p_owner->__a_max = a_max;
	p_owner->__d_max = d_max;
	return TRUE;
}

void pifMotionProfile_SetPosition(PifMotionProfile *p_owner, float position)
{
	p_owner->_position = position;
	p_owner->_velocity = 0.0f;
	p_owner->_acceleration = 0.0f;
	p_owner->_done = TRUE;
	p_owner->__target = position;
	p_owner->__duration = 0.0f;
	p_owner->__segment_count = 0;
}

void pifMotionProfile_Move(PifMotionProfile *p_owner, float target)
{
	p_owner->__target = target;
	_plan(p_owner);
}

void pifMotionProfile_Stop(PifMotionProfile *p_owner)
{
	float velocity = p_owner->_velocity;

	// Where braking at d_max comes to rest, which _plan() reaches with the brake alone.
	p_owner->__target = p_owner->_position + velocity * fabsf(velocity) / (2.0f * p_owner->__d_max);
	_plan(p_owner);
}

void pifMotionProfile_Sample(PifMotionProfile *p_owner, float time)
{
	const PifMotionProfileSegment *p_segment;
	int i;

	if (time < 0.0f) time = 0.0f;
	if (time >= p_owner->__duration) {
		// The end is the target itself, not the sum of the segments, which rounds.
		p_owner->_position = p_owner->__target;
		p_owner->_velocity = 0.0f;
		p_owner->_acceleration = 0.0f;
		p_owner->_done = TRUE;
		return;
	}

	for (i = p_owner->__segment_count - 1; i > 0 && p_owner->__segments[i].time > time; i--);
	p_segment = &p_owner->__segments[i];
	time -= p_segment->time;
	p_owner->_position = p_segment->position + (p_segment->velocity + 0.5f * p_segment->acceleration * time) * time;
	p_owner->_velocity = p_segment->velocity + p_segment->acceleration * time;
	p_owner->_acceleration = p_segment->acceleration;
	p_owner->_done = FALSE;
}

float pifMotionProfile_GetDuration(PifMotionProfile *p_owner)
{
	return p_owner->__duration;
}

float pifMotionProfile_TimeAtPosition(PifMotionProfile *p_owner, float position)
{
	const PifMotionProfileSegment *p_segment;
	float duration, change, a, v, disc, root, t1, t2, t;
	int i;

	if (p_owner->__segment_count == 0) return position == p_owner->__target ? 0.0f : -1.0f;

	for (i = 0; i < p_owner->__segment_count; i++) {
		p_segment = &p_owner->__segments[i];
		duration = (i + 1 < p_owner->__segment_count ? p_owner->__segments[i + 1].time : p_owner->__duration)
				- p_segment->time;
		change = position - p_segment->position;
		a = p_segment->acceleration;
		v = p_segment->velocity;

		// Smallest t in [0, duration] with v t + a t^2 / 2 = change.
		if (a == 0.0f) {
			if (v == 0.0f) {
				if (change != 0.0f) continue;
				t = 0.0f;
			}
			else t = change / v;
		}
		else {
			disc = v * v + 2.0f * a * change;
			if (disc < 0.0f) {
				// The turning point of a brake, missed by rounding only.
				if (disc < -1e-6f * v * v) continue;
				disc = 0.0f;
			}
			// Each root from the form that does not subtract close values.
			root = v >= 0.0f ? -v - sqrtf(disc) : -v + sqrtf(disc);
			t1 = root / a;
			t2 = root != 0.0f ? -2.0f * change / root : t1;
			if (t1 > t2) { t = t1; t1 = t2; t2 = t; }
			t = t1 >= 0.0f ? t1 : t2;
		}
		if (t < 0.0f) {
			if (t > -1e-6f * duration) t = 0.0f;
			else continue;
		}
		if (t <= duration * (1.0f + 1e-6f)) return p_segment->time + (t < duration ? t : duration);
	}
	return -1.0f;
}
