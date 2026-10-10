// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/pif_solenoid.h"

// The pulse and the delayed commands of every channel are timed by deadlines on the microsecond
// clock, and the one task is triggered at the earliest of them. Deadlines are compared by their
// signed difference to the current time, which stays right across the wrap of the clock for any
// delay below 35 minutes.


static uint32_t _remain(uint32_t end, uint32_t now)
{
	int32_t diff = (int32_t)(end - now);

	return diff > 0 ? (uint32_t)diff : 0UL;
}

static PifSolenoidChannel* _channel(PifSolenoid* p_owner, uint8_t index)
{
	if (index >= p_owner->_count) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}
	return &p_owner->__p_channel[index];
}

#ifdef PIF_COLLECT_SIGNAL

// "SN", the kind and the index in decimal. Not pif_Printf(), which writes a number only where the
// buffer has room for its widest value.
static void _formatName(char* p_name, char kind, uint8_t index)
{
	char digits[3];
	int count = 0, i = 0;

	do {
		digits[count++] = '0' + index % 10;
		index /= 10;
	} while (index);

	p_name[i++] = 'S';
	p_name[i++] = 'N';
	p_name[i++] = kind;
	while (count) p_name[i++] = digits[--count];
	p_name[i] = '\0';
}

// The direction is the one driven, as act_control gets it: SD_INVALID while the channel is OFF.
static void _putSignals(PifSolenoidChannel* p_channel, PifSolenoidDir dir)
{
	pifCollectSignal_Put(&p_channel->__cs[SN_CSF_ACTION_IDX], p_channel->__state);
	pifCollectSignal_Put(&p_channel->__cs[SN_CSF_DIR_IDX], dir);
}

#endif

static void _switchOff(PifSolenoid* p_owner, uint8_t index)
{
	(*p_owner->__act_control)(index, OFF, SD_INVALID);
	p_owner->__p_channel[index].__state = FALSE;
#ifdef PIF_COLLECT_SIGNAL
	_putSignals(&p_owner->__p_channel[index], SD_INVALID);
#endif
}

static void _checkOff(PifSolenoid* p_owner, uint8_t index, uint32_t now)
{
	PifSolenoidChannel* p_channel = &p_owner->__p_channel[index];

	if (!p_channel->__on_pending || _remain(p_channel->__on_end, now)) return;

	p_channel->__on_pending = FALSE;
	if (!p_channel->__state) return;

	_switchOff(p_owner, index);
	if (p_owner->evt_off) (*p_owner->evt_off)(p_owner, index);
}

static void _switchOn(PifSolenoid* p_owner, uint8_t index, uint32_t now, PifSolenoidDir dir)
{
	PifSolenoidChannel* p_channel = &p_owner->__p_channel[index];

	if (p_owner->_type == ST_2POINT && dir == p_channel->__current_dir) return;

	// A pulse that is over but not yet switched off ends first, as its own pulse.
	_checkOff(p_owner, index, now);

	if (p_channel->on_time) {
		p_channel->__on_end = now + p_channel->on_time * 1000UL;
		p_channel->__on_pending = TRUE;
	}
	else {
		p_channel->__on_pending = FALSE;
	}
	p_channel->__current_dir = dir;
	(*p_owner->__act_control)(index, ON, dir);
	p_channel->__state = TRUE;
#ifdef PIF_COLLECT_SIGNAL
	_putSignals(p_channel, dir);
#endif
}

static void _startDelay(PifSolenoidChannel* p_channel, uint32_t end, PifSolenoidDir dir)
{
	p_channel->__dir = dir;
	p_channel->__delay_end = end;
	p_channel->__delay_pending = TRUE;
}

