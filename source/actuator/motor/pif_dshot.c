// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/motor/pif_dshot.h"


// Pauses a command sequence keeps, in microseconds. An ESC takes a command only once it has seen the
// motor stopped for a while, wants a gap between two frames of the same command, and is busy for a
// good while after a beacon or a request for its information, during which another command is lost.
#define START_DELAY_US			10000UL
#define REPEAT_DELAY_US			1000UL
#define ESC_INFO_DELAY_US		12000UL
#define BEACON_DELAY_US			100000UL

// Frames of a command an ESC stores, which it only acts on once it has seen the same one this often.
#define STORED_COMMAND_FRAMES	10

#define TELEMETRY_BITS			21	// The start bit and the four symbols of the frame
#define TELEMETRY_SYMBOLS		4

// The longest a telemetry line holds its level: a 1 and the two 0s that may follow it at most.
#define TELEMETRY_MAX_RUN		3

// The ESC answers on a clock of its own, a few percent off ours, so the 21 bits come out a little
// longer or shorter. Runs are taken to the nearest bit, and an answer is looked for within this much
// of its length.
#define TELEMETRY_SLACK			2

#define BAD_SYMBOL				0xFF


/*
 * Nibble of each 5-bit symbol, which is the table of the protocol read the other way round:
 *
 *     0:11001  1:11011  2:10010  3:10011  4:11101  5:10101  6:10110  7:10111
 *     8:11010  9:01001  A:01010  B:01011  C:11110  D:01101  E:01110  F:01111
 *
 * A symbol leaves no more than two 0s in a row in the stream, its ends included, since a 0 is a bit
 * period in which the line does not change and the receiver needs a change often enough to hold on
 * to the bit grid. Exactly 17 of the 32 values do that, and the protocol uses all but 0x1F of them.
 * The other 15 cannot come out of an ESC, so they are marked here and let a frame fail rather than
 * decode into something that looks sound.
 */
static const uint8_t c_symbol_nibble[32] = {
	BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL, BAD_SYMBOL,
	BAD_SYMBOL, 0x09, 0x0A, 0x0B, BAD_SYMBOL, 0x0D, 0x0E, 0x0F,
	BAD_SYMBOL, BAD_SYMBOL, 0x02, 0x03, BAD_SYMBOL, 0x05, 0x06, 0x07,
	BAD_SYMBOL, 0x00, 0x08, 0x01, BAD_SYMBOL, 0x04, 0x0C, BAD_SYMBOL
};


// Adds one run of an answer to the bits gathered so far: the change of level that opens the run is a
// 1, and every further bit period of it a 0.
static void _addRun(uint32_t *p_gcr, uint8_t *p_bits, uint8_t run)
{
	*p_gcr = (*p_gcr << run) | (1UL << (run - 1));
	*p_bits += run;
}

uint16_t pifDshot_MakeFrame(uint16_t value, BOOL telemetry, BOOL bidirectional)
{
	uint16_t data = ((value & 0x07FF) << 1) | (telemetry ? 1 : 0);
	uint16_t crc = (data ^ (data >> 4) ^ (data >> 8)) & 0x0F;

	if (bidirectional) crc ^= 0x0F;
	return (data << 4) | crc;
}

uint8_t pifDshot_LoadBuffer(uint32_t *p_buffer, uint8_t stride, uint16_t frame, uint32_t bit0, uint32_t bit1)
{
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * stride] = (frame & (0x8000 >> i)) ? bit1 : bit0;
	}

	// Compare 0 keeps the line at rest, which marks the end of the frame for the ESC.
	p_buffer[16 * stride] = 0;
	p_buffer[17 * stride] = 0;
	return PIF_DSHOT_BUFFER_SIZE;
}

uint8_t pifDshot_LoadBufferProshot(uint32_t *p_buffer, uint8_t stride, uint16_t frame, uint32_t base, uint32_t width)
{
	uint8_t i;

	for (i = 0; i < 4; i++) {
		p_buffer[i * stride] = base + ((frame >> (12 - i * 4)) & 0x0F) * width;
	}
	p_buffer[4 * stride] = 0;
	p_buffer[5 * stride] = 0;
	return PIF_PROSHOT_BUFFER_SIZE;
}

void pifDshot_InitBitbangBuffer(uint32_t *p_buffer, uint32_t set_mask, uint32_t reset_mask, BOOL inverted)
{
	uint32_t active = inverted ? reset_mask : set_mask;
	uint32_t rest = inverted ? set_mask : reset_mask;
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * 3] |= active;		// Every bit opens active
		p_buffer[i * 3 + 2] |= rest;	// and is over by its third slot
	}

	// The middle slot is the frame and is left to pifDshot_LoadBitbangBuffer(). The three slots of
	// the hold bit leave the pins where the last bit left them, at rest, so they stay empty: an
	// entry of 0 writes nothing into the port.
}

