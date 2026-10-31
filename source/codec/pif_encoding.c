// SPDX-License-Identifier: BSD-3-Clause
#include "codec/pif_encoding.h"

#include <string.h>

// Compact integer encodings (zigzag, base-128 varint), float bit access and COBS framing.

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

// COBS: each block is a code byte n and n - 1 data bytes, and stands for those bytes followed by a 0x00,
// which is left out after the last block and after a block of 0xFF, the longest one.
static void _cobsAppend(PifCobsDecoder *p_owner, uint8_t data)
{
	if (p_owner->__length >= p_owner->__size) {
		p_owner->__error = TRUE;
		return;
	}
	p_owner->__p_buffer[p_owner->__length++] = data;
}

uint16_t pifEncoding_CobsEncode(const uint8_t *p_src, uint16_t length, uint8_t *p_dst, uint16_t size)
{
	uint16_t i, code_pos = 0, pos = 1;
	uint8_t code = 1;

	if (!size) return 0;
	for (i = 0; i < length; i++) {
		if (p_src[i]) {
			if (pos >= size) return 0;
			p_dst[pos++] = p_src[i];
			code++;
		}
		if (!p_src[i] || code == 0xFF) {
			p_dst[code_pos] = code;
			code = 1;
			code_pos = pos;
			// A full block at the very end needs no empty block after it.
			if (!p_src[i] || i + 1 < length) {
				if (pos >= size) return 0;
				pos++;
			}
		}
	}
	p_dst[code_pos] = code;
	return pos;
}

BOOL pifEncoding_CobsDecode(const uint8_t *p_src, uint16_t length, uint8_t *p_dst, uint16_t size,
		uint16_t *p_length)
{
	uint16_t i = 0, out = 0;
	uint8_t code, n;

	while (i < length) {
		code = p_src[i++];
		if (!code || code - 1 > length - i) return FALSE;
		for (n = 1; n < code; n++) {
			if (!p_src[i] || out >= size) return FALSE;
			p_dst[out++] = p_src[i++];
		}
		if (code < 0xFF && i < length) {
			if (out >= size) return FALSE;
			p_dst[out++] = 0;
		}
	}
	*p_length = out;
	return TRUE;
}

void pifEncoding_CobsDecoderInit(PifCobsDecoder *p_owner, uint8_t *p_buffer, uint16_t size)
{
	p_owner->__p_buffer = p_buffer;
	p_owner->__size = size;
	pifEncoding_CobsDecoderReset(p_owner);
}

void pifEncoding_CobsDecoderReset(PifCobsDecoder *p_owner)
{
	p_owner->__length = 0;
	p_owner->__code = 0;
	p_owner->__remain = 0;
	p_owner->__error = FALSE;
}

int pifEncoding_CobsDecoderPut(PifCobsDecoder *p_owner, uint8_t data)
{
	int result;

	if (!data) {
		if (!p_owner->__code) result = PIF_COBS_MORE;
		else if (p_owner->__error || p_owner->__remain) result = PIF_COBS_ERROR;
		else result = p_owner->__length;
		pifEncoding_CobsDecoderReset(p_owner);
		return result;
	}
	if (p_owner->__error) return PIF_COBS_MORE;	// Dropped up to the next 0x00

	if (p_owner->__remain) {
		_cobsAppend(p_owner, data);
		p_owner->__remain--;
	}
	else {
		// A new block: the one before it stood for a 0x00 too, unless it was full.
		if (p_owner->__code && p_owner->__code < 0xFF) _cobsAppend(p_owner, 0);
		p_owner->__code = data;
		p_owner->__remain = data - 1;
	}
	return PIF_COBS_MORE;
}
