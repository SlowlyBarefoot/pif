// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_stream_buffer.h"

#include <string.h>

// Bounds-checked byte cursor for packet serialization.

/**
 * @brief Reserves size bytes at the cursor.
 * @param p_owner Pointer to the target object instance.
 * @param size Number of bytes.
 * @return Pointer to the reserved bytes, or NULL after setting _overflow if they do not fit.
 */
static uint8_t* _take(PifStreamBuffer* p_owner, uint16_t size)
{
	uint8_t* p_start;

	if (pifStreamBuffer_Remaining(p_owner) < size) {
		p_owner->_overflow = TRUE;
		return NULL;
	}
	p_start = p_owner->_p_ptr;
	p_owner->_p_ptr += size;
	return p_start;
}

void pifStreamBuffer_Init(PifStreamBuffer* p_owner, uint8_t* p_buffer, uint16_t size)
{
	p_owner->_p_ptr = p_buffer;
	p_owner->_p_end = p_buffer + size;
	p_owner->_overflow = FALSE;
}

void pifStreamBuffer_SwitchToReader(PifStreamBuffer* p_owner, uint8_t* p_base)
{
	p_owner->_p_end = p_owner->_p_ptr;
	p_owner->_p_ptr = p_base;
}

uint16_t pifStreamBuffer_Remaining(const PifStreamBuffer* p_owner)
{
	return p_owner->_p_end > p_owner->_p_ptr ? (uint16_t)(p_owner->_p_end - p_owner->_p_ptr) : 0;
}

BOOL pifStreamBuffer_Advance(PifStreamBuffer* p_owner, uint16_t size)
{
	return _take(p_owner, size) != NULL;
}

void pifStreamBuffer_WriteU8(PifStreamBuffer* p_owner, uint8_t value)
{
	uint8_t* p = _take(p_owner, 1);

	if (p) p[0] = value;
}

void pifStreamBuffer_WriteU16(PifStreamBuffer* p_owner, uint16_t value)
{
	uint8_t* p = _take(p_owner, 2);

	if (!p) return;
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
}

void pifStreamBuffer_WriteU32(PifStreamBuffer* p_owner, uint32_t value)
{
	uint8_t* p = _take(p_owner, 4);

	if (!p) return;
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

void pifStreamBuffer_WriteU16Be(PifStreamBuffer* p_owner, uint16_t value)
{
	uint8_t* p = _take(p_owner, 2);

	if (!p) return;
	p[0] = (uint8_t)(value >> 8);
	p[1] = (uint8_t)value;
}

void pifStreamBuffer_WriteU32Be(PifStreamBuffer* p_owner, uint32_t value)
{
	uint8_t* p = _take(p_owner, 4);

	if (!p) return;
	p[0] = (uint8_t)(value >> 24);
	p[1] = (uint8_t)(value >> 16);
	p[2] = (uint8_t)(value >> 8);
	p[3] = (uint8_t)value;
}

void pifStreamBuffer_Fill(PifStreamBuffer* p_owner, uint8_t value, uint16_t size)
{
	uint8_t* p = _take(p_owner, size);

	if (p) memset(p, value, size);
}

void pifStreamBuffer_WriteData(PifStreamBuffer* p_owner, const void* p_data, uint16_t size)
{
	uint8_t* p = _take(p_owner, size);

	if (p) memcpy(p, p_data, size);
}

void pifStreamBuffer_WriteString(PifStreamBuffer* p_owner, const char* p_string, BOOL terminator)
{
	size_t length = strlen(p_string) + (terminator ? 1 : 0);

	if (length > 0xFFFF) {
		p_owner->_overflow = TRUE;
		return;
	}
	pifStreamBuffer_WriteData(p_owner, p_string, (uint16_t)length);
}

uint8_t pifStreamBuffer_ReadU8(PifStreamBuffer* p_owner)
{
	uint8_t* p = _take(p_owner, 1);

	return p ? p[0] : 0;
}

uint16_t pifStreamBuffer_ReadU16(PifStreamBuffer* p_owner)
{
	uint8_t* p = _take(p_owner, 2);

	if (!p) return 0;
	return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t pifStreamBuffer_ReadU32(PifStreamBuffer* p_owner)
{
	uint8_t* p = _take(p_owner, 4);

	if (!p) return 0;
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t pifStreamBuffer_ReadU16Be(PifStreamBuffer* p_owner)
{
	uint8_t* p = _take(p_owner, 2);

	if (!p) return 0;
	return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t pifStreamBuffer_ReadU32Be(PifStreamBuffer* p_owner)
{
	uint8_t* p = _take(p_owner, 4);

	if (!p) return 0;
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

BOOL pifStreamBuffer_ReadData(PifStreamBuffer* p_owner, void* p_data, uint16_t size)
{
	uint8_t* p = _take(p_owner, size);

	if (!p) return FALSE;
	memcpy(p_data, p, size);
	return TRUE;
}
