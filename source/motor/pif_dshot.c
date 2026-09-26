#include "motor/pif_dshot.h"


#define DSHOT_INITIAL_DELAY_US		10000UL
#define DSHOT_COMMAND_DELAY_US		1000UL
#define DSHOT_ESC_INFO_DELAY_US		12000UL
#define DSHOT_BEACON_DELAY_US		100000UL

#define GCR_BITS					21

// An answer has 21 bits, so the samples of one lie within a couple of bits of 21 times the
// oversampling. Fewer than the minimum left after the start cannot hold one, and scanning beyond
// the maximum only reads whatever the line did after it.
#define GCR_MIN_BITS				(GCR_BITS - 2)
#define GCR_MAX_BITS				(GCR_BITS + 2)

#define GCR_BAD_SYMBOL				0xFF


// 5-bit GCR symbol to 4-bit nibble. Symbols that no nibble maps to are marked, so that a frame
// with one of them fails rather than decoding to something plausible.
static const uint8_t c_gcr_decode[32] = {
	GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, GCR_BAD_SYMBOL,
	GCR_BAD_SYMBOL, 9, 10, 11, GCR_BAD_SYMBOL, 13, 14, 15,
	GCR_BAD_SYMBOL, GCR_BAD_SYMBOL, 2, 3, GCR_BAD_SYMBOL, 5, 6, 7,
	GCR_BAD_SYMBOL, 0, 8, 1, GCR_BAD_SYMBOL, 4, 12, GCR_BAD_SYMBOL
};


/**
 * @fn _appendRun
 * @brief Adds the bits of one run between two edges to a GCR frame: an edge stands for a 1 and
 *        every further bit of the run for a 0.
 * @param p_gcr Frame being built.
 * @param p_bits Number of bits in it so far.
 * @param len Bits in the run, at least one.
 */
static void _appendRun(uint32_t* p_gcr, uint8_t* p_bits, uint8_t len)
{
	*p_gcr = (*p_gcr << len) | (1UL << (len - 1));
	*p_bits += len;
}

/**
 * @fn _cyclesFromTime
 * @brief Output cycles that cover at least a given time.
 * @param p_owner Pointer to the instance.
 * @param time_us Time to cover.
 * @return Number of cycles, rounded up.
 */
static uint32_t _cyclesFromTime(PifDshot* p_owner, uint32_t time_us)
{
	return (time_us + p_owner->_cycle_us - 1) / p_owner->_cycle_us;
}

/**
 * @fn _isQueueEmpty
 * @param p_owner Pointer to the instance.
 * @return TRUE if no command is queued.
 */
static BOOL _isQueueEmpty(PifDshot* p_owner)
{
	return p_owner->__queue_head == p_owner->__queue_tail;
}

/**
 * @fn _isLastCommand
 * @param p_owner Pointer to the instance.
 * @return TRUE if the command at the tail is the only one queued.
 */
static BOOL _isLastCommand(PifDshot* p_owner)
{
	return (p_owner->__queue_tail + 1) % (PIF_DSHOT_COMMAND_QUEUE_SIZE + 1) == p_owner->__queue_head;
}

/**
 * @fn _areMotorsIdle
 * @param p_owner Pointer to the instance.
 * @return TRUE if every motor is at 0.
 */
static BOOL _areMotorsIdle(PifDshot* p_owner)
{
	uint8_t i;

	for (i = 0; i < p_owner->_motor_count; i++) {
		if (p_owner->_motor[i].value) return FALSE;
	}
	return TRUE;
}

/**
 * @fn _nextCommand
 * @brief Drops the command at the tail and readies the one after it, if any, to go out at once:
 *        the pause before a sequence has already been had.
 * @param p_owner Pointer to the instance.
 * @return TRUE if another command follows.
 */
