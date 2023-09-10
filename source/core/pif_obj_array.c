#include "core/pif_obj_array.h"


BOOL pifObjArray_Init(PifObjArray* p_owner, int size, int max_count, PifEvtObjArrayClear evt_clear)
{
	char* p_buffer;
	PifObjArrayIterator p_node;

	if (!p_owner || !size || !max_count) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_buffer = calloc(2 * sizeof(PifObjArrayIterator) + size, max_count);
	if (!p_buffer) goto fail;

	p_owner->p_node = (PifObjArrayIterator)p_buffer;
	p_owner->size = size;
	p_owner->max_count = max_count;
	p_owner->count = 0;

	p_owner->p_first = NULL;

	p_node = p_owner->p_node;
	p_owner->p_free = p_node;
	for (int i = 1; i < max_count; i++) {
		p_buffer += 2 * sizeof(PifObjArrayIterator) + size;
		p_node->p_next = (PifObjArrayIterator)p_buffer;
		p_node->p_prev = NULL;
		p_node = (PifObjArrayIterator)p_buffer;
	}
	p_node->p_next = NULL;
	p_node->p_prev = NULL;

	p_owner->evt_clear = evt_clear;
	return TRUE;

fail:
	pifObjArray_Clear(p_owner);
	return FALSE;
}

void pifObjArray_Clear(PifObjArray* p_owner)
{
	if (p_owner->p_node) {
		if (p_owner->evt_clear) {
			PifObjArrayIterator it = p_owner->p_first;
			while (it) {
				(*p_owner->evt_clear)(it);
				it = it ? it->p_next : NULL;
			}
		}

		free(p_owner->p_node);
		p_owner->p_node = NULL;
	}

	p_owner->size = 0;
	p_owner->max_count = 0;
}

PifObjArrayIterator pifObjArray_Add(PifObjArray* p_owner)
{
	if (p_owner->p_free == NULL) return NULL;

	PifObjArrayIterator p_node = p_owner->p_free;
	p_owner->p_free = p_node->p_next;

	p_node->p_next = p_owner->p_first;
	if (p_owner->p_first) {
		p_owner->p_first->p_prev = p_node;
	}
	p_owner->p_first = p_node;
	p_owner->count++;

	memset(&p_node->data, 0, p_owner->size);
    return p_node;
}

void pifObjArray_Remove(PifObjArray* p_owner, void* p_data)
{
	PifObjArrayIterator p_node = p_owner->p_first;

	while (p_node) {
		if (p_data == p_node->data) break;
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

PIF_INLINE int pifObjArray_Count(PifObjArray* p_owner)
{
	return p_owner->count;
}

PIF_INLINE PifObjArrayIterator pifObjArray_Begin(PifObjArray* p_owner)
{
	return p_owner->p_first;
}

PIF_INLINE PifObjArrayIterator pifObjArray_Next(PifObjArrayIterator it)
{
	return it ? it->p_next : NULL;
}
