#include "pif_list.h"


// ---------- PIF Singly Linked List ----------

PIF_SList* pifSList_Create()
{
	PIF_SList* p_owner = calloc(sizeof(PIF_SList), 1);
	if (p_owner) {
		pif_enError = E_enOutOfHeap;
		return NULL;
	}
	return p_owner;
}

void pifSList_Destroy(PIF_SList** pp_owner)
{
	if (*pp_owner) {
		pifSList_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifSList_Init(PIF_SList* p_owner)
{
	if (!p_owner) {
        pif_enError = E_enInvalidParam;
        return FALSE;
	}

	p_owner->p_head = NULL;
	p_owner->p_tail = NULL;
	p_owner->size = 0;
	return TRUE;
}

void pifSList_Clear(PIF_SList* p_owner)
{
	PIF_SListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
        free(p_owner->p_head);
        p_owner->p_head = it;
    }
    p_owner->p_tail = NULL;
    p_owner->size = 0;
}

void* pifSList_AddFirst(PIF_SList* p_owner, int data_size)
{
	PIF_SListNode* p_node = calloc(sizeof(PIF_SListNode) + data_size - 1, 1);
    if (!p_node) {
		pif_enError = E_enOutOfHeap;
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

void* pifSList_AddLast(PIF_SList* p_owner, int data_size)
{
	PIF_SListNode* p_node = calloc(sizeof(PIF_SListNode) + data_size - 1, 1);
    if (!p_node) {
		pif_enError = E_enOutOfHeap;
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

void pifSList_RemoveFirst(PIF_SList* p_owner)
{
	PIF_SListIterator it;

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

int pifSList_Size(PIF_SList* p_owner)
{
	return p_owner->size;
}

PIF_SListIterator pifSList_Begin(PIF_SList* p_owner)
{
	return p_owner->p_head;
}

PIF_SListIterator pifSList_End(PIF_SList* p_owner)
{
	return p_owner->p_tail;
}

PIF_SListIterator pifSList_Next(PIF_SListIterator it)
{
	return it ? it->p_next : NULL;
}

#endif

PIF_SListIterator pifSList_Find(PIF_SList* p_owner, int index)
{
	PIF_SListIterator it = p_owner->p_head;
	for (int i = 0; i < index; i++) {
		if (!it) return NULL;

		it = it->p_next;
	}
	return it;
}


// ---------- PIF Doubly Linked List ----------

PIF_DList* pifDList_Create()
{
	PIF_DList* p_owner = calloc(sizeof(PIF_DList), 1);
	if (p_owner) {
		pif_enError = E_enOutOfHeap;
		return NULL;
	}
	return p_owner;
}

void pifDList_Destroy(PIF_DList** pp_owner)
{
	if (*pp_owner) {
		pifDList_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifDList_Init(PIF_DList* p_owner)
{
	if (!p_owner) {
        pif_enError = E_enInvalidParam;
        return FALSE;
	}

	p_owner->p_head = NULL;
	p_owner->p_tail = NULL;
	p_owner->size = 0;
	return TRUE;
}

void pifDList_Clear(PIF_DList* p_owner)
{
	PIF_DListIterator it;

    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
        free(p_owner->p_head);
        p_owner->p_head = it;
    }
    p_owner->p_tail = NULL;
    p_owner->size = 0;
}

void* pifDList_AddFirst(PIF_DList* p_owner, int data_size)
{
	PIF_DListNode* p_node = calloc(sizeof(PIF_DListNode) + data_size - 1, 1);
    if (!p_node) {
		pif_enError = E_enOutOfHeap;
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

void* pifDList_AddLast(PIF_DList* p_owner, int data_size)
{
	PIF_DListNode* p_node = calloc(sizeof(PIF_DListNode) + data_size - 1, 1);
    if (!p_node) {
		pif_enError = E_enOutOfHeap;
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

void* pifDList_Add(PIF_DList* p_owner, int data_size, PIF_DListIterator it)
{
    if (!it) {
        return pifDList_AddLast(p_owner, data_size);
    }

    PIF_DListNode* p_node = calloc(sizeof(PIF_DListNode) + data_size - 1, 1);
    if (!p_node) {
		pif_enError = E_enOutOfHeap;
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

void pifDList_RemoveFirst(PIF_DList* p_owner)
{
	PIF_DListIterator it_prev;
	PIF_DListIterator it_next;

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

void pifDList_RemoveLast(PIF_DList* p_owner)
{
	PIF_DListIterator it_prev;
	PIF_DListIterator it_next;

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

void pifDList_Remove(PIF_DList* p_owner, PIF_DListIterator it)
{
	PIF_DListIterator it_prev;
	PIF_DListIterator it_next;

    if (!it) return;

    it_prev = it->p_prev;
    it_next = it->p_next;
    free(it);
    if (it_prev) {
        it_prev->p_next = it_next;
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

int pifDList_Size(PIF_DList* p_owner)
{
	return p_owner->size;
}

PIF_DListIterator pifDList_Begin(PIF_DList* p_owner)
{
	return p_owner->p_head;
}

PIF_DListIterator pifDList_Next(PIF_DListIterator it)
{
	return it ? it->p_next : NULL;
}

PIF_DListIterator pifDList_End(PIF_DList* p_owner)
{
	return p_owner->p_tail;
}

PIF_DListIterator pifDList_Prev(PIF_DListIterator it)
{
	return it ? it->p_prev : NULL;
}

#endif

PIF_DListIterator pifDList_Find(PIF_DList* p_owner, int index)
{
	PIF_DListIterator it = p_owner->p_head;
	for (int i = 0; i < index; i++) {
		if (!it) return NULL;

		it = it->p_next;
	}
	return it;
}
