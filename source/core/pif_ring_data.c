#include "core/pif_ring_data.h"

// Ring container for fixed-size records with FIFO add/remove operations.

PifRingData* pifRingData_Create(PifId id, uint16_t data_size, uint16_t data_count)
{
	PifRingData* p_owner = malloc(sizeof(PifRingData));
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
    if (!p_owner || !data_size || !data_count) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifRingData));

	p_owner->__p_data = calloc(data_size, data_count);
	if (!p_owner) {
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
}

BOOL pifRingData_IsEmpty(PifRingData* p_owner)
{
	return p_owner->__head == p_owner->__tail;
}

void* pifRingData_GetData(PifRingData* p_owner, uint16_t index)
{
	return p_owner->__p_data + (index * p_owner->_data_size);
}

void* pifRingData_GetFirstData(PifRingData* p_owner)
{
	// Prepare iteration from the current tail element.
	if (p_owner->__head == p_owner->__tail) return NULL;
	p_owner->__index = p_owner->__tail;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

void* pifRingData_GetNextData(PifRingData* p_owner)
{
	// Iterate circularly until the head position is reached.
	p_owner->__index++;
	if (p_owner->__index >= p_owner->_data_count) p_owner->__index = 0;
	if (p_owner->__index == p_owner->__head) return NULL;
	return p_owner->__p_data + (p_owner->__index * p_owner->_data_size);
}

uint16_t pifRingData_GetFillSize(PifRingData* p_owner)
{
	if (p_owner->__head > p_owner->__tail) {
    	return p_owner->__head - p_owner->__tail;
    }
    else {
    	return p_owner->_data_count - p_owner->__tail + p_owner->__head;
    }
}

uint16_t pifRingData_GetRemainSize(PifRingData* p_owner)
{
	uint16_t usRemain;

    if (p_owner->__head < p_owner->__tail) {
    	usRemain = p_owner->__tail - p_owner->__head;
    }
    else {
    	usRemain = p_owner->_data_count - p_owner->__head + p_owner->__tail;
    }
    return usRemain - 1;
}

void* pifRingData_Add(PifRingData* p_owner)
{
	uint16_t next =	p_owner->__head + 1;

	// Keep one slot reserved to prevent head/tail ambiguity.
	if (next >= p_owner->_data_count) next = 0;
	if (next == p_owner->__tail) {
		pif_error = E_OVERFLOW_BUFFER;
		return NULL;
	}

	uint8_t* p_data = p_owner->__p_data + (p_owner->__head * p_owner->_data_size);
	p_owner->__head = next;
	return p_data;
}

void* pifRingData_Remove(PifRingData* p_owner)
{
	// Pop from tail in FIFO order.
	if (p_owner->__head == p_owner->__tail) {
		pif_error = E_EMPTY_IN_BUFFER;
		return NULL;
	}

	uint8_t* p_data = p_owner->__p_data + (p_owner->__tail * p_owner->_data_size);
	p_owner->__tail++;
	if (p_owner->__tail >= p_owner->_data_count) p_owner->__tail = 0;
	return p_data;
}
