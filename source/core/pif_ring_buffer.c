#include "core/pif_ring_buffer.h"

#include <string.h>


static BOOL _chopOff(PifRingBuffer* p_owner, uint16_t count)
{
	uint16_t length;
	uint16_t size, tail;

	switch (p_owner->_bt.chop_off) {
	case RB_CHOP_OFF_CHAR:
		size = 0;
		tail = p_owner->__tail;
		while (tail != p_owner->__head) {
			if (p_owner->__p_buffer[tail] == p_owner->__ui.chop_off_char) {
				if (size > count) {
					p_owner->__tail = tail;
					return TRUE;
				}
			}
			tail++;
			if (tail >= p_owner->_size) tail -= p_owner->_size;
			size++;
		}
		break;

	case RB_CHOP_OFF_LENGTH:
		length = pifRingBuffer_GetFillSize(p_owner);
		size = p_owner->__ui.chop_off_length;
		while (count > size) {
			size += p_owner->__ui.chop_off_length;
		}
		if (size < length) {
			p_owner->__tail += size;
			if (p_owner->__tail >= p_owner->_size) p_owner->__tail -= p_owner->_size;
			return TRUE;
		}
		else if (count <= length) {
			p_owner->__tail = p_owner->__head;
			return TRUE;
		}
		break;
	}
	return FALSE;
}

PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size)
{
	PifRingBuffer* p_owner = malloc(sizeof(PifRingBuffer));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifRingBuffer_InitHeap(p_owner, id, size)) {
		pifRingBuffer_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer)
{
	PifRingBuffer* p_owner = calloc(sizeof(PifRingBuffer), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifRingBuffer_InitStatic(p_owner, id, size, p_buffer)) {
		pifRingBuffer_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifRingBuffer_Destroy(PifRingBuffer** pp_owner)
{
	if (pp_owner) {
		pifRingBuffer_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size)
{
    if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRingBuffer));

	p_owner->__p_buffer = calloc(sizeof(uint8_t), size);
	if (!p_owner->__p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
    p_owner->_size = size;
	p_owner->__backup_head = size;
    return TRUE;

fail:
	pifRingBuffer_Clear(p_owner);
    return FALSE;
}

BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer)
{
    if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRingBuffer));

	p_owner->__p_buffer = p_buffer;

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
    p_owner->_bt.is_static = TRUE;
    p_owner->_size = size;
	p_owner->__backup_head = size;
    return TRUE;
}

void pifRingBuffer_Clear(PifRingBuffer* p_owner)
{
	if (p_owner->_bt.is_static == FALSE && p_owner->__p_buffer) {
        free(p_owner->__p_buffer);
    }
    p_owner->__p_buffer = NULL;
}

void pifRingBuffer_Empty(PifRingBuffer* p_owner)
{
	p_owner->__tail = p_owner->__head;
}

BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size)
{
    if (p_owner->_bt.is_static) {
		pif_error = E_INVALID_STATE;
    	return FALSE;
    }

    if (p_owner->__p_buffer) {
    	free(p_owner->__p_buffer);
    	p_owner->__p_buffer = NULL;
    }

	p_owner->__p_buffer = calloc(sizeof(uint8_t), size);
	if (!p_owner->__p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
    p_owner->_size = size;
	return TRUE;
}

void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name)
{
	p_owner->__p_name = p_name;
}

uint8_t *pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos)
{
	return &p_owner->__p_buffer[(p_owner->__tail + pos) % p_owner->_size];
}

BOOL pifRingBuffer_MoveHeadForLinear(PifRingBuffer* p_owner, uint16_t size)
{
	if (size >= p_owner->_size) return FALSE;
	if (p_owner->__head != p_owner->__tail) return FALSE;
	if (p_owner->_size - p_owner->__head < size) p_owner->__head = p_owner->__tail = 0;
	return TRUE;
}

void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_NONE;
}

void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_CHAR;
	p_owner->__ui.chop_off_char = ch;
}

void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_LENGTH;
	p_owner->__ui.chop_off_length = length;
}

BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner)
{
	return p_owner->__p_buffer != NULL;
}

BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner)
{
	return p_owner->__head == p_owner->__tail;
}

uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner)
{
	uint16_t fill;

    if (p_owner->__head >= p_owner->__tail) {
    	fill = p_owner->__head - p_owner->__tail;
    }
    else {
    	fill = p_owner->_size - p_owner->__tail + p_owner->__head;
    }
    return fill;
}