// Wakes the task at the earliest deadline of all channels. A trigger still pending is replaced, so
// the task is never woken later than that.
static void _schedule(PifSolenoid* p_owner)
{
	PifSolenoidChannel* p_channel;
	uint32_t now = (*pif_act_timer1us)();
	uint32_t wait = 0xFFFFFFFFUL;
	uint32_t remain;
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		if (p_channel->__on_pending) {
			remain = _remain(p_channel->__on_end, now);
			if (remain < wait) wait = remain;
		}
		if (p_channel->__delay_pending) {
			remain = _remain(p_channel->__delay_end, now);
			if (remain < wait) wait = remain;
		}
	}
	if (wait == 0xFFFFFFFFUL) return;

	if (!pifTask_SetTrigger(p_owner->_p_task, wait)) {
		if (p_owner->evt_error) (*p_owner->evt_error)(p_owner, 0);
	}
}

static void _processChannel(PifSolenoid* p_owner, uint8_t index, uint32_t now)
{
	PifSolenoidChannel* p_channel = &p_owner->__p_channel[index];
	PifSolenoidContent* p_content;
	uint32_t base;

	// The pulse that ended is switched off before a delayed command starts the next one.
	_checkOff(p_owner, index, now);

	while (p_channel->__delay_pending && !_remain(p_channel->__delay_end, now)) {
		// The queue counts each delay from the deadline before it rather than from when the task
		// got to run, so a late run does not push the commands after it back.
		base = p_channel->__delay_end;
		p_channel->__delay_pending = FALSE;
		_switchOn(p_owner, index, now, p_channel->__dir);

		if (!p_channel->__p_buffer) break;

		// A queued command with no delay left runs now. The first one with a delay waits, and the
		// rest stay queued behind it.
		while ((p_content = pifRingData_Remove(p_channel->__p_buffer)) != NULL) {
			if (p_content->delay) {
				_startDelay(p_channel, base + p_content->delay, p_content->dir);
				break;
			}
			_switchOn(p_owner, index, now, p_content->dir);
		}
	}
}

static uint32_t _doTask(PifTask* p_task)
{
	PifSolenoid* p_owner = (PifSolenoid*)p_task->_p_client;
	uint32_t now = (*pif_act_timer1us)();
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) _processChannel(p_owner, i, now);

	_schedule(p_owner);
	return 0;
}

static uint32_t _calcurateTime(PifSolenoidChannel* p_channel)
{
	PifSolenoidContent* p_content;
	uint32_t time;

	time = _remain(p_channel->__delay_end, (*pif_act_timer1us)());
	p_content = pifRingData_GetFirstData(p_channel->__p_buffer);
	while (p_content) {
		time += p_content->delay;
		p_content = pifRingData_GetNextData(p_channel->__p_buffer);
	}
	return time;
}

BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, uint8_t count, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control)
{
	uint8_t i;

    if (!p_owner || !count || count > PIF_SOLENOID_MAX_COUNT || !act_control) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSolenoid));

	p_owner->__p_channel = calloc(count, sizeof(PifSolenoidChannel));
	if (!p_owner->__p_channel) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
	for (i = 0; i < count; i++) p_owner->__p_channel[i].on_time = on_time;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    // TM_EXTERNAL: it runs only when a deadline is due, and pifTask_SetTrigger() holds it back for
    // the realtime slack the way it does any other task.
    p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, p_owner, FALSE);
    if (!p_owner->_p_task) {
		free(p_owner->__p_channel);
		p_owner->__p_channel = NULL;
		return FALSE;
    }
    p_owner->_p_task->name = "Solenoid";

    p_owner->__act_control = act_control;
    p_owner->_type = type;
    p_owner->_count = count;
    return TRUE;
}

void pifSolenoid_Clear(PifSolenoid* p_owner)
{
	PifSolenoidChannel* p_channel;
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		// Nothing would switch it off once the task is gone.
		if (p_channel->__state) _switchOff(p_owner, i);
		pifRingData_Destroy(&p_channel->__p_buffer);
	}
#ifdef PIF_COLLECT_SIGNAL
	pifSolenoid_ResetCsFlag(p_owner, SN_CSF_ALL_BIT);
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

BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint8_t index, uint16_t size)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	pifRingData_Destroy(&p_channel->__p_buffer);
	p_channel->__p_buffer = pifRingData_Create(PIF_ID_AUTO, sizeof(PifSolenoidContent), size);
	return p_channel->__p_buffer != NULL;
}

