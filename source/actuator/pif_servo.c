// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/pif_servo.h"


// A move is a function of the time since it started: each update works out the share of its time that has passed,
// in 1/1024 steps, and puts the channel at that share of the way. A late update therefore does not slow a move
// down, it only skips a step. The start is kept on the microsecond clock and compared by difference, which stays
// right across the wrap of the clock for any move below 71 minutes.

#define PROGRESS_SHIFT		10
#define PROGRESS_FULL		(1UL << PROGRESS_SHIFT)


static void _lock(PifServo *p_owner, BOOL lock)
{
	if (p_owner->__act_lock) (*p_owner->__act_lock)(p_owner, lock);
}

static PifServoChannel *_channel(PifServo *p_owner, uint8_t index)
{
	if (index >= p_owner->_count) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}
	return &p_owner->__p_channel[index];
}

#ifdef COLLECT_SIGNAL_SERVO

// "SVP" and the index in decimal. Not pif_Printf(), which writes a number only where the buffer has room for its
// widest value.
static void _formatName(char *p_name, uint8_t index)
{
	char digits[3];
	int count = 0, i = 0;

	do {
		digits[count++] = '0' + index % 10;
		index /= 10;
	} while (index);

	p_name[i++] = 'S';
	p_name[i++] = 'V';
	p_name[i++] = 'P';
	while (count) p_name[i++] = digits[--count];
	p_name[i] = '\0';
}

#endif

static int16_t _clampAngle(PifServoChannel *p_channel, int16_t angle)
{
	if (angle < p_channel->_min_angle) return p_channel->_min_angle;
	if (angle > p_channel->_max_angle) return p_channel->_max_angle;
	return angle;
}

// The angle is within the range, so the offset from the end pulse is too.
static uint16_t _angleToPulse(PifServoChannel *p_channel, int16_t angle)
{
	int32_t span_angle = (int32_t)p_channel->_max_angle - p_channel->_min_angle;
	int32_t span_pulse = (int32_t)p_channel->_max_pulse - p_channel->_min_pulse;
	int32_t offset = ((int32_t)angle - p_channel->_min_angle) * span_pulse;
	int32_t pulse;

	offset = (offset + span_angle / 2) / span_angle;
	pulse = p_channel->_reverse ? p_channel->_max_pulse - offset : p_channel->_min_pulse + offset;
	pulse += p_channel->_trim;
	return pulse > 0xFFFF ? 0xFFFF : (uint16_t)pulse;
}

static int16_t _pulseToAngle(PifServoChannel *p_channel, uint16_t pulse_us)
{
	int32_t span_angle = (int32_t)p_channel->_max_angle - p_channel->_min_angle;
	int32_t span_pulse = (int32_t)p_channel->_max_pulse - p_channel->_min_pulse;
	int32_t pulse = (int32_t)pulse_us - p_channel->_trim;
	int32_t offset;

	if (pulse < p_channel->_min_pulse) pulse = p_channel->_min_pulse;
	else if (pulse > p_channel->_max_pulse) pulse = p_channel->_max_pulse;
	offset = p_channel->_reverse ? p_channel->_max_pulse - pulse : pulse - p_channel->_min_pulse;
	return (int16_t)(p_channel->_min_angle + (offset * span_angle + span_pulse / 2) / span_pulse);
}

// act_control is called only for a pulse that changes, so an update of a channel that keeps still costs nothing.
static void _output(PifServo *p_owner, uint8_t index, uint16_t pulse_us)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];

	if (pulse_us == p_channel->_pulse) return;

	(*p_owner->__act_control)(index, pulse_us);
	p_channel->_pulse = pulse_us;
#ifdef COLLECT_SIGNAL_SERVO
	pifCollectSignal_Put(&p_channel->__cs[SV_CSF_PULSE_IDX], pulse_us);
#endif
}

static void _setAngle(PifServo *p_owner, uint8_t index, int16_t angle)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];

	p_channel->_angle = angle;
	_output(p_owner, index, _angleToPulse(p_channel, angle));
}