uint16_t pifRingBuffer_GetLinerSize(PifRingBuffer* p_owner, uint16_t pos)
{
	uint16_t tail = (p_owner->__tail + pos) % p_owner->_size;

    if (p_owner->__head >= tail) {
    	return p_owner->__head - tail;
    }
    else {
    	return p_owner->_size - tail;
    }
}

uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner)
{
	uint16_t remain;

    if (p_owner->__head < p_owner->__tail) {
    	remain = p_owner->__tail - p_owner->__head;
    }
    else {
    	remain = p_owner->_size - p_owner->__head + p_owner->__tail;
    }
    return remain - 1;
}

void pifRingBuffer_BeginPutting(PifRingBuffer* p_owner)
{
	if (p_owner->__backup_head < p_owner->_size) {
		p_owner->__head = p_owner->__backup_head;
	}
	p_owner->__backup_head = p_owner->__head;
}

void pifRingBuffer_CommitPutting(PifRingBuffer* p_owner)
{
	p_owner->__backup_head = p_owner->_size;
}

void pifRingBuffer_RollbackPutting(PifRingBuffer* p_owner)
{
	p_owner->__head = p_owner->__backup_head;
	p_owner->__backup_head = p_owner->_size;
}

uint8_t* pifRingBuffer_GetPointerPutting(PifRingBuffer* p_owner, uint16_t pos)
{
	return &p_owner->__p_buffer[(p_owner->__backup_head + pos) % p_owner->_size];
}

BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data)
{
    uint16_t next;

    next = p_owner->__head + 1;
	if (next >= p_owner->_size) next = 0;
    if (next == p_owner->__tail) {
    	if (!_chopOff(p_owner, 1)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    p_owner->__p_buffer[p_owner->__head] = data;
    p_owner->__head = next;
    return TRUE;
}

BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner);

    if (length > remain) {
    	if (!_chopOff(p_owner, length - remain)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < length; i++) {
    	p_owner->__p_buffer[p_owner->__head] = p_data[i];
    	p_owner->__head++;
    	if (p_owner->__head >= p_owner->_size) p_owner->__head = 0;
    }
    return TRUE;
}

BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner);
	uint16_t length = strlen(p_string);

    if (length > remain) {
    	if (!_chopOff(p_owner, length - remain)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    for (uint16_t i = 0; i < length; i++) {
    	p_owner->__p_buffer[p_owner->__head] = p_string[i];
    	p_owner->__head++;
    	if (p_owner->__head >= p_owner->_size) p_owner->__head = 0;
    }
    return TRUE;
}

BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data)
{
	if (p_owner->__tail == p_owner->__head) return FALSE;

	*p_data = p_owner->__p_buffer[p_owner->__tail];
	p_owner->__tail++;
	if (p_owner->__tail >= p_owner->_size) p_owner->__tail = 0;
	return TRUE;
}

uint16_t pifRingBuffer_GetBytes(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t i;

	for (i = 0; i < length; i++) {
		if (p_owner->__tail == p_owner->__head) return i;

		p_data[i] = p_owner->__p_buffer[p_owner->__tail];
		p_owner->__tail++;
		if (p_owner->__tail >= p_owner->_size) p_owner->__tail = 0;
	}
	return i;
}

uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos)
{
	uint16_t tail = p_src->__tail + pos;
	if (tail >= p_src->_size) tail -= p_src->_size;

	for (uint16_t i = 0; i < count; i++) {
		p_dst[i] = p_src->__p_buffer[tail];
		tail++;
		if (tail >= p_src->_size) tail = 0;
		if (tail == p_src->__head) return i + 1;
	}
	return count;
}

uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_src) - pos;
	uint16_t remain = pifRingBuffer_GetRemainSize(p_dst);
	uint16_t length = remain < fill ? remain : fill;

	uint16_t tail = p_src->__tail + pos;
	if (tail >= p_src->_size) tail -= p_src->_size;
	for (uint16_t i = 0; i < length; i++) {
		pifRingBuffer_PutByte(p_dst, p_src->__p_buffer[tail]);
		tail++;
		if (tail >= p_src->_size) tail = 0;
	}
	return length;
}

BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_src);

	if (pos + length > fill) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (pifRingBuffer_GetRemainSize(p_dst) < length) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	uint16_t usTail = (p_src->__tail + pos) % p_src->_size;
	while (usTail != p_src->__head) {
		pifRingBuffer_PutByte(p_dst, p_src->__p_buffer[usTail]);
		usTail++;
		if (usTail >= p_src->_size) usTail = 0;
	}
	return TRUE;
}

void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_owner);

	if (size >= fill) {
		p_owner->__tail = p_owner->__head;
	}
	else {
		p_owner->__tail = (p_owner->__tail + size) % p_owner->_size;
	}
}