static BOOL _nextCommand(PifDshot* p_owner)
{
	PifDshotCommandControl* p_next;

	p_owner->__queue_tail = (p_owner->__queue_tail + 1) % (PIF_DSHOT_COMMAND_QUEUE_SIZE + 1);
	if (_isQueueEmpty(p_owner)) return FALSE;

	p_next = &p_owner->__queue[p_owner->__queue_tail];
	p_next->state = DCS_ACTIVE;
	p_next->delay_cycles = 0;
	return TRUE;
}

/**
 * @fn _writeFrames
 * @brief Builds the frames of a cycle and hands them to act_write.
 * @param p_owner Pointer to the instance.
 * @param p_command Command of each motor, or NULL for their throttle.
 */
static void _writeFrames(PifDshot* p_owner, const uint8_t* p_command)
{
	uint16_t frames[PIF_DSHOT_MAX_MOTORS];
	PifDshotMotor* p_motor;
	uint16_t value;
	BOOL telemetry;
	uint8_t i;

	for (i = 0; i < p_owner->_motor_count; i++) {
		p_motor = &p_owner->_motor[i];
		if (p_command) {
			// ESCs take a settings command only with the telemetry bit set.
			value = p_command[i];
			telemetry = value != DSC_MOTOR_STOP || p_motor->request_telemetry;
		}
		else {
			value = p_motor->value;
			telemetry = p_motor->request_telemetry;
		}
		p_motor->request_telemetry = FALSE;
		frames[i] = pifDshot_MakeFrame(value, telemetry, p_owner->_bidirectional);
	}
	(*p_owner->__act_write)(p_owner, frames, p_owner->_motor_count);
}

uint16_t pifDshot_MakeFrame(uint16_t value, BOOL telemetry, BOOL bidirectional)
{
	uint16_t frame = ((value & 0x07FF) << 1) | (telemetry ? 1 : 0);
	uint16_t csum;

	csum = frame ^ (frame >> 4) ^ (frame >> 8);
	if (bidirectional) csum = ~csum;
	return (frame << 4) | (csum & 0x0F);
}

uint8_t pifDshot_LoadBuffer(uint32_t* p_buffer, uint8_t stride, uint16_t frame, uint32_t bit0, uint32_t bit1)
{
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * stride] = (frame & 0x8000) ? bit1 : bit0;
		frame <<= 1;
	}
	p_buffer[16 * stride] = 0;
	p_buffer[17 * stride] = 0;
	return PIF_DSHOT_BUFFER_SIZE;
}

uint8_t pifDshot_LoadBufferProshot(uint32_t* p_buffer, uint8_t stride, uint16_t frame, uint32_t base, uint32_t width)
{
	uint8_t i;

	for (i = 0; i < 4; i++) {
		p_buffer[i * stride] = base + ((frame & 0xF000) >> 12) * width;
		frame <<= 4;
	}
	p_buffer[4 * stride] = 0;
	p_buffer[5 * stride] = 0;
	return PIF_PROSHOT_BUFFER_SIZE;
}

void pifDshot_InitBitbangBuffer(uint32_t* p_buffer, uint32_t set_mask, uint32_t reset_mask, BOOL inverted)
{
	uint32_t active = inverted ? reset_mask : set_mask;
	uint32_t rest = inverted ? set_mask : reset_mask;
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * 3] |= active;
		p_buffer[i * 3 + 1] = 0;
		p_buffer[i * 3 + 2] |= rest;
	}

	// The hold bit: at rest for all three of its slots.
	p_buffer[16 * 3] |= rest;
	p_buffer[16 * 3 + 1] = 0;
	p_buffer[16 * 3 + 2] = 0;
}

void pifDshot_ClearBitbangBuffer(uint32_t* p_buffer)
{
	uint8_t i;

	for (i = 0; i < 16; i++) {
		p_buffer[i * 3 + 1] = 0;
	}
}

