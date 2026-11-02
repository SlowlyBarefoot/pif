// SPDX-License-Identifier: BSD-3-Clause
#include "input/pif_joystick.h"


/**
 * @brief Turns a raw value of one axis into a position.
 * @param p_owner Pointer to the joystick instance.
 * @param axis Axis the value belongs to.
 * @param raw Raw value, after the filter.
 * @return Position, -PIF_JOYSTICK_RANGE to +PIF_JOYSTICK_RANGE.
 */
static int16_t _toPosition(PifJoystick *p_owner, PifJoystickAxis axis, int16_t raw)
{
	PifJoystickAxisCal *p_cal = &p_owner->_cal[axis];
	int32_t value, dead = p_owner->_dead_zone;

	// Each side of the centre has a scale of its own, since the centre is rarely half-way.
	if (raw >= p_cal->_center) {
		value = (int32_t)(raw - p_cal->_center) * PIF_JOYSTICK_RANGE / (p_cal->_max - p_cal->_center);
	}
	else {
		value = (int32_t)(raw - p_cal->_center) * PIF_JOYSTICK_RANGE / (p_cal->_center - p_cal->_min);
	}
	if (value > PIF_JOYSTICK_RANGE) value = PIF_JOYSTICK_RANGE;
	else if (value < -PIF_JOYSTICK_RANGE) value = -PIF_JOYSTICK_RANGE;

	// What is left outside the dead zone is stretched over the whole range, so the position starts from
	// 0 at its edge instead of jumping to it.
	if (value > dead) value = (value - dead) * PIF_JOYSTICK_RANGE / (PIF_JOYSTICK_RANGE - dead);
	else if (value < -dead) value = (value + dead) * PIF_JOYSTICK_RANGE / (PIF_JOYSTICK_RANGE - dead);
	else value = 0;

	return p_cal->_invert ? -value : value;
}

/**
 * @brief Works out where one axis points, with hysteresis against the way it pointed before.
 * @param position Position of the axis.
 * @param prev -1, 0 or +1: where it pointed before.
 * @return -1, 0 or +1.
 */
static int8_t _axisDirection(int16_t position, int8_t prev)
{
	if (prev > 0 && position >= PIF_JOYSTICK_DIRECTION_OFF) return 1;
	if (prev < 0 && position <= -PIF_JOYSTICK_DIRECTION_OFF) return -1;
	if (position >= PIF_JOYSTICK_DIRECTION_ON) return 1;
	if (position <= -PIF_JOYSTICK_DIRECTION_ON) return -1;
	return 0;
}

/**
 * @brief Periodic joystick task.
 * @param p_task Task context that contains a `PifJoystick` client pointer.
 * @return Always returns `0` to keep the periodic task active.
 */
static uint32_t _doTask(PifTask *p_task)
{
	pifJoystick_Process(p_task->_p_client);
	return 0;
}

BOOL pifJoystick_Init(PifJoystick *p_owner, PifId id, int16_t raw_min, int16_t raw_max, PifActJoystickRead act_read)
{
	int i;

	// The centre has to lie strictly between the ends, which needs a span of at least two.
	if (!p_owner || (int32_t)raw_max - raw_min < 2) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifJoystick));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_control_period_1ms = PIF_JOYSTICK_CONTROL_PERIOD;
	for (i = 0; i < JA_COUNT; i++) {
		p_owner->_cal[i]._min = raw_min;
		p_owner->_cal[i]._center = ((int32_t)raw_min + raw_max) / 2;
		p_owner->_cal[i]._max = raw_max;
	}
	p_owner->__act_read = act_read;
	return TRUE;
}

