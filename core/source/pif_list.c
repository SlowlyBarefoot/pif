#include "pif_list.h"


// ---------- PIF Singly Linked List ----------

PifSList* pifSList_Create()
{
	PifSList* p_owner = calloc(sizeof(PifSList), 1);
	if (p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}
	return p_owner;
}

void pifSList_Destroy(PifSList** pp_owner)
{
	if (*pp_owner) {
		pifSList_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifSList_Init(PifSList* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSList));
	return TRUE;
}

void pifSList_Clear(PifSList* p_owner)
{
	PifSListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
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

PifDList* pifDList_Create()
{
	PifDList* p_owner = calloc(sizeof(PifDList), 1);
	if (p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}
	return p_owner;
}

void pifDList_Destroy(PifDList** pp_owner)
{
	if (*pp_owner) {
		pifDList_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifDList_Init(PifDList* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

    memset(p_owner, 0, sizeof(PifDList));
	return TRUE;
}

void pifDList_Clear(PifDList* p_owner)
{
	PifDListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
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

void pifDList_Remove(PifDList* p_owner, PifDListIterator it)
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
