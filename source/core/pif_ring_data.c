// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_ring_data.h"


// Ring container for fixed-size records with FIFO add/remove operations.
// One extra slot is allocated internally so that all data_count items are usable.

// Slot after head, or slot_count when the ring is full.
static uint16_t _nextHead(PifRingData* p_owner)
{
	uint16_t next = p_owner->__head + 1;

	// One spare slot keeps head/tail unambiguous.
	if (next >= p_owner->__slot_count) next = 0;
	if (next == p_owner->__tail) return p_owner->__slot_count;
	return next;
}


PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count)
{
	PifRingData* p_owner = calloc(1, sizeof(PifRingData));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

	if (!pifRingData_Init(p_owner, id, data_size, data_count)) {
		pifRingData_Destroy(&p_owner);
	    return NULL;
	}
    return p_owner;
}

void pifRingData_Destroy(PifRingData** pp_owner)
{
	if (*pp_owner) {
		pifRingData_Clear(*pp_owner);
        free(*pp_owner);
        *pp_owner = NULL;
    }
}

BOOL pifRingData_Init(PifRingData* p_owner, PifId id, uint16_t data_size, uint16_t data_count)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRingData));

    if (!data_size || !data_count || data_count == UINT16_MAX) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	p_owner->__slot_count = data_count + 1;
	p_owner->__p_data = calloc(data_size, p_owner->__slot_count);
	if (!p_owner->__p_data) {
		pif_error = E_OUT_OF_HEAP;
	    goto fail;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_data_size = data_size;
    p_owner->_data_count = data_count;
    return TRUE;

fail:
	pifRingData_Clear(p_owner);
    return FALSE;
}

void pifRingData_Clear(PifRingData* p_owner)
{
	if (p_owner->__p_data) {
        free(p_owner->__p_data);
        p_owner->__p_data = NULL;
    }
	p_owner->__head = 0;
	p_owner->__tail = 0;
	p_owner->__index = 0;
	p_owner->__slot_count = 0;
}

void pifRingData_Reset(PifRingData* p_owner)
{
	p_owner->__head = 0;
	p_owner->__tail = 0;
	p_owner->__index = 0;
}

BOOL pifRingData_IsEmpty(PifRingData* p_owner)
{
	return p_owner->__head == p_owner->__tail;
}

void* pifRingData_GetData(PifRingData* p_owner, uint16_t index)
{
	// Index is counted from the oldest item (tail).
	if (index >= pifRingData_GetFillSize(p_owner)) return NULL;
	uint32_t pos = (uint32_t)p_owner->__tail + index;
	if (pos >= p_owner->__slot_count) pos -= p_owner->__slot_count;
	return p_owner->__p_data + (pos * p_owner->_data_size);
}

void* pifRingData_GetFirstData(PifRingData* p_owner)
{
	// Prepare iteration from the current tail element.
	p_owner->__index = p_owner->__tail;
	if (p_owner->__head == p_owner->__tail) return NULL;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

void* pifRingData_GetNextData(PifRingData* p_owner)
{
	// Iterate circularly until the head position is reached, then stay there.
	if (p_owner->__index == p_owner->__head) return NULL;
	p_owner->__index++;
	if (p_owner->__index >= p_owner->__slot_count) p_owner->__index = 0;
	if (p_owner->__index == p_owner->__head) return NULL;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

uint16_t pifRingData_GetFillSize(PifRingData* p_owner)
{
	// Read once each, since the other side may move one of them in between.
	uint16_t head = p_owner->__head;
	uint16_t tail = p_owner->__tail;

	if (head >= tail) {
    	return head - tail;
    }
    else {
    	return p_owner->__slot_count - tail + head;
    }
}

uint16_t pifRingData_GetRemainSize(PifRingData* p_owner)
{
	return p_owner->_data_count - pifRingData_GetFillSize(p_owner);
}

void* pifRingData_Add(PifRingData* p_owner)
{
	uint8_t* p_data = pifRingData_BeginAdd(p_owner);

	if (p_data) pifRingData_CommitAdd(p_owner);
	return p_data;
}

void* pifRingData_BeginAdd(PifRingData* p_owner)
{
	if (!p_owner->__p_data) {
		pif_error = E_INVALID_STATE;
		return NULL;
	}

	if (_nextHead(p_owner) == p_owner->__slot_count) {
		pif_error = E_OVERFLOW_BUFFER;
		return NULL;
	}
	return p_owner->__p_data + (p_owner->__head * p_owner->_data_size);
}

void pifRingData_CommitAdd(PifRingData* p_owner)
{
	uint16_t next = _nextHead(p_owner);

	// Written once with a value in range, since the reader may be looking at it.
	if (next != p_owner->__slot_count) p_owner->__head = next;
}

void* pifRingData_Remove(PifRingData* p_owner)
{
	uint16_t tail = p_owner->__tail;
	uint16_t next;

	// Pop from tail in FIFO order.
	if (p_owner->__head == tail) {
		pif_error = E_EMPTY_IN_BUFFER;
		return NULL;
	}

	uint8_t* p_data = p_owner->__p_data + (tail * p_owner->_data_size);
	next = tail + 1;
	if (next >= p_owner->__slot_count) next = 0;
	// Written once with a value in range, since the writer may be looking at it.
	p_owner->__tail = next;
	return p_data;
}
