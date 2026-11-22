// SPDX-License-Identifier: BSD-3-Clause
// An HMC5883L in memory behind a PifI2cPort: its three writable registers, the identification registers that
// read "H43", the measurement that waits in the six output registers with the ready bit over it, and the two
// bias currents of the self test.
//
// Two things the datasheet says about the device are modelled on purpose, because the driver has to deal with
// both: a measurement that is still running answers "not ready" rather than old values, and the measurement
// that follows a gain change comes out at the gain before it.
#ifndef HMC5883_EMU_H
#define HMC5883_EMU_H

#include "sensor/mag/pif_hmc5883.h"

#include <string.h>


// The self test bias at the gain the driver runs it at, 660 LSb/Gauss: 1.16 Gauss on X and Y, 1.08 on Z.
#define HMC5883_EMU_BIAS_XY		766
#define HMC5883_EMU_BIAS_Z		713


typedef struct
{
	uint8_t config_a, config_b, mode;	// The three registers that can be written
	uint8_t ident[3];					// What the identification registers read
	int16_t field[AXIS_COUNT];			// What a measurement of the field comes to
	int16_t pos_bias[AXIS_COUNT];		// What one with the positive bias current comes to
	int16_t neg_bias[AXIS_COUNT];		// What one with the negative bias current comes to
	int16_t stale[AXIS_COUNT];			// What the one measurement after a gain change comes to
	BOOL gain_changed;					// That measurement is still to come
	int16_t data[AXIS_COUNT];			// The measurement in the output registers
	BOOL ready;							// The ready bit over it
	int not_ready_reads;				// Status reads a measurement takes before it is ready
	int pending_reads;					// What is left of them
	uint16_t ms_per_status_read;		// Milliseconds a status read takes, for a device that is late
	int measurements;					// Measurements the device has made
	int fail_in;						// Transfers that go through before they all fail, -1 for never
	int transfers;			// Transfers the device was asked for, failed ones among them
} Hmc5883Emu;


static void hmc5883Emu_Reset(Hmc5883Emu* p_emu)
{
	int axis;

	memset(p_emu, 0, sizeof(Hmc5883Emu));
	// What the device holds after power on: one sample at 15Hz, gain 1.3 Gauss, single measurement mode.
	p_emu->config_a = HMC5883_MEASURE_MODE_NORMAL | HMC5883_SAMPLES_1 | HMC5883_DATARATE_15HZ;
	p_emu->config_b = HMC5883_GAIN_1_3GA;
	p_emu->mode = HMC5883_MODE_SINGLE;
	p_emu->ident[0] = 'H';
	p_emu->ident[1] = '4';
	p_emu->ident[2] = '3';
	for (axis = 0; axis < AXIS_COUNT; axis++) {
		p_emu->field[axis] = (int16_t)(100 * (axis + 1));
		p_emu->pos_bias[axis] = axis == AXIS_Z ? HMC5883_EMU_BIAS_Z : HMC5883_EMU_BIAS_XY;
		p_emu->neg_bias[axis] = (int16_t)-p_emu->pos_bias[axis];
		// Nothing like a measurement, so that a driver which fails to throw this one away is caught.
		p_emu->stale[axis] = 4000;
	}
	p_emu->fail_in = -1;
}

// Makes one measurement out of whatever the configuration asks for, and leaves it in the output registers.
static void hmc5883Emu_Measure(Hmc5883Emu* p_emu)
{
	const int16_t* p_source;
	int axis;

	if (p_emu->gain_changed) {
		p_source = p_emu->stale;
		p_emu->gain_changed = FALSE;
	}
	else {
		switch (p_emu->config_a & HMC5883_MEASURE_MODE_MASK) {
		case HMC5883_MEASURE_MODE_POS_BIAS:	p_source = p_emu->pos_bias; break;
		case HMC5883_MEASURE_MODE_NEG_BIAS:	p_source = p_emu->neg_bias; break;
		default:							p_source = p_emu->field; break;
		}
	}
	for (axis = 0; axis < AXIS_COUNT; axis++) p_emu->data[axis] = p_source[axis];

	p_emu->measurements++;
	p_emu->pending_reads = p_emu->not_ready_reads;
	p_emu->ready = p_emu->pending_reads <= 0;
}