void pifDshot_LoadBitbangBuffer(uint32_t* p_buffer, uint32_t set_mask, uint32_t reset_mask, uint16_t frame, BOOL inverted)
{
	uint32_t rest = inverted ? set_mask : reset_mask;
	uint8_t i;

	// A 1 stays active until the third slot, where every pin goes back anyway, so only a 0 needs
	// anything here.
	for (i = 0; i < 16; i++) {
		if (!(frame & 0x8000)) p_buffer[i * 3 + 1] |= rest;
		frame <<= 1;
	}
}

uint32_t pifDshot_EdgesToGcr(const uint32_t* p_edges, uint8_t count, uint32_t bit_ticks)
{
	uint32_t gcr = 0;
	uint32_t len;
	uint8_t bits = 0;
	uint8_t i;

	if (!count || !bit_ticks) return PIF_DSHOT_GCR_NONE;

	for (i = 1; i < count && bits < GCR_BITS; i++) {
		// Rounded to the nearest bit: the capture timer runs off a clock of its own, so a run of
		// the ESC is a little longer or shorter than a whole number of our bits.
		len = (p_edges[i] - p_edges[i - 1] + bit_ticks / 2) / bit_ticks;
		if (!len || bits + len > GCR_BITS) return PIF_DSHOT_GCR_INVALID;
		_appendRun(&gcr, &bits, len);
	}

	// The line rests at the level of the last bit, so the last run has no edge at its end and is
	// whatever is left of the frame.
	if (bits < GCR_BITS) _appendRun(&gcr, &bits, GCR_BITS - bits);
	return gcr;
}

uint32_t pifDshot_SamplesToGcr(const uint16_t* p_samples, uint16_t count, uint16_t mask, uint8_t oversample)
{
	uint32_t gcr = 0;
	uint16_t start, end, i;
	uint16_t level, last;
	uint16_t len;
	uint8_t bits = 0;

	if (!p_samples || !oversample) return PIF_DSHOT_GCR_NONE;
	if (count < GCR_MIN_BITS * oversample) return PIF_DSHOT_GCR_NONE;

	// The line idles high, and the answer starts where it first goes low. An ESC too busy to
	// answer leaves it high, which is not an error in the answer.
	end = count - GCR_MIN_BITS * oversample;
	for (start = 0; start < end; start++) {
		if (!(p_samples[start] & mask)) break;
	}
	if (start >= end) return PIF_DSHOT_GCR_NONE;

	end = start + GCR_MAX_BITS * oversample;
	if (end > count) end = count;

	last = 0;
	for (i = start + 1; i < end; i++) {
		level = p_samples[i] & mask;
		if (level == last) continue;

		// Rounded to the nearest bit, and a run shorter than half a bit still counts as one: it is
		// an edge, and an edge is a bit.
		len = (i - start + oversample / 2) / oversample;
		if (!len) len = 1;
		if (bits + len > GCR_BITS) return PIF_DSHOT_GCR_INVALID;
		_appendRun(&gcr, &bits, len);
		start = i;
		last = level;
	}

	// The last bit leaves the line high, where it rests, so there is no edge after it and its run
	// is whatever is left of the 21 bits. Too many bits left means the answer broke off.
	if (bits < GCR_MIN_BITS - 1) return PIF_DSHOT_GCR_NONE;
	if (bits < GCR_BITS) _appendRun(&gcr, &bits, GCR_BITS - bits);
	return gcr;
}

BOOL pifDshot_DecodeGcr(uint32_t gcr, uint16_t* p_value)
{
	uint16_t decoded = 0;
	uint8_t nibble, csum;
	int8_t i;

	if (gcr == PIF_DSHOT_GCR_NONE || gcr == PIF_DSHOT_GCR_INVALID) return FALSE;

	// The 21st bit is the start bit and carries nothing.
	for (i = 3; i >= 0; i--) {
		nibble = c_gcr_decode[(gcr >> (i * 5)) & 0x1F];
		if (nibble == GCR_BAD_SYMBOL) return FALSE;
		decoded = (decoded << 4) | nibble;
	}

	// The answer is 12 bits and an inverted checksum of their three nibbles, so all four nibbles
	// XOR to 0xF.
	csum = decoded ^ (decoded >> 8);
	csum ^= csum >> 4;
	if ((csum & 0x0F) != 0x0F) return FALSE;

	if (p_value) *p_value = decoded >> 4;
	return TRUE;
}

