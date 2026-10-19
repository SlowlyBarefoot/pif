// SPDX-License-Identifier: BSD-3-Clause
#include "protocol/pif_modbus_ascii.h"


/**
 * @brief Converts data between protocol wire format and native representation.
 * @param ascii Input argument used by this operation.
 * @return Converted numeric value decoded from ASCII input, or -1 when it is not a hex digit.
 */
static short _AsciiToBinary(uint8_t ascii)
{
	if (ascii >= '0' && ascii <= '9') return ascii - '0';
	else if (ascii >= 'A' && ascii <= 'F') return ascii - 'A' + 10;
	else if (ascii >= 'a' && ascii <= 'f') return ascii - 'a' + 10;
	return -1;
}

static uint8_t _BinaryToAscii(uint8_t binary)
{
	return binary < 10 ? '0' + binary : 'A' + binary - 10;
}

short pifModbusAscii_AsciiToChar(uint8_t *p_ascii)
{
	int i, v;
	short binary = 0;

	for (i = 0; i < 2; i++) {
		v = _AsciiToBinary(p_ascii[i]);
		if (v < 0) return -1;
		binary = (binary << 4) + v;
	}
	return binary;
}

long pifModbusAscii_AsciiToShort(uint8_t *p_ascii)
{
	int i, v;
	long binary = 0L;

	for (i = 0; i < 4; i++) {
		v = _AsciiToBinary(p_ascii[i]);
		if (v < 0) return -1L;
		binary = (binary << 4) + v;
	}
	return binary;
}

void pifModbusAscii_CharToAscii(uint8_t binary, uint8_t *p_ascii)
{
	p_ascii[0] = _BinaryToAscii((binary >> 4) & 0x0F);
	p_ascii[1] = _BinaryToAscii(binary & 0x0F);
}

void pifModbusAscii_ShortToAscii(uint16_t binary, uint8_t *p_ascii)
{
	p_ascii[0] = _BinaryToAscii((binary >> 12) & 0x0F);
	p_ascii[1] = _BinaryToAscii((binary >> 8) & 0x0F);
	p_ascii[2] = _BinaryToAscii((binary >> 4) & 0x0F);
	p_ascii[3] = _BinaryToAscii(binary & 0x0F);
}

uint8_t pifModbusAscii_CalcLrc(uint8_t *p_data, uint16_t length)
{
	uint8_t lrc = 0;
	int i;

	for (i = 0; i < length; i++)
		lrc += p_data[i];

	return 0x100 - lrc;
}

uint8_t pifModbusAscii_CalcLrcOfAscii(uint8_t *p_ascii, uint16_t length)
{
	uint8_t lrc = 0;
	int i;

	// The LRC is over the bytes the hex digits stand for, not over the digits themselves.
	for (i = 0; i + 1 < length; i += 2)
		lrc += (uint8_t)pifModbusAscii_AsciiToChar(&p_ascii[i]);

	return 0x100 - lrc;
}

int16_t pifModbusAscii_Decode(uint8_t *p_buffer, uint16_t length)
{
	uint16_t i;
	short value;

	if (length & 1) return -1;

	for (i = 0; i < length; i += 2) {
		value = pifModbusAscii_AsciiToChar(&p_buffer[i]);
		if (value < 0) return -1;
		p_buffer[i / 2] = value;
	}
	return length / 2;
}