static uint32_t _progress(PifServoChannel *p_channel, uint32_t now)
{
	uint32_t elapsed = (now - p_channel->__move_start) / 1000;
	uint32_t t;

	if (elapsed >= p_channel->__move_time) return PROGRESS_FULL;

	t = (elapsed << PROGRESS_SHIFT) / p_channel->__move_time;
	if (p_channel->__profile == SVP_EASE) {
		// 3t^2 - 2t^3 in 1/1024 steps. At most 1024 * 1024 * 3072, within 32 bits.
		t = (t * t * (3 * PROGRESS_FULL - 2 * t)) >> (2 * PROGRESS_SHIFT);
	}
	return t;
}

// Puts a moving channel where its move is now. Returns TRUE when that is the target, and the move is over.
static BOOL _update(PifServo *p_owner, uint8_t index, uint32_t now)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];
	uint32_t t = _progress(p_channel, now);
	int32_t distance = (int32_t)p_channel->__target_angle - p_channel->__start_angle;

	if (t >= PROGRESS_FULL) {
		_setAngle(p_owner, index, p_channel->__target_angle);
		p_channel->__moving = FALSE;
		return TRUE;
	}
	_setAngle(p_owner, index, (int16_t)(p_channel->__start_angle + distance * (int32_t)t / (int32_t)PROGRESS_FULL));
	return FALSE;
}

// Where a channel is now, a move under way included.
static int16_t _currentAngle(PifServo *p_owner, uint8_t index, uint32_t now)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];

	if (p_channel->__moving) _update(p_owner, index, now);
	return p_channel->_angle;
}

static void _stopMove(PifServo *p_owner, uint8_t index)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];

	if (!p_channel->__moving) return;

	_update(p_owner, index, (*pif_act_timer1us)());
	p_channel->__moving = FALSE;
}

static void _startMove(PifServo *p_owner, uint8_t index, int16_t angle, uint16_t time_ms, uint32_t now)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];

	angle = _clampAngle(p_channel, angle);
	if (!p_channel->_pulse) {
		_setAngle(p_owner, index, angle);
		time_ms = 0;
	}
	else {
		_currentAngle(p_owner, index, now);
	}
	p_channel->__start_angle = p_channel->_angle;
	p_channel->__target_angle = angle;
	p_channel->__move_start = now;
	p_channel->__move_time = time_ms;
	p_channel->__profile = p_channel->_profile;
	p_channel->__moving = TRUE;
}

// Runs the task at its next visit, which starts the chain of updates. One still pending is brought forward.
static void _wake(PifServo *p_owner)
{
	pifTask_SetTrigger(p_owner->_p_task, 0);
}

static uint32_t _doTask(PifTask *p_task)
{
	PifServo *p_owner = (PifServo *)p_task->_p_client;
	BOOL moving = FALSE;
	uint32_t now;
	uint8_t i;

	_lock(p_owner, TRUE);
	now = (*pif_act_timer1us)();
	for (i = 0; i < p_owner->_count; i++) {
		if (!p_owner->__p_channel[i].__moving) continue;
		if (_update(p_owner, i, now) && p_owner->evt_arrive) (*p_owner->evt_arrive)(p_owner, i);
	}
	// After the events, which may have started moves on channels already passed.
	for (i = 0; i < p_owner->_count; i++) {
		if (p_owner->__p_channel[i].__moving) moving = TRUE;
	}
	_lock(p_owner, FALSE);
	return moving ? p_owner->_period : 0;
}

BOOL pifServo_Init(PifServo *p_owner, PifId id, uint8_t count, uint16_t period_ms, PifActServoControl act_control)
{
	PifServoChannel *p_channel;
	uint8_t i;

	if (!p_owner || !count || count > PIF_SERVO_MAX_COUNT || !period_ms || !act_control) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifServo));

	p_owner->__p_channel = calloc(count, sizeof(PifServoChannel));
	if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
	for (i = 0; i < count; i++) {
		p_channel = &p_owner->__p_channel[i];
		p_channel->_min_pulse = PIF_SERVO_DEFAULT_MIN_PULSE;
		p_channel->_max_pulse = PIF_SERVO_DEFAULT_MAX_PULSE;
		p_channel->_min_angle = PIF_SERVO_DEFAULT_MIN_ANGLE;
		p_channel->_max_angle = PIF_SERVO_DEFAULT_MAX_ANGLE;
		p_channel->_angle = PIF_SERVO_DEFAULT_MIN_ANGLE;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;

	// TM_EXTERNAL: it sleeps while nothing moves, and asks for the next update by its return value while
	// something does.
	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, p_owner, FALSE);
	if (!p_owner->_p_task) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
		return FALSE;
	}

	p_owner->__act_control = act_control;
	p_owner->_count = count;
	p_owner->_period = period_ms * 1000UL;
	return TRUE;
}