void pifJoystick_Clear(PifJoystick *p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifJoystick_SetAxis(PifJoystick *p_owner, PifJoystickAxis axis, int16_t min, int16_t center, int16_t max)
{
	// _toPosition() divides by both sides of the centre.
	if (!p_owner || axis >= JA_COUNT || center <= min || center >= max) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_cal[axis]._min = min;
	p_owner->_cal[axis]._center = center;
	p_owner->_cal[axis]._max = max;
	return TRUE;
}

BOOL pifJoystick_SetInvert(PifJoystick *p_owner, PifJoystickAxis axis, BOOL invert)
{
	if (!p_owner || axis >= JA_COUNT) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_cal[axis]._invert = invert;
	return TRUE;
}

BOOL pifJoystick_SetDeadZone(PifJoystick *p_owner, uint16_t dead_zone)
{
	if (!p_owner || dead_zone >= PIF_JOYSTICK_RANGE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_dead_zone = dead_zone;
	return TRUE;
}

BOOL pifJoystick_CalibrateCenter(PifJoystick *p_owner)
{
	int16_t raw[JA_COUNT];
	int i;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_owner->__act_read) {
		(*p_owner->__act_read)(p_owner, &raw[JA_X], &raw[JA_Y]);
	}
	else if (p_owner->__sampled) {
		raw[JA_X] = p_owner->_raw[JA_X];
		raw[JA_Y] = p_owner->_raw[JA_Y];
	}
	else {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	for (i = 0; i < JA_COUNT; i++) {
		if (raw[i] <= p_owner->_cal[i]._min || raw[i] >= p_owner->_cal[i]._max) {
			pif_error = E_INVALID_STATE;
			return FALSE;
		}
	}
	for (i = 0; i < JA_COUNT; i++) p_owner->_cal[i]._center = raw[i];
	return TRUE;
}

BOOL pifJoystick_AttachFilter(PifJoystick *p_owner, PifNoiseFilter *p_filter_x, PifNoiseFilter *p_filter_y)
{
	if (!p_owner || !p_filter_x || !p_filter_y || p_filter_x->_value != NFV_INT16 || p_filter_y->_value != NFV_INT16) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_filter[JA_X] = p_filter_x;
	p_owner->__p_filter[JA_Y] = p_filter_y;
	return TRUE;
}

void pifJoystick_sigData(PifJoystick *p_owner, int16_t x, int16_t y)
{
	int16_t raw[JA_COUNT] = { x, y };
	int i;

	if (!p_owner) return;

	if (p_owner->__p_filter[JA_X] && p_owner->__p_filter[JA_Y]) {
		for (i = 0; i < JA_COUNT; i++) {
			raw[i] = *(int16_t*)pifNoiseFilter_Process(p_owner->__p_filter[i], &raw[i]);
		}
		if (!p_owner->__p_filter[JA_X]->_ready || !p_owner->__p_filter[JA_Y]->_ready) return;
	}

	p_owner->_raw[JA_X] = raw[JA_X];
	p_owner->_raw[JA_Y] = raw[JA_Y];
	p_owner->__sampled = TRUE;
	p_owner->__updated = TRUE;
}

void pifJoystick_Process(PifJoystick *p_owner)
{
	int16_t raw[JA_COUNT], position[JA_COUNT];
	int8_t dir_x, dir_y;
	uint8_t direction;
	int i;

	if (!p_owner) return;

	if (p_owner->__act_read) {
		(*p_owner->__act_read)(p_owner, &raw[JA_X], &raw[JA_Y]);
		pifJoystick_sigData(p_owner, raw[JA_X], raw[JA_Y]);
	}
	if (!p_owner->__updated) return;

	// Cleared before _raw is copied, so a reading that pifJoystick_sigData() gives from an interrupt
	// meanwhile is used by the next call rather than lost.
	p_owner->__updated = FALSE;
	raw[JA_X] = p_owner->_raw[JA_X];
	raw[JA_Y] = p_owner->_raw[JA_Y];

	for (i = 0; i < JA_COUNT; i++) {
		position[i] = _toPosition(p_owner, (PifJoystickAxis)i, raw[i]);
	}

	if (position[JA_X] != p_owner->_position[JA_X] || position[JA_Y] != p_owner->_position[JA_Y]) {
		p_owner->_position[JA_X] = position[JA_X];
		p_owner->_position[JA_Y] = position[JA_Y];
		if (p_owner->evt_move) (*p_owner->evt_move)(p_owner, position[JA_X], position[JA_Y]);
	}

	dir_x = _axisDirection(position[JA_X], (p_owner->_direction & JD_RIGHT) ? 1 : (p_owner->_direction & JD_LEFT) ? -1 : 0);
	dir_y = _axisDirection(position[JA_Y], (p_owner->_direction & JD_UP) ? 1 : (p_owner->_direction & JD_DOWN) ? -1 : 0);
	direction = (dir_x > 0 ? JD_RIGHT : dir_x < 0 ? JD_LEFT : JD_NONE) | (dir_y > 0 ? JD_UP : dir_y < 0 ? JD_DOWN : JD_NONE);
	if (direction != p_owner->_direction) {
		p_owner->_direction = direction;
		if (p_owner->evt_direction) (*p_owner->evt_direction)(p_owner, direction);
	}
}

BOOL pifJoystick_SetControlPeriod(PifJoystick *p_owner, uint16_t period1ms)
{
	if (!p_owner || !period1ms) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_control_period_1ms = period1ms;
	if (p_owner->_p_task) {
		pifTask_ChangePeriod(p_owner->_p_task, p_owner->_control_period_1ms * 1000UL);
	}
	return TRUE;
}

BOOL pifJoystick_Start(PifJoystick *p_owner, const char *p_name)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_owner->_p_task) pifTask_Resume(p_owner->_p_task);
	else {
		p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, p_owner->_control_period_1ms * 1000UL, _doTask, p_owner, TRUE);
		if (!p_owner->_p_task) return FALSE;
		p_owner->_p_task->name = p_name ? p_name : "Joystick";
	}
	return TRUE;
}

void pifJoystick_Stop(PifJoystick *p_owner)
{
	if (p_owner && p_owner->_p_task) pifTask_Pause(p_owner->_p_task);
}
