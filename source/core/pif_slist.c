#include "core/pif_slist.h"


BOOL pifSList_Init(PifSList* p_owner)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
        return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSList));
	return TRUE;
}

void pifSList_Clear(PifSList* p_owner, PifEvtSListClear evt_clear)
{
	PifSListIterator it;

	if (!p_owner) return;

	// Release all nodes from head to tail.
    while (p_owner->p_head) {
    	it = p_owner->p_head->p_next;
    	if (evt_clear) (*evt_clear)(p_owner->p_head->data);

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

    p_node->p_next = NULL;
    if (!p_owner->p_head) {
        p_owner->p_head = p_node;
    }
    else {
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

    // Update tail when removing the only node.
    if (p_owner->p_tail == p_owner->p_head) {
    	p_owner->p_tail = NULL;
    }
    it = p_owner->p_head->p_next;
    free(p_owner->p_head);
    p_owner->p_head = it;
    p_owner->size--;
}

PIF_INLINE int pifSList_Size(PifSList* p_owner)
{
	return p_owner->size;
}

PIF_INLINE PifSListIterator pifSList_Begin(PifSList* p_owner)
{
	return p_owner->p_head;
}

PIF_INLINE PifSListIterator pifSList_End(PifSList* p_owner)
{
	return p_owner->p_tail;
}

PIF_INLINE PifSListIterator pifSList_Next(PifSListIterator it)
{
	return it ? it->p_next : NULL;
}

PifSListIterator pifSList_Find(PifSList* p_owner, int index)
{
	PifSListIterator it = p_owner->p_head;
	for (int i = 0; i < index; i++) {
		if (!it) return NULL;

		it = it->p_next;
	}
	return it;
}
