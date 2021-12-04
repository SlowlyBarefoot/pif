#include "pif_list.h"


// ---------- PIF Singly Linked List ----------

BOOL pifSList_Init(PifSList* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSList));
	return TRUE;
}

void pifSList_Clear(PifSList* p_owner, PifEvtSListClear* evt_clear)
{
	PifSListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
    	if (evt_clear) (*evt_clear)(it->data);

        free(p_owner->p_head);
        p_owner->p_head = it;
    }
    p_owner->p_tail = NULL;
    p_owner->size = 0;
}

void* pifSList_AddFirst(PifSList* p_owner, int data_size)
{
	PifSListNode* p_node = calloc(sizeof(PifSListNode) + data_size, 1);
    if (!p_node) {
    	pif_error = E_OUT_OF_HEAP;
    	return NULL;
    }

    p_node->p_next = p_owner->p_head;
    if (!p_owner->p_head) {
    	p_owner->p_tail = p_node;
    }
    p_owner->p_head = p_node;
    p_owner->size++;
    return p_node->data;
}

void* pifSList_AddLast(PifSList* p_owner, int data_size)
{
	PifSListNode* p_node = calloc(sizeof(PifSListNode) + data_size, 1);
    if (!p_node) {
    	pif_error = E_OUT_OF_HEAP;
    	return NULL;
    }

    if (!p_owner->p_head) {
    	p_node->p_next = p_owner->p_head;
        p_owner->p_head = p_node;
    }
    else {
    	p_node->p_next = p_owner->p_tail->p_next;
        p_owner->p_tail->p_next = p_node;
    }
    p_owner->p_tail = p_node;
    p_owner->size++;
    return p_node->data;
}

void pifSList_RemoveFirst(PifSList* p_owner)
{
	PifSListIterator it;

    if (!p_owner->p_head) return;

    if (p_owner->p_tail == p_owner->p_head) {
    	p_owner->p_tail = NULL;
    }
    it = p_owner->p_head->p_next;
    free(p_owner->p_head);
    p_owner->p_head = it;
    p_owner->size--;
}

#ifdef __PIF_NO_USE_INLINE__

int pifSList_Size(PifSList* p_owner)
{
	return p_owner->size;
}

PifSListIterator pifSList_Begin(PifSList* p_owner)
{
	return p_owner->p_head;
}

PifSListIterator pifSList_End(PifSList* p_owner)
{
	return p_owner->p_tail;
}

PifSListIterator pifSList_Next(PifSListIterator it)
{
	return it ? it->p_next : NULL;
}

#endif

PifSListIterator pifSList_Find(PifSList* p_owner, int index)
{
	PifSListIterator it = p_owner->p_head;
	for (int i = 0; i < index; i++) {
		if (!it) return NULL;

		it = it->p_next;
	}
	return it;
}


// ---------- PIF Doubly Linked List ----------

BOOL pifDList_Init(PifDList* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDList));
	return TRUE;
}

void pifDList_Clear(PifDList* p_owner, PifEvtDListClear* evt_clear)
{
	PifDListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
    	if (evt_clear) (*evt_clear)(it->data);

    	free(p_owner->p_head);
        p_owner->p_head = it;
    }
    p_owner->p_tail = NULL;
    p_owner->size = 0;
}

void* pifDList_AddFirst(PifDList* p_owner, int data_size)
{
	PifDListNode* p_node = calloc(sizeof(PifDListNode) + data_size, 1);
    if (!p_node) {
    	pif_error = E_OUT_OF_HEAP;
    	return NULL;
    }

    p_node->p_prev = NULL;
    p_node->p_next = p_owner->p_head;
    if (!p_owner->p_head) {
    	p_owner->p_tail = p_node;
    }
    else {
    	p_owner->p_head->p_prev = p_node;
    }
    p_owner->p_head = p_node;
    p_owner->size++;
    return p_node->data;
}

void* pifDList_AddLast(PifDList* p_owner, int data_size)
{
	PifDListNode* p_node = calloc(sizeof(PifDListNode) + data_size, 1);
    if (!p_node) {
    	pif_error = E_OUT_OF_HEAP;
    	return NULL;
    }

    if (!p_owner->p_head) {
    	p_node->p_prev = NULL;
    	p_node->p_next = p_owner->p_head;
        p_owner->p_head = p_node;
    }
    else {
    	p_node->p_prev = p_owner->p_tail;
    	p_node->p_next = p_owner->p_tail->p_next;
        p_owner->p_tail->p_next = p_node;
    }
    p_owner->p_tail = p_node;
    p_owner->size++;
    return p_node->data;
}

void* pifDList_Add(PifDList* p_owner, int data_size, PifDListIterator it)
{
    if (!it) {
        return pifDList_AddLast(p_owner, data_size);
    }

    PifDListNode* p_node = calloc(sizeof(PifDListNode) + data_size, 1);
    if (!p_node) {
    	pif_error = E_OUT_OF_HEAP;
    	return NULL;
    }

    p_node->p_prev = it->p_prev;
    p_node->p_next = it;
    if (it->p_prev) {
        it->p_prev->p_next = p_node;
    }
    it->p_prev = p_node;
    p_owner->size++;
    return p_node->data;
}