void pifDshot_ClearBitbangBuffer(uint32_t *p_buffer)
{
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * 3 + 1] = 0;
	}
}

void pifDshot_LoadBitbangBuffer(uint32_t *p_buffer, uint32_t set_mask, uint32_t reset_mask, uint16_t frame, BOOL inverted)
{
	uint32_t rest = inverted ? set_mask : reset_mask;
	uint8_t i;

	// A 1 stays active into the third slot, where every pin goes back to rest anyway, so a 1 has
	// nothing to do here and only a 0 ends its bit early.
	for (i = 0; i < 16; i++) {
		if (!(frame & (0x8000 >> i))) p_buffer[i * 3 + 1] |= rest;
	}
}

uint32_t pifDshot_EdgesToGcr(const uint32_t *p_edges, uint8_t count, uint32_t bit_ticks)
{
	uint32_t gcr = 0;
	uint32_t run;
	uint8_t bits = 0;
	uint8_t i;

	if (!p_edges || !count || !bit_ticks) return PIF_DSHOT_GCR_NONE;

	for (i = 1; i < count && bits < TELEMETRY_BITS; i++) {
		// The time to the next edge is the run, taken to the nearest bit.
		run = (p_edges[i] - p_edges[i - 1] + bit_ticks / 2) / bit_ticks;
		if (!run || run > TELEMETRY_MAX_RUN || bits + run > TELEMETRY_BITS) return PIF_DSHOT_GCR_INVALID;
		_addRun(&gcr, &bits, run);
	}

	if (bits < TELEMETRY_BITS) {
		// Nothing marks the end of the last run: the line simply stays where its bit left it. That
		// run is therefore whatever is left of the 21 bits, and more than a run can hold means the
		// answer broke off in the middle.
		run = TELEMETRY_BITS - bits;
		if (run > TELEMETRY_MAX_RUN) return PIF_DSHOT_GCR_INVALID;
		_addRun(&gcr, &bits, run);
	}
	return gcr;
}

uint32_t pifDshot_SamplesToGcr(const uint16_t *p_samples, uint16_t count, uint16_t mask, uint8_t oversample)
{
	uint32_t gcr = 0;
	uint16_t start, end, i;
	uint16_t level;
	uint8_t bits = 0;
	uint8_t run;

	if (!p_samples || !oversample) return PIF_DSHOT_GCR_NONE;

	// Fewer samples than a whole answer takes cannot hold one.
	if (count < (TELEMETRY_BITS - TELEMETRY_SLACK) * oversample) return PIF_DSHOT_GCR_NONE;
	end = count - (TELEMETRY_BITS - TELEMETRY_SLACK) * oversample;

	// The line rests high, and the answer opens by pulling it low. An ESC too busy to answer leaves
	// it high, which is no fault of an answer.
	for (start = 0; start < end; start++) {
		if (!(p_samples[start] & mask)) break;
	}
	if (start >= end) return PIF_DSHOT_GCR_NONE;

	// Past the end of the answer only whatever the line did afterwards is left to read.
	end = start + (TELEMETRY_BITS + TELEMETRY_SLACK) * oversample;
	if (end > count) end = count;

	level = 0;
	for (i = start + 1; i < end; i++) {
		if ((p_samples[i] & mask) == level) continue;

		// Taken to the nearest bit, and a run that came out shorter than half a bit still counts as
		// one: the level changed, and a change of level is a bit.
		run = (i - start + oversample / 2) / oversample;
		if (!run) run = 1;
		if (run > TELEMETRY_MAX_RUN || bits + run > TELEMETRY_BITS) return PIF_DSHOT_GCR_INVALID;
		_addRun(&gcr, &bits, run);
		start = i;
		level = p_samples[i] & mask;
	}

	if (bits < TELEMETRY_BITS) {
		// As in pifDshot_EdgesToGcr(), the last run is whatever is left of the 21 bits. Where that is
		// more than a run can hold, the pin went low and came back without an answer behind it,
		// which is a line nobody drove rather than an answer that came out wrong.
		run = TELEMETRY_BITS - bits;
		if (run > TELEMETRY_MAX_RUN) return PIF_DSHOT_GCR_NONE;
		_addRun(&gcr, &bits, run);
	}
	return gcr;
}

