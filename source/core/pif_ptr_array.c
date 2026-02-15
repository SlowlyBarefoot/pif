#include "core/pif_ptr_array.h"

// Fixed-capacity pointer array with free-list style allocation tracking.

BOOL pifPtrArray_Init(PifPtrArray* p_owner, int max_count, PifEvtPtrArrayClear evt_clear)
{
	char* p_buffer;
	PifPtrArrayIterator p_node;

	if (!p_owner || !max_count) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_buffer = calloc(sizeof(PifPtrArrayNode), max_count);
	if (!p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	p_owner->_p_node = (PifPtrArrayIterator)p_buffer;
	p_owner->_max_count = max_count;
	p_owner->_count = 0;

	p_owner->_p_first = NULL;

	p_node = p_owner->_p_node;
	p_owner->_p_free = p_node;
	for (int i = 1; i < max_count; i++) {
		p_buffer += sizeof(PifPtrArrayNode);
		p_node->p_next = (PifPtrArrayIterator)p_buffer;
		p_node->p_prev = NULL;
		p_node = (PifPtrArrayIterator)p_buffer;
	}
	p_node->p_next = NULL;
	p_node->p_prev = NULL;

	p_owner->__evt_clear = evt_clear;
	return TRUE;
}

void pifPtrArray_Clear(PifPtrArray* p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_node) {
		if (p_owner->__evt_clear) {
			PifPtrArrayIterator it = p_owner->_p_first;
			while (it) {
				(*p_owner->__evt_clear)(it);
				it = it ? it->p_next : NULL;
			}
		}

		free(p_owner->_p_node);
		p_owner->_p_node = NULL;
	}

	p_owner->_max_count = 0;
	p_owner->_count = 0;
}

PifPtrArrayIterator pifPtrArray_Add(PifPtrArray* p_owner, void *p_data)
{
	if (!p_owner) return NULL;

	if (p_owner->_p_free == NULL) return NULL;

	PifPtrArrayIterator p_node = p_owner->_p_free;
	p_node->p_data = p_data;
	p_owner->_p_free = p_node->p_next;

	p_node->p_next = p_owner->_p_first;
	p_node->p_prev = NULL;
	if (p_owner->_p_first) {
		p_owner->_p_first->p_prev = p_node;
	}
	p_owner->_p_first = p_node;
	p_owner->_count++;
    return p_node;
}

void pifPtrArray_Remove(PifPtrArray* p_owner, PifPtrArrayIterator p_node)
{
	if (!p_owner || !p_node) return;

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

PifPtrArrayIterator pifPtrArray_Find(PifPtrArray* p_owner, void *p_data)
{
	PifPtrArrayIterator p_node;

	if (!p_owner) return NULL;

	p_node = p_owner->_p_first;
	while (p_node) {
		if (p_data == p_node->p_data) return p_node;
		p_node = p_node->p_next;
	}
	return NULL;
}
