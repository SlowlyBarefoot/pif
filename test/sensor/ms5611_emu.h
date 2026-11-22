// SPDX-License-Identifier: BSD-3-Clause
// An MS5611 in memory behind a PifI2cPort: the five commands it answers, the calibration PROM, and the ADC
// that holds whatever the conversion that was asked for last came to.
//
// The PROM and the two conversions it is reset with are the worked example of the datasheet: those
// coefficients with those conversions come to 20.07 degrees C and 1000.09 hPa. The CRC in the low nibble of
// the last word is the one of the other words.
#ifndef MS5611_EMU_H
#define MS5611_EMU_H

#include "sensor/baro/pif_ms5611.h"

#include <string.h>


#define MS5611_EMU_D1			9085466UL	///< Pressure conversion of the worked example
#define MS5611_EMU_D2			8569150UL	///< Temperature conversion of the worked example
#define MS5611_EMU_PRESSURE		1000.09f	///< What the two come to
#define MS5611_EMU_TEMPERATURE	20.07f

// C0 and the six coefficients, then the serial code with the CRC of them all in its low nibble.
static const uint16_t s_ms5611_emu_prom[8] = { 0x3132, 40127, 36924, 23317, 23282, 33464, 28312, 0x3A02 };


typedef struct
{
	uint16_t prom[8];		// What a PROM read answers
	uint32_t d1, d2;		// What the pressure and the temperature conversion come to
	int resets;				// Reset commands taken
	uint8_t conv_cmd;		// The conversion command asked for last, 0 for none since the reset
	uint32_t adc;			// What the conversion left in the ADC
	int d1_convs, d2_convs;	// Conversions started per channel
	int prom_reads;			// Words read out of the PROM
	int adc_reads;			// Reads of the ADC
	int fail_in;			// Transfers that go through before they all fail, -1 for never
	int transfers;			// Transfers the device was asked for, failed ones among them
} Ms5611Emu;


static void ms5611Emu_Reset(Ms5611Emu* p_emu)
{
	memset(p_emu, 0, sizeof(Ms5611Emu));
	memcpy(p_emu->prom, s_ms5611_emu_prom, sizeof(p_emu->prom));
	p_emu->d1 = MS5611_EMU_D1;
	p_emu->d2 = MS5611_EMU_D2;
	p_emu->fail_in = -1;
}

// TRUE while the device still answers. fail_in counts the transfers that go through first.
static BOOL ms5611Emu_Alive(Ms5611Emu* p_emu)
{
	p_emu->transfers++;
	if (p_emu->fail_in < 0) return TRUE;
	if (p_emu->fail_in > 0) {
		p_emu->fail_in--;
		return TRUE;
	}
	return FALSE;
}

static PifI2cReturn ms5611Emu_Write(Ms5611Emu* p_emu, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	uint8_t cmd;

	if (!ms5611Emu_Alive(p_emu)) return IR_ERROR;
	// Every command is one byte sent on its own, with no internal address in front of it.
	if (isize || size != 1) return IR_ERROR;

	cmd = p_data[0];
	if (cmd == MS5611_REG_RESET) {
		p_emu->resets++;
		p_emu->conv_cmd = 0;
		p_emu->adc = 0;
		return IR_COMPLETE;
	}
	if ((cmd & 0xF0) == MS5611_REG_CONV_D1) {
		p_emu->conv_cmd = cmd;
		p_emu->adc = p_emu->d1;
		p_emu->d1_convs++;
		return IR_COMPLETE;
	}
	if ((cmd & 0xF0) == MS5611_REG_CONV_D2) {
		p_emu->conv_cmd = cmd;
		p_emu->adc = p_emu->d2;
		p_emu->d2_convs++;
		return IR_COMPLETE;
	}
	return IR_ERROR;
}

static PifI2cReturn ms5611Emu_Read(Ms5611Emu* p_emu, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	int word;

	if (!ms5611Emu_Alive(p_emu)) return IR_ERROR;
	if (isize != 1) return IR_ERROR;

	if (iaddr == MS5611_REG_ADC_READ && size == 3) {
		p_data[0] = (uint8_t)(p_emu->adc >> 16);
		p_data[1] = (uint8_t)(p_emu->adc >> 8);
		p_data[2] = (uint8_t)p_emu->adc;
		p_emu->adc_reads++;
		return IR_COMPLETE;
	}

	// The eight words sit two addresses apart from MS5611_REG_READ_PROM on, each MSB first.
	if (iaddr >= MS5611_REG_READ_PROM && iaddr <= MS5611_REG_READ_PROM + 14 &&
			!((iaddr - MS5611_REG_READ_PROM) & 1) && size == 2) {
		word = (int)(iaddr - MS5611_REG_READ_PROM) / 2;
		p_data[0] = (uint8_t)(p_emu->prom[word] >> 8);
		p_data[1] = (uint8_t)p_emu->prom[word];
		p_emu->prom_reads++;
		return IR_COMPLETE;
	}
	return IR_ERROR;
}


#endif  // MS5611_EMU_H
