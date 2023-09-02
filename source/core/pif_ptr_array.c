#include "core/pif_ptr_array.h"


BOOL pifPtrArray_Init(PifPtrArray* p_owner, int max_count, PifEvtPtrArrayClear evt_clear)
{
	char* p_buffer;
	PifPtrArrayIterator p_node;

	if (!p_owner || !max_count) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_buffer = calloc(sizeof(PifPtrArrayNode), max_count);
	if (!p_buffer) goto fail;

	p_owner->p_node = (PifPtrArrayIterator)p_buffer;
	p_owner->max_count = max_count;
	p_owner->count = 0;

	p_owner->p_first = NULL;

	p_node = p_owner->p_node;
	p_owner->p_free = p_node;
	for (int i = 1; i < max_count; i++) {
		p_buffer += sizeof(PifPtrArrayNode);
		p_node->p_next = (PifPtrArrayIterator)p_buffer;
		p_node->p_prev = NULL;
		p_node = (PifPtrArrayIterator)p_buffer;
	}
	p_node->p_next = NULL;
	p_node->p_prev = NULL;

	p_owner->evt_clear = evt_clear;
	return TRUE;

fail:
	pifPtrArray_Clear(p_owner);
	return FALSE;
}

void pifPtrArray_Clear(PifPtrArray* p_owner)
{
	if (p_owner->p_node) {
		if (p_owner->evt_clear) {
			PifPtrArrayIterator it = p_owner->p_first;
			while (it) {
				(*p_owner->evt_clear)(it);
				it = it ? it->p_next : NULL;
			}
		}

		free(p_owner->p_node);
		p_owner->p_node = NULL;
	}

	p_owner->max_count = 0;
}

PifPtrArrayIterator pifPtrArray_Add(PifPtrArray* p_owner)
{
	if (p_owner->p_free == NULL) return NULL;

	PifPtrArrayIterator p_node = p_owner->p_free;
	p_owner->p_free = p_node->p_next;

	p_node->p_next = p_owner->p_first;
	if (p_owner->p_first) {
		p_owner->p_first->p_prev = p_node;
	}
	p_owner->p_first = p_node;
	p_owner->count++;
    return p_node;
}

void pifPtrArray_Remove(PifPtrArray* p_owner, void* p_data)
{
	PifPtrArrayIterator p_node = p_owner->p_first;

	while (p_node) {
		if (p_data == p_node->p_data) break;
		p_node = p_node ? p_node->p_next : NULL;
	}
	if (!p_node) return;

	if (p_owner->evt_clear) (*p_owner->evt_clear)(p_node);

	if (p_node->p_prev) {
		p_node->p_prev->p_next = p_node->p_next;
	}
	else {
		p_owner->p_first = p_node->p_next;
	}
	if (p_node->p_next) {
		p_node->p_next->p_prev = p_node->p_prev;
	}
	p_node->p_next = p_owner->p_free;
	p_node->p_prev = NULL;
	p_owner->p_free = p_node;

	p_owner->count--;
}

#ifdef __PIF_NO_USE_INLINE__

int pifPtrArray_Count(PifPtrArray* p_owner)
{
	return p_owner->count;
}

PifPtrArrayIterator pifPtrArray_Begin(PifPtrArray* p_owner)
{
	return p_owner->p_first;
}

PifPtrArrayIterator pifPtrArray_Next(PifPtrArrayIterator it)
{
	return it ? it->p_next : NULL;
}

#endif