BOOL pifDshot_DecodeGcr(uint32_t gcr, uint16_t *p_value)
{
	uint16_t word = 0;
	uint8_t nibble;
	uint8_t i;

	if (gcr == PIF_DSHOT_GCR_NONE || gcr == PIF_DSHOT_GCR_INVALID) return FALSE;

	// The start bit, the top one of the 21, carries nothing.
	for (i = TELEMETRY_SYMBOLS; i > 0; i--) {
		nibble = c_symbol_nibble[(gcr >> ((i - 1) * 5)) & 0x1F];
		if (nibble == BAD_SYMBOL) return FALSE;
		word = (word << 4) | nibble;
	}

	// The crc is the three nibbles of the value XORed together and inverted, so all four nibbles of
	// the word XOR to 0x0F.
	if (((word ^ (word >> 4) ^ (word >> 8) ^ (word >> 12)) & 0x0F) != 0x0F) return FALSE;

	if (p_value) *p_value = word >> 4;
	return TRUE;
}

BOOL pifDshot_DecodeTelemetry(uint16_t value, BOOL extended, PifDshotTelemetry *p_telemetry)
{
	uint32_t period;

	if (!p_telemetry) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	/*
	 * An eRPM comes as eee mmmmmmmmm: a period in microseconds, the mantissa shifted up by the
	 * exponent. A period with a non-zero exponent and the top bit of the mantissa clear would fit
	 * the next exponent down with twice the precision, so no ESC sends one, which leaves those
	 * seven shapes for EDT: the exponent says what the value is and the low byte carries it.
	 */
	if (extended) {
		p_telemetry->value = value & 0x00FF;
		switch (value >> 8) {
		case 0x02:
			p_telemetry->type = DTT_TEMPERATURE;
			return TRUE;

		case 0x04:
			p_telemetry->type = DTT_VOLTAGE;
			p_telemetry->value *= 250;
			return TRUE;

		case 0x06:
			p_telemetry->type = DTT_CURRENT;
			p_telemetry->value *= 1000;
			return TRUE;

		case 0x08:
			p_telemetry->type = DTT_DEBUG1;
			return TRUE;

		case 0x0A:
			p_telemetry->type = DTT_DEBUG2;
			return TRUE;

		case 0x0C:
			p_telemetry->type = DTT_STRESS;
			return TRUE;

		case 0x0E:
			p_telemetry->type = DTT_STATUS;
			return TRUE;

		default:
			break;
		}
	}

	p_telemetry->type = DTT_ERPM;

	// The longest period there is stands for a motor at a standstill.
	if (value == 0x0FFF) {
		p_telemetry->value = 0;
		return TRUE;
	}

	period = (uint32_t)(value & 0x01FF) << (value >> 9);
	if (!period) return FALSE;

	// A period is one electrical revolution, and a minute is 60,000,000 microseconds of them.
	p_telemetry->value = (60000000UL + period / 2) / period;
	return TRUE;
}

static uint8_t _nextSlot(uint8_t slot)
{
	return (slot + 1) % (PIF_DSHOT_COMMAND_QUEUE_SIZE + 1);
}

static BOOL _isQueueEmpty(PifDshot *p_owner)
{
	return p_owner->__queue_head == p_owner->__queue_tail;
}

static BOOL _areMotorsStopped(PifDshot *p_owner)
{
	uint8_t i;

	for (i = 0; i < p_owner->_motor_count; i++) {
		if (p_owner->_motor[i].value) return FALSE;
	}
	return TRUE;
}

// Holds the output of the cycles to come for at least the given time.
static void _hold(PifDshot *p_owner, PifDshotCommandControl *p_control, uint32_t time_us)
{
	p_control->delay_cycles = (time_us + p_owner->_cycle_us - 1) / p_owner->_cycle_us;
}

// Builds the frames of one cycle and hands them over: the throttle of every motor, or the commands
// of p_command where a command is going out.
static void _writeFrames(PifDshot *p_owner, const uint8_t *p_command)
{
	uint16_t frame[PIF_DSHOT_MAX_MOTORS];
	PifDshotMotor *p_motor;
	uint16_t value;
	BOOL telemetry;
	uint8_t i;

	for (i = 0; i < p_owner->_motor_count; i++) {
		p_motor = &p_owner->_motor[i];
		value = p_command ? p_command[i] : p_motor->value;
		// A command is only taken with the telemetry bit set, which it therefore gets whether or not
		// the client asked for telemetry.
		telemetry = p_motor->request_telemetry || (p_command && value != DSC_MOTOR_STOP);
		p_motor->request_telemetry = FALSE;
		frame[i] = pifDshot_MakeFrame(value, telemetry, p_owner->_bidirectional);
	}
	(*p_owner->__act_write)(p_owner, frame, p_owner->_motor_count);
}

