// SPDX-License-Identifier: BSD-3-Clause
#include "codec/pif_encoding.h"

#include <string.h>

// Compact integer encodings (zigzag, base-128 varint) and float bit access.

uint32_t pifEncoding_ZigzagEncode(int32_t value)
{
	// Shift the magnitude up one bit and put the sign in bit 0.
	return ((uint32_t)value << 1) ^ (value < 0 ? 0xFFFFFFFFUL : 0UL);
}

int32_t pifEncoding_ZigzagDecode(uint32_t value)
{
	return (int32_t)((value >> 1) ^ (0UL - (value & 1)));
}

uint8_t pifEncoding_UvarintEncode(uint32_t value, uint8_t* p_buffer, uint16_t size)
{
	uint8_t count = 0;

	do {
		if (count >= size) return 0;
		p_buffer[count] = (uint8_t)(value & 0x7F);
		value >>= 7;
		if (value) p_buffer[count] |= 0x80;
		count++;
	} while (value);
	return count;
}

uint8_t pifEncoding_UvarintDecode(uint32_t* p_value, const uint8_t* p_buffer, uint16_t size)
{
	uint32_t value = 0;
	uint8_t i;

	for (i = 0; i < PIF_UVARINT32_MAX_SIZE && i < size; i++) {
		// The fifth byte holds bits 28-31, so only its low 4 bits may be set.
		if (i == PIF_UVARINT32_MAX_SIZE - 1 && (p_buffer[i] & 0xF0)) return 0;

		value |= (uint32_t)(p_buffer[i] & 0x7F) << (7 * i);
		if (!(p_buffer[i] & 0x80)) {
			*p_value = value;
			return i + 1;
		}
	}
	return 0;
}

uint32_t pifEncoding_FloatToBits(float value)
{
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	return bits;
}

float pifEncoding_BitsToFloat(uint32_t bits)
{
	float value;

	memcpy(&value, &bits, sizeof(value));
	return value;
}