static BOOL hmc5883Emu_Alive(Hmc5883Emu* p_emu)
{
	p_emu->transfers++;
	if (p_emu->fail_in < 0) return TRUE;
	if (p_emu->fail_in > 0) {
		p_emu->fail_in--;
		return TRUE;
	}
	return FALSE;
}

static PifI2cReturn hmc5883Emu_Write(Hmc5883Emu* p_emu, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	size_t i;

	if (!hmc5883Emu_Alive(p_emu)) return IR_ERROR;
	if (isize != 1) return IR_ERROR;

	for (i = 0; i < size; i++) {
		switch (iaddr + i) {
		case HMC5883_REG_CONFIG_A:
			p_emu->config_a = p_data[i];
			break;

		case HMC5883_REG_CONFIG_B:
			// The measurement that comes next keeps the gain this one takes the place of.
			if ((p_data[i] & HMC5883_GAIN_MASK) != (p_emu->config_b & HMC5883_GAIN_MASK)) p_emu->gain_changed = TRUE;
			p_emu->config_b = p_data[i];
			break;

		case HMC5883_REG_MODE:
			p_emu->mode = p_data[i];
			// Either mode starts measuring; the continuous one keeps going on its own from here.
			if ((p_emu->mode & HMC5883_MODE_MASK) != HMC5883_MODE_IDLE) hmc5883Emu_Measure(p_emu);
			break;

		default:
			return IR_ERROR;
		}
	}
	return IR_COMPLETE;
}

static PifI2cReturn hmc5883Emu_Read(Hmc5883Emu* p_emu, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	if (!hmc5883Emu_Alive(p_emu)) return IR_ERROR;
	if (isize != 1) return IR_ERROR;

	if (iaddr == HMC5883_REG_STATUS && size == 1) {
		pif_cumulative_timer1ms += p_emu->ms_per_status_read;
		if (p_emu->pending_reads > 0) {
			p_emu->pending_reads--;
			p_emu->ready = p_emu->pending_reads <= 0;
		}
		p_data[0] = p_emu->ready ? HMC5883_RDY_MASK : 0;
		return IR_COMPLETE;
	}

	if (iaddr == HMC5883_REG_OUT_X_M && size == 6) {
		// The six output registers, each axis MSB first and Z between X and Y.
		p_data[0] = (uint8_t)(p_emu->data[AXIS_X] >> 8);
		p_data[1] = (uint8_t)p_emu->data[AXIS_X];
		p_data[2] = (uint8_t)(p_emu->data[AXIS_Z] >> 8);
		p_data[3] = (uint8_t)p_emu->data[AXIS_Z];
		p_data[4] = (uint8_t)(p_emu->data[AXIS_Y] >> 8);
		p_data[5] = (uint8_t)p_emu->data[AXIS_Y];
		// Reading them is what clears the ready bit, and in continuous mode the next measurement follows.
		p_emu->ready = FALSE;
		if ((p_emu->mode & HMC5883_MODE_MASK) == HMC5883_MODE_CONTINOUS) hmc5883Emu_Measure(p_emu);
		return IR_COMPLETE;
	}

	if (iaddr == HMC5883_REG_IDENT_A && size == 3) {
		memcpy(p_data, p_emu->ident, 3);
		return IR_COMPLETE;
	}

	if (iaddr == HMC5883_REG_CONFIG_A && size == 1) {
		p_data[0] = p_emu->config_a;
		return IR_COMPLETE;
	}
	if (iaddr == HMC5883_REG_CONFIG_B && size == 1) {
		p_data[0] = p_emu->config_b;
		return IR_COMPLETE;
	}
	if (iaddr == HMC5883_REG_MODE && size == 1) {
		p_data[0] = p_emu->mode;
		return IR_COMPLETE;
	}
	return IR_ERROR;
}


#endif  // HMC5883_EMU_H