#ifdef COLLECT_SIGNAL_DSHOT

// Step of the command going out, or PIF_DSHOT_CS_NO_COMMAND while none is.
static uint8_t _csState(PifDshot *p_owner)
{
	if (_isQueueEmpty(p_owner)) return PIF_DSHOT_CS_NO_COMMAND;
	return p_owner->__queue[p_owner->__queue_tail].state;
}

// The command going out, or DSC_MOTOR_STOP while none is. Motors the command is not meant for hold
// DSC_MOTOR_STOP, which is 0, so the highest entry is the command itself.
static uint8_t _csCommand(PifDshot *p_owner)
{
	PifDshotCommandControl *p_control;
	uint8_t command = DSC_MOTOR_STOP;
	uint8_t i;

	if (_isQueueEmpty(p_owner)) return DSC_MOTOR_STOP;

	p_control = &p_owner->__queue[p_owner->__queue_tail];
	for (i = 0; i < p_owner->_motor_count; i++) {
		if (p_control->command[i] > command) command = p_control->command[i];
	}
	return command;
}

#endif	// COLLECT_SIGNAL_DSHOT

BOOL pifDshot_Init(PifDshot *p_owner, PifId id, uint8_t motor_count, BOOL bidirectional, uint32_t cycle_us, PifActDshotWrite act_write)
{
	if (!p_owner || !motor_count || motor_count > PIF_DSHOT_MAX_MOTORS || !cycle_us || !act_write) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifDshot));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_motor_count = motor_count;
	p_owner->_bidirectional = bidirectional;
	p_owner->_cycle_us = cycle_us;
	p_owner->__act_write = act_write;
	return TRUE;
}