BOOL pifDshot_DecodeTelemetry(uint16_t value, BOOL extended, PifDshotTelemetry* p_telemetry)
{
	uint32_t period;

	if (!p_telemetry) return FALSE;

	// An eRPM is a period in microseconds as eeem mmmm mmmm, the mantissa shifted left by the
	// exponent. A period never needs a non-zero exponent with the top mantissa bit clear, since
	// the next lower exponent holds the same period more precisely, and those are the forms
	// Extended DShot Telemetry takes for its other values.
	if (extended) {
		p_telemetry->value = value & 0xFF;
		switch (value & 0x0F00) {
		case 0x0200:
			p_telemetry->type = DTT_TEMPERATURE;
			return TRUE;

		case 0x0400:
			p_telemetry->type = DTT_VOLTAGE;
			p_telemetry->value *= 250;
			return TRUE;

		case 0x0600:
			p_telemetry->type = DTT_CURRENT;
			p_telemetry->value *= 1000;
			return TRUE;

		case 0x0800:
			p_telemetry->type = DTT_DEBUG1;
			return TRUE;

		case 0x0A00:
			p_telemetry->type = DTT_DEBUG2;
			return TRUE;

		case 0x0C00:
			p_telemetry->type = DTT_STRESS;
			return TRUE;

		case 0x0E00:
			p_telemetry->type = DTT_STATUS;
			return TRUE;
		}
	}

	p_telemetry->type = DTT_ERPM;

	// The longest period there is means the motor stands still.
	if (value == 0x0FFF) {
		p_telemetry->value = 0;
		return TRUE;
	}

	period = (uint32_t)(value & 0x01FF) << ((value & 0x0E00) >> 9);
	if (!period) return FALSE;

	// A period is one electrical revolution, so there are 60,000,000 us / period of them a minute.
	p_telemetry->value = (60000000UL + period / 2) / period;
	return TRUE;
}

BOOL pifDshot_Init(PifDshot* p_owner, PifId id, uint8_t motor_count, BOOL bidirectional, uint32_t cycle_us, PifActDshotWrite act_write)
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

void pifDshot_SetCyclePeriod(PifDshot* p_owner, uint32_t cycle_us)
{
	if (cycle_us) p_owner->_cycle_us = cycle_us;
}

void pifDshot_SetThrottle(PifDshot* p_owner, uint8_t index, uint16_t value)
{
	uint8_t i;

	if (value && value < PIF_DSHOT_MIN_THROTTLE) value = PIF_DSHOT_MIN_THROTTLE;
	else if (value > PIF_DSHOT_MAX_THROTTLE) value = PIF_DSHOT_MAX_THROTTLE;

	for (i = 0; i < p_owner->_motor_count; i++) {
		if (index == i || index == PIF_DSHOT_ALL_MOTORS) p_owner->_motor[i].value = value;
	}
}

