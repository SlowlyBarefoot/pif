#include "core/pif_obj_array.h"

// Fixed-capacity object array with contiguous storage and iterator access.

BOOL pifObjArray_Init(PifObjArray* p_owner, int size, int max_count, PifEvtObjArrayClear evt_clear)
{
	char* p_buffer;
	int tmp, len = sizeof(PifObjArrayIterator);
	PifObjArrayIterator p_node;

	if (!p_owner || !size || !max_count) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	if (size < len) size = len;
	else {
		tmp = size % len;
		if (tmp) size += len - tmp;
	}

	p_buffer = calloc(2 * len + size, max_count);
	if (!p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_owner->_p_node = (PifObjArrayIterator)p_buffer;
	p_owner->_size = size;
	p_owner->_max_count = max_count;
	p_owner->_count = 0;

	p_owner->_p_first = NULL;

	p_node = p_owner->_p_node;
	p_owner->_p_free = p_node;
	for (int i = 1; i < max_count; i++) {
		p_buffer += 2 * len + size;
		p_node->p_next = (PifObjArrayIterator)p_buffer;
		p_node->p_prev = NULL;
		p_node = (PifObjArrayIterator)p_buffer;
	}
	p_node->p_next = NULL;
	p_node->p_prev = NULL;

	p_owner->__evt_clear = evt_clear;
	return TRUE;
}

void pifObjArray_Clear(PifObjArray* p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_node) {
		if (p_owner->__evt_clear) {
			PifObjArrayIterator it = p_owner->_p_first;
			while (it) {
				(*p_owner->__evt_clear)(it);
				it = it ? it->p_next : NULL;
			}
		}

		free(p_owner->_p_node);
		p_owner->_p_node = NULL;
	}

	p_owner->_size = 0;
	p_owner->_max_count = 0;
	p_owner->_count = 0;
}

PifObjArrayIterator pifObjArray_Add(PifObjArray* p_owner)
{
	if (!p_owner) return NULL;

	if (p_owner->_p_free == NULL) return NULL;

	PifObjArrayIterator p_node = p_owner->_p_free;
	p_owner->_p_free = p_node->p_next;

	p_node->p_next = p_owner->_p_first;
	p_node->p_prev = NULL;
	if (p_owner->_p_first) {
		p_owner->_p_first->p_prev = p_node;
	}
	p_owner->_p_first = p_node;
	p_owner->_count++;

	memset(&p_node->data, 0, p_owner->_size);
    return p_node;
}

void pifObjArray_Remove(PifObjArray* p_owner, void* p_data)
{
	PifObjArrayIterator p_node;
	
	if (!p_data) return;

	p_node = (PifObjArrayIterator)((char *)p_data - 2 * sizeof(PifObjArrayIterator));

	if (p_owner->__evt_clear) (*p_owner->__evt_clear)(p_node);

	if (p_node->p_prev) {
		p_node->p_prev->p_next = p_node->p_next;
	}
	else {
		p_owner->_p_first = p_node->p_next;
	}
	if (p_node->p_next) {
		p_node->p_next->p_prev = p_node->p_prev;
	}
	p_node->p_next = p_owner->_p_free;
	p_node->p_prev = NULL;
	p_owner->_p_free = p_node;

	p_owner->_count--;
}