void pifServo_Clear(PifServo *p_owner)
{
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) {
		p_owner->__p_channel[i].__moving = FALSE;
		_output(p_owner, i, 0);
	}
#ifdef COLLECT_SIGNAL_SERVO
	pifServo_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, SV_CSF_ALL_BIT);
#endif
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->__p_channel) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
	}
	p_owner->_count = 0;
}

BOOL pifServo_SetRange(PifServo *p_owner, uint8_t index, uint16_t min_pulse, uint16_t max_pulse, int16_t min_angle,
		int16_t max_angle)
{
	PifServoChannel *p_channel = _channel(p_owner, index);
	int32_t trim;

	if (!p_channel) return FALSE;

	trim = p_channel->_trim;
	if (min_pulse <= (trim < 0 ? -trim : trim) || min_pulse >= max_pulse || min_angle == PIF_SERVO_KEEP ||
			min_angle >= max_angle) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_lock(p_owner, TRUE);
	_stopMove(p_owner, index);
	p_channel->_min_pulse = min_pulse;
	p_channel->_max_pulse = max_pulse;
	p_channel->_min_angle = min_angle;
	p_channel->_max_angle = max_angle;
	if (p_channel->_pulse) _setAngle(p_owner, index, _clampAngle(p_channel, p_channel->_angle));
	else p_channel->_angle = _clampAngle(p_channel, p_channel->_angle);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_SetTrim(PifServo *p_owner, uint8_t index, int16_t trim)
{
	PifServoChannel *p_channel = _channel(p_owner, index);
	int32_t size = trim < 0 ? -(int32_t)trim : trim;

	if (!p_channel) return FALSE;

	if (size >= p_channel->_min_pulse) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_lock(p_owner, TRUE);
	p_channel->_trim = trim;
	if (p_channel->_pulse) _setAngle(p_owner, index, p_channel->_angle);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_SetReverse(PifServo *p_owner, uint8_t index, BOOL reverse)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	_lock(p_owner, TRUE);
	p_channel->_reverse = reverse;
	if (p_channel->_pulse) _setAngle(p_owner, index, p_channel->_angle);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_SetProfile(PifServo *p_owner, uint8_t index, PifServoProfile profile)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	if (profile != SVP_LINEAR && profile != SVP_EASE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// A move under way keeps the profile it started with, which a change halfway would make jump.
	p_channel->_profile = profile;
	return TRUE;
}

BOOL pifServo_SetAngle(PifServo *p_owner, uint8_t index, int16_t angle)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	_lock(p_owner, TRUE);
	p_channel->__moving = FALSE;
	_setAngle(p_owner, index, _clampAngle(p_channel, angle));
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_SetPulse(PifServo *p_owner, uint8_t index, uint16_t pulse_us)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	if (!pulse_us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_lock(p_owner, TRUE);
	p_channel->__moving = FALSE;
	p_channel->_angle = _pulseToAngle(p_channel, pulse_us);
	_output(p_owner, index, pulse_us);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_MoveTo(PifServo *p_owner, uint8_t index, int16_t angle, uint16_t time_ms)
{
	if (!_channel(p_owner, index)) return FALSE;

	_lock(p_owner, TRUE);
	_startMove(p_owner, index, angle, time_ms, (*pif_act_timer1us)());
	_wake(p_owner);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_MoveAtSpeed(PifServo *p_owner, uint8_t index, int16_t angle, uint16_t speed)
{
	PifServoChannel *p_channel = _channel(p_owner, index);
	uint32_t now, time_ms;
	int32_t distance;
	BOOL result = FALSE;

	if (!p_channel) return FALSE;

	if (!speed) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_lock(p_owner, TRUE);
	now = (*pif_act_timer1us)();
	angle = _clampAngle(p_channel, angle);
	// A channel that is detached jumps to the target, so it has no distance to go.
	distance = p_channel->_pulse ? (int32_t)angle - _currentAngle(p_owner, index, now) : 0;
	if (distance < 0) distance = -distance;
	// The distance is in 0.1 degrees and the speed in degrees per second. Rounded up, so that the speed is never
	// passed.
	time_ms = ((uint32_t)distance * 100UL + speed - 1) / speed;
	if (time_ms <= 0xFFFF) {
		_startMove(p_owner, index, angle, (uint16_t)time_ms, now);
		_wake(p_owner);
		result = TRUE;
	}
	else {
		pif_error = E_INVALID_PARAM;
	}
	_lock(p_owner, FALSE);
	return result;
}

BOOL pifServo_MoveSync(PifServo *p_owner, const int16_t *p_angles, uint16_t time_ms)
{
	uint32_t now;
	uint8_t i;

	if (!p_angles) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	_lock(p_owner, TRUE);
	// One start for all of them, so that they arrive together.
	now = (*pif_act_timer1us)();
	for (i = 0; i < p_owner->_count; i++) {
		if (p_angles[i] != PIF_SERVO_KEEP) _startMove(p_owner, i, p_angles[i], time_ms, now);
	}
	_wake(p_owner);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_Stop(PifServo *p_owner, uint8_t index)
{
	if (!_channel(p_owner, index)) return FALSE;

	// A trigger still pending finds nothing to move on this channel.
	_lock(p_owner, TRUE);
	_stopMove(p_owner, index);
	_lock(p_owner, FALSE);
	return TRUE;
}

BOOL pifServo_Detach(PifServo *p_owner, uint8_t index)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	_lock(p_owner, TRUE);
	p_channel->__moving = FALSE;
	_output(p_owner, index, 0);
	_lock(p_owner, FALSE);
	return TRUE;
}

void pifServo_AttachActLock(PifServo *p_owner, PifActServoLock act_lock)
{
	p_owner->__act_lock = act_lock;
}

const PifServoChannel *pifServo_GetChannel(PifServo *p_owner, uint8_t index)
{
	return _channel(p_owner, index);
}

int16_t pifServo_GetAngle(PifServo *p_owner, uint8_t index)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	return p_channel ? p_channel->_angle : 0;
}

BOOL pifServo_IsMoving(PifServo *p_owner, uint8_t index)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	return p_channel ? p_channel->__moving : FALSE;
}

BOOL pifServo_IsAttached(PifServo *p_owner, uint8_t index)
{
	PifServoChannel *p_channel = _channel(p_owner, index);

	return p_channel ? p_channel->_pulse != 0 : FALSE;
}


#ifdef COLLECT_SIGNAL_SERVO

static BOOL _setCsFlag(PifServo *p_owner, uint8_t index, PifServoCsFlag flag)
{
	PifServoChannel *p_channel = &p_owner->__p_channel[index];
	char name[PIF_COLLECT_SIGNAL_NAME_SIZE];

	if (flag & SV_CSF_PULSE_BIT) {
		_formatName(name, index);
		if (!pifCollectSignal_AddChannel(&p_channel->__cs[SV_CSF_PULSE_IDX], name, p_owner->_id, CSVT_REG, 16,
				p_channel->_pulse)) return FALSE;
	}
	return TRUE;
}

BOOL pifServo_SetCsFlag(PifServo *p_owner, uint8_t index, PifServoCsFlag flag)
{
	uint8_t i;

	if (index < p_owner->_count) {
		if (_setCsFlag(p_owner, index, flag)) return TRUE;
	}
	else {
		for (i = 0; i < p_owner->_count; i++) {
			if (!_setCsFlag(p_owner, i, flag)) break;
		}
		if (i >= p_owner->_count) return TRUE;
	}

	// Leaves none of the requested signals half added.
	pifServo_ResetCsFlag(p_owner, index, flag);
	return FALSE;
}

void pifServo_ResetCsFlag(PifServo *p_owner, uint8_t index, PifServoCsFlag flag)
{
	PifServoChannel *p_channel;
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) {
		if (index < p_owner->_count && i != index) continue;
		p_channel = &p_owner->__p_channel[i];
		if (flag & SV_CSF_PULSE_BIT) pifCollectSignal_RemoveChannel(&p_channel->__cs[SV_CSF_PULSE_IDX]);
	}
}

#endif	// COLLECT_SIGNAL_SERVO