BOOL pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner, uint8_t index)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	p_channel->__current_dir = SD_INVALID;
	return TRUE;
}

BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint8_t index, uint16_t on_time)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

    p_channel->on_time = on_time;
    return TRUE;
}

BOOL pifSolenoid_ActionOn(PifSolenoid* p_owner, uint8_t index, uint16_t delay)
{
	return pifSolenoid_ActionOnDir(p_owner, index, delay, SD_INVALID);
}

BOOL pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint8_t index, uint16_t delay, PifSolenoidDir dir)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);
	PifSolenoidContent *pstContent;
	uint32_t delay_us = delay * 1000UL;
	uint32_t time;

	if (!p_channel) return FALSE;

	if (p_owner->_type == ST_2POINT && dir == SD_INVALID) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Also queued while a delay that is over waits for the task, which takes the queue in order.
	if (p_channel->__p_buffer && p_channel->__delay_pending) {
		time = _calcurateTime(p_channel);
		pstContent = pifRingData_Add(p_channel->__p_buffer);
		if (!pstContent) return FALSE;

		pstContent->delay = delay_us > time ? delay_us - time : 0;
		pstContent->dir = dir;
		return TRUE;
	}

	if (delay_us) {
		_startDelay(p_channel, (*pif_act_timer1us)() + delay_us, dir);
	}
	else {
		_switchOn(p_owner, index, (*pif_act_timer1us)(), dir);
	}
	_schedule(p_owner);
	return TRUE;
}

BOOL pifSolenoid_ActionOff(PifSolenoid* p_owner, uint8_t index)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);

	if (!p_channel) return FALSE;

	// Commands still waiting would switch it back on. A trigger still pending finds nothing due
	// on this channel.
	p_channel->__delay_pending = FALSE;
	if (p_channel->__p_buffer) pifRingData_Reset(p_channel->__p_buffer);
	p_channel->__on_pending = FALSE;

	if (p_channel->__state) _switchOff(p_owner, index);
	return TRUE;
}

BOOL pifSolenoid_IsOn(PifSolenoid* p_owner, uint8_t index)
{
	PifSolenoidChannel* p_channel = _channel(p_owner, index);

	return p_channel ? p_channel->__state : FALSE;
}


#ifdef PIF_COLLECT_SIGNAL

BOOL pifSolenoid_SetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag)
{
	PifSolenoidChannel* p_channel;
	char name[PIF_COLLECT_SIGNAL_NAME_SIZE];
	uint8_t i;

	// The channel copies the name, so one buffer serves them all.
	for (i = 0; i < p_owner->_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		if (flag & SN_CSF_ACTION_BIT) {
			_formatName(name, 'A', i);
			if (!pifCollectSignal_AddChannel(&p_channel->__cs[SN_CSF_ACTION_IDX], name, p_owner->_id, CSVT_WIRE, 1,
					p_channel->__state)) goto fail;
		}
		if (flag & SN_CSF_DIR_BIT) {
			_formatName(name, 'D', i);
			if (!pifCollectSignal_AddChannel(&p_channel->__cs[SN_CSF_DIR_IDX], name, p_owner->_id, CSVT_WIRE, 2,
					p_channel->__state ? p_channel->__current_dir : SD_INVALID)) goto fail;
		}
	}
	return TRUE;

fail:
	// Leaves none of the requested signals half added.
	pifSolenoid_ResetCsFlag(p_owner, flag);
	return FALSE;
}

void pifSolenoid_ResetCsFlag(PifSolenoid* p_owner, PifSolenoidCsFlag flag)
{
	PifSolenoidChannel* p_channel;
	uint8_t i;

	for (i = 0; i < p_owner->_count; i++) {
		p_channel = &p_owner->__p_channel[i];
		if (flag & SN_CSF_ACTION_BIT) pifCollectSignal_RemoveChannel(&p_channel->__cs[SN_CSF_ACTION_IDX]);
		if (flag & SN_CSF_DIR_BIT) pifCollectSignal_RemoveChannel(&p_channel->__cs[SN_CSF_DIR_IDX]);
	}
}

#endif	// PIF_COLLECT_SIGNAL