void pifDList_RemoveFirst(PifDList* p_owner)
{
	PifDListIterator it_prev;
	PifDListIterator it_next;

    if (!p_owner->p_head) return;

    if (p_owner->p_tail == p_owner->p_head) {
    	p_owner->p_tail = NULL;
    }
    it_prev = p_owner->p_head->p_prev;
    it_next = p_owner->p_head->p_next;
    free(p_owner->p_head);
    p_owner->p_head = it_next;
    if (it_next) {
        it_next->p_prev = it_prev;
    }
    p_owner->size--;
}

void pifDList_RemoveLast(PifDList* p_owner)
{
	PifDListIterator it_prev;
	PifDListIterator it_next;

    if (!p_owner->p_head) return;

    it_prev = p_owner->p_tail->p_prev;
    it_next = p_owner->p_tail->p_next;
    free(p_owner->p_tail);
    if (it_prev) {
        it_prev->p_next = it_next;
    }
    p_owner->p_tail = it_prev;
    p_owner->size--;
}

void pifDList_RemoveIterator(PifDList* p_owner, PifDListIterator it)
{
	PifDListIterator it_prev;
	PifDListIterator it_next;

    if (!it) return;

    it_prev = it->p_prev;
    it_next = it->p_next;
    free(it);
    if (it_prev) {
        it_prev->p_next = it_next;
    }
    else {
    	p_owner->p_head = it_next;
    }
    if (it_next) {
        it_next->p_prev = it_prev;
    }
    else {
    	p_owner->p_tail = it_prev;
    }
    p_owner->size--;
}

void pifDList_Remove(PifDList* p_owner, void* p_data)
{
	PifDListIterator it = pifDList_Begin(p_owner);
	while (it) {
		if (it->data == p_data) {
			pifDList_RemoveIterator(p_owner, it);
			return;
		}
		it = pifDList_Next(it);
	}
}

#ifdef __PIF_NO_USE_INLINE__

int pifDList_Size(PifDList* p_owner)
{
	return p_owner->size;
}

PifDListIterator pifDList_Begin(PifDList* p_owner)
{
	return p_owner->p_head;
}

PifDListIterator pifDList_Next(PifDListIterator it)
{
	return it ? it->p_next : NULL;
}

PifDListIterator pifDList_End(PifDList* p_owner)
{
	return p_owner->p_tail;
}

PifDListIterator pifDList_Prev(PifDListIterator it)
{
	return it ? it->p_prev : NULL;
}

#endif

PifDListIterator pifDList_Find(PifDList* p_owner, int index)
{
	PifDListIterator it = p_owner->p_head;
	for (int i = 0; i < index; i++) {
		if (!it) return NULL;

		it = it->p_next;
	}
	return it;
}

// ---------- PIF Fixed Linked List ----------

BOOL pifFixList_Init(PifFixList* p_owner, int size, int max_count)
{
	char* p_buffer;
	PifFixListIterator p_node;

	if (!p_owner || !size || !max_count) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	p_buffer = calloc(2 * sizeof(PifFixListIterator) + size, max_count);
	if (!p_buffer) goto fail;

	p_owner->p_node = (PifFixListIterator)p_buffer;
	p_owner->size = size;
	p_owner->max_count = max_count;
	p_owner->count = 0;

	p_owner->p_first = NULL;

	p_node = p_owner->p_node;
	p_owner->p_free = p_node;
	for (int i = 1; i < max_count; i++) {
		p_buffer += 2 * sizeof(PifFixListIterator) + size;
		p_node->p_next = (PifFixListIterator)p_buffer;
		p_node->p_prev = NULL;
		p_node = (PifFixListIterator)p_buffer;
	}
	p_node->p_next = NULL;
	p_node->p_prev = NULL;

	return TRUE;

fail:
	pifFixList_Clear(p_owner, NULL);
	return FALSE;
}

void pifFixList_Clear(PifFixList* p_owner, PifEvtFixListClear evt_clear)
{
	if (p_owner->p_node) {
		if (evt_clear) {
			PifFixListIterator it = p_owner->p_first;
			while (it) {
				(*evt_clear)(it->data);
				it = it ? it->p_next : NULL;
			}
		}

		free(p_owner->p_node);
		p_owner->p_node = NULL;
	}

	p_owner->size = 0;
	p_owner->max_count = 0;
}

void* pifFixList_AddFirst(PifFixList* p_owner)
{
	if (p_owner->p_free == NULL) return NULL;

	PifFixListIterator p_node = p_owner->p_free;
	p_owner->p_free = p_node->p_next;

	p_node->p_next = p_owner->p_first;
	if (p_owner->p_first) {
		p_owner->p_first->p_prev = p_node;
	}
	p_owner->p_first = p_node;
	p_owner->count++;
    return (char*)p_node + 2 * sizeof(PifFixListIterator);
}

void pifFixList_Remove(PifFixList* p_owner, void* p_data)
{
	PifFixListIterator p_node = p_data - 2 * sizeof(PifFixListIterator);

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

int pifFixList_Count(PifFixList* p_owner)
{
	return p_owner->count;
}

PifFixListIterator pifFixList_Begin(PifFixList* p_owner)
{
	return p_owner->p_first;
}

PifFixListIterator pifFixList_Next(PifFixListIterator it)
{
	return it ? it->p_next : NULL;
}

#endif