void pifDshot_Clear(PifDshot *p_owner)
{
#ifdef COLLECT_SIGNAL_DSHOT
	pifDshot_ResetCsFlag(p_owner, PIF_CS_INDEX_ALL, DS_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

void pifDshot_SetCyclePeriod(PifDshot *p_owner, uint32_t cycle_us)
{
	if (cycle_us) p_owner->_cycle_us = cycle_us;
}

void pifDshot_SetThrottle(PifDshot *p_owner, uint8_t index, uint16_t value)
{
	uint8_t i;

	if (value && value < PIF_DSHOT_MIN_THROTTLE) value = PIF_DSHOT_MIN_THROTTLE;
	else if (value > PIF_DSHOT_MAX_THROTTLE) value = PIF_DSHOT_MAX_THROTTLE;

	for (i = 0; i < p_owner->_motor_count; i++) {
		if (index == i || index == PIF_DSHOT_ALL_MOTORS) p_owner->_motor[i].value = value;
	}
}

BOOL pifDshot_Command(PifDshot *p_owner, uint8_t index, uint8_t command)
{
	PifDshotCommandControl *p_control;
	uint8_t head;
	uint8_t i;

	if (command > PIF_DSHOT_MAX_COMMAND || (index >= p_owner->_motor_count && index != PIF_DSHOT_ALL_MOTORS)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	head = _nextSlot(p_owner->__queue_head);
	if (head == p_owner->__queue_tail) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	p_control = &p_owner->__queue[p_owner->__queue_head];
	p_control->state = DCS_IDLE_WAIT;
	p_control->delay_cycles = 0;
	p_control->repeats = 1;
	p_control->post_delay_us = REPEAT_DELAY_US;

	switch (command) {
	// What an ESC stores it only acts on after seeing the same command a number of times, which
	// keeps a stray frame from changing a setting.
	case DSC_SPIN_DIRECTION_1:
	case DSC_SPIN_DIRECTION_2:
	case DSC_3D_MODE_OFF:
	case DSC_3D_MODE_ON:
	case DSC_SAVE_SETTINGS:
	case DSC_EXTENDED_TELEMETRY_ENABLE:
	case DSC_EXTENDED_TELEMETRY_DISABLE:
	case DSC_SPIN_DIRECTION_NORMAL:
	case DSC_SPIN_DIRECTION_REVERSED:
		p_control->repeats = STORED_COMMAND_FRAMES;
		break;

	// A beacon beeps for a good while, and a command sent into the beep is lost.
	case DSC_BEACON1:
	case DSC_BEACON2:
	case DSC_BEACON3:
	case DSC_BEACON4:
	case DSC_BEACON5:
		p_control->post_delay_us = BEACON_DELAY_US;
		break;

	case DSC_ESC_INFO:
		p_control->post_delay_us = ESC_INFO_DELAY_US;
		break;

	default:
		break;
	}

	for (i = 0; i < p_owner->_motor_count; i++) {
		p_control->command[i] = (index == i || index == PIF_DSHOT_ALL_MOTORS) ? command : DSC_MOTOR_STOP;
	}

	// Published last: pifDshot_Update() may run between two calls of this one, never inside it, but
	// a command it gets to see has to be a whole one.
	p_owner->__queue_head = head;
	return TRUE;
}

BOOL pifDshot_IsCommandBusy(PifDshot *p_owner)
{
	return !_isQueueEmpty(p_owner);
}

// Body of pifDshot_Update().
static BOOL _update(PifDshot *p_owner)
{
	PifDshotCommandControl *p_control;

	while (!_isQueueEmpty(p_owner)) {
		p_control = &p_owner->__queue[p_owner->__queue_tail];

		if (p_control->state == DCS_IDLE_WAIT) {
			// The motors keep their throttle until they have all come to a stop.
			if (!_areMotorsStopped(p_owner)) break;
			p_control->state = DCS_START_DELAY;
			_hold(p_owner, p_control, START_DELAY_US);
		}

		// A pause holds the output: nothing goes on the wire this cycle.
		if (p_control->delay_cycles) {
			p_control->delay_cycles--;
			return FALSE;
		}

		if (p_control->state == DCS_POST_DELAY) {
			// This command is through. A command waiting behind it starts its own sequence in this
			// same cycle, so that the pause between the two is the pause and not a cycle more.
			p_owner->__queue_tail = _nextSlot(p_owner->__queue_tail);
			continue;
		}

		// DCS_START_DELAY and DCS_ACTIVE both send a frame once their pause is over, and the pause
		// before the first frame differs from the one between two frames in length alone.
		p_control->state = DCS_ACTIVE;
		_writeFrames(p_owner, p_control->command);
		p_control->repeats--;
		if (p_control->repeats) {
			_hold(p_owner, p_control, REPEAT_DELAY_US);
		}
		else {
			p_control->state = DCS_POST_DELAY;
			_hold(p_owner, p_control, p_control->post_delay_us);
		}
		return TRUE;
	}

	_writeFrames(p_owner, NULL);
	return TRUE;
}

BOOL pifDshot_Update(PifDshot *p_owner)
{
	BOOL rtn = _update(p_owner);

#ifdef COLLECT_SIGNAL_DSHOT
	pifCollectSignal_Put(&p_owner->__cs[DS_CSF_STATE_IDX], _csState(p_owner));
	pifCollectSignal_Put(&p_owner->__cs[DS_CSF_COMMAND_IDX], _csCommand(p_owner));
#endif
	return rtn;
}

BOOL pifDshot_PutGcr(PifDshot *p_owner, uint8_t index, uint32_t gcr)
{
	PifDshotMotor *p_motor;
	PifDshotTelemetry telemetry;
	uint16_t value;

	if (index >= p_owner->_motor_count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_motor = &p_owner->_motor[index];

	if (gcr == PIF_DSHOT_GCR_NONE) {
		p_motor->_missing_count++;
		return FALSE;
	}
	if (!pifDshot_DecodeGcr(gcr, &value) || !pifDshot_DecodeTelemetry(value, p_owner->extended_telemetry, &telemetry)) {
		p_motor->_error_count++;
		return FALSE;
	}

	p_motor->_telemetry_count++;
	if (telemetry.type == DTT_ERPM) p_motor->_erpm = telemetry.value;
	if (p_owner->evt_telemetry) (*p_owner->evt_telemetry)(p_owner, index, &telemetry);
	return TRUE;
}

#ifdef COLLECT_SIGNAL_DSHOT

BOOL pifDshot_SetCsFlag(PifDshot *p_owner, uint8_t index, PifDshotCsFlag flag)
{
	(void)index;

	if (flag & DS_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[DS_CSF_STATE_IDX], "DSS", p_owner->_id, CSVT_REG, 3,
				_csState(p_owner))) return FALSE;
	}
	if (flag & DS_CSF_COMMAND_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[DS_CSF_COMMAND_IDX], "DSC", p_owner->_id, CSVT_REG, 6,
				_csCommand(p_owner))) return FALSE;
	}
	return TRUE;
}

void pifDshot_ResetCsFlag(PifDshot *p_owner, uint8_t index, PifDshotCsFlag flag)
{
	(void)index;

	if (flag & DS_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DS_CSF_STATE_IDX]);
	if (flag & DS_CSF_COMMAND_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[DS_CSF_COMMAND_IDX]);
}

#endif	// COLLECT_SIGNAL_DSHOT