BOOL pifDshot_Command(PifDshot* p_owner, uint8_t index, uint8_t command)
{
	PifDshotCommandControl* p_control;
	uint8_t head;
	uint8_t i;

	if (command > PIF_DSHOT_MAX_COMMAND || (index >= p_owner->_motor_count && index != PIF_DSHOT_ALL_MOTORS)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	head = (p_owner->__queue_head + 1) % (PIF_DSHOT_COMMAND_QUEUE_SIZE + 1);
	if (head == p_owner->__queue_tail) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	p_control = &p_owner->__queue[p_owner->__queue_head];
	p_control->repeats = 1;
	p_control->post_delay_us = DSHOT_COMMAND_DELAY_US;

	switch (command) {
	// What the ESC stores has to be asked for more than once before it takes it.
	case DSC_SPIN_DIRECTION_1:
	case DSC_SPIN_DIRECTION_2:
	case DSC_3D_MODE_OFF:
	case DSC_3D_MODE_ON:
	case DSC_SAVE_SETTINGS:
	case DSC_EXTENDED_TELEMETRY_ENABLE:
	case DSC_EXTENDED_TELEMETRY_DISABLE:
	case DSC_SPIN_DIRECTION_NORMAL:
	case DSC_SPIN_DIRECTION_REVERSED:
		p_control->repeats = 10;
		break;

	// A beacon beeps for a while, and a command during the beep is lost.
	case DSC_BEACON1:
	case DSC_BEACON2:
	case DSC_BEACON3:
	case DSC_BEACON4:
	case DSC_BEACON5:
		p_control->post_delay_us = DSHOT_BEACON_DELAY_US;
		break;

	case DSC_ESC_INFO:
		p_control->post_delay_us = DSHOT_ESC_INFO_DELAY_US;
		break;

	default:
		break;
	}

	for (i = 0; i < p_owner->_motor_count; i++) {
		p_control->command[i] = (index == i || index == PIF_DSHOT_ALL_MOTORS) ? command : DSC_MOTOR_STOP;
	}

	if (_areMotorsIdle(p_owner)) {
		p_control->state = DCS_START_DELAY;
		p_control->delay_cycles = _cyclesFromTime(p_owner, DSHOT_INITIAL_DELAY_US);
	}
	else {
		p_control->state = DCS_IDLE_WAIT;
		p_control->delay_cycles = 0;
	}

	// Published last: pifDshot_Update() may run between two calls, never inside this one, but a
	// command it sees has to be whole.
	p_owner->__queue_head = head;
	return TRUE;
}

BOOL pifDshot_IsCommandBusy(PifDshot* p_owner)
{
	return !_isQueueEmpty(p_owner);
}

BOOL pifDshot_Update(PifDshot* p_owner)
{
	PifDshotCommandControl* p_control;

	if (_isQueueEmpty(p_owner)) {
		_writeFrames(p_owner, NULL);
		return TRUE;
	}

	p_control = &p_owner->__queue[p_owner->__queue_tail];
	switch (p_control->state) {
	case DCS_IDLE_WAIT:
		// The motors keep their throttle until they have all come down to 0.
		if (_areMotorsIdle(p_owner)) {
			p_control->state = DCS_START_DELAY;
			p_control->delay_cycles = _cyclesFromTime(p_owner, DSHOT_INITIAL_DELAY_US);
		}
		_writeFrames(p_owner, NULL);
		return TRUE;

	// Both hold the output while their delay lasts and then send the command: the pause before a
	// sequence and the one between two repeats differ only in length.
	case DCS_START_DELAY:
	case DCS_ACTIVE:
		if (p_control->delay_cycles) {
			p_control->delay_cycles--;
			return FALSE;
		}
		p_control->state = DCS_ACTIVE;
		_writeFrames(p_owner, p_control->command);

		p_control->repeats--;
		if (p_control->repeats) {
			p_control->delay_cycles = _cyclesFromTime(p_owner, DSHOT_COMMAND_DELAY_US);
		}
		else {
			p_control->state = DCS_POST_DELAY;
			p_control->delay_cycles = _cyclesFromTime(p_owner, p_control->post_delay_us);
			// Moving on to the next command costs a held cycle of its own, which this one gives
			// back so that the gap between the two is the delay and not a cycle more.
			if (!_isLastCommand(p_owner) && p_control->delay_cycles) p_control->delay_cycles--;
		}
		return TRUE;

	case DCS_POST_DELAY:
		if (p_control->delay_cycles) {
			p_control->delay_cycles--;
			return FALSE;
		}
		if (_nextCommand(p_owner)) return FALSE;
		_writeFrames(p_owner, NULL);
		return TRUE;
	}
	return FALSE;
}

BOOL pifDshot_PutGcr(PifDshot* p_owner, uint8_t index, uint32_t gcr)
{
	PifDshotMotor* p_motor;
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
