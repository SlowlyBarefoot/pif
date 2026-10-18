// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_dlist.h
 * @brief Doubly linked list with the data stored in each node.
 *
 * Each node is allocated with room for the data of its item, so the list can be walked both ways and an item can
 * be removed from the middle in O(1).
 */
#ifndef PIF_DLIST_H
#define PIF_DLIST_H


#include "core/pif.h"


/**
 * @struct StPifDListNode
 * @brief One node of a list, followed by the data of its item.
 */
typedef struct StPifDListNode
{
    struct StPifDListNode* p_prev;	///< Previous node, NULL at the head
    struct StPifDListNode* p_next;	///< Next node, NULL at the tail
    char data[];	///< Data of the item
} PifDListNode;

/**
 * @brief Position in a list, a node. NULL is past the end.
 */
typedef PifDListNode* PifDListIterator;

/**
 * @struct StPifDList
 * @brief Doubly linked list of items of any size.
 */
typedef struct StPifDList
{
	PifDListIterator p_head;	///< First node, NULL for an empty list
	PifDListIterator p_tail;	///< Last node, NULL for an empty list
	int size;	///< Number of nodes
} PifDList;

/**
 * @brief Called for each node removed by pifDList_Clear() to free what its data refers to.
 */
typedef void (*PifEvtDListClear)(char* p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Makes an empty list.
 * @note Nodes left in a non-empty list are not freed; call pifDList_Clear() first.
 * @param p_owner List instance to initialize.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDList_Init(PifDList* p_owner);

/**
 * @brief Removes and frees every node.
 * @param p_owner Target list.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifDList_Clear(PifDList* p_owner, PifEvtDListClear evt_clear);

/**
 * @brief Allocates a node with data_size bytes of data and puts it at the head.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes. Must not be negative.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_AddFirst(PifDList* p_owner, int data_size);

/**
 * @brief Allocates a node with data_size bytes of data and puts it at the tail.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes. Must not be negative.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_AddLast(PifDList* p_owner, int data_size);

/**
 * @brief Allocates a node with data_size bytes of data and puts it before it.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes. Must not be negative.
 * @param it Node to insert the new item before; if NULL, append to tail.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_Add(PifDList* p_owner, int data_size, PifDListIterator it);

/**
 * @brief Removes and frees the node at the head. Nothing happens on an empty list.
 * @param p_owner Target list.
 */
void pifDList_RemoveFirst(PifDList* p_owner);

/**
 * @brief Removes and frees the node at the tail. Nothing happens on an empty list.
 * @param p_owner Target list.
 */
void pifDList_RemoveLast(PifDList* p_owner);

/**
 * @brief Removes and frees one node.
 * @param p_owner Target list.
 * @param it Iterator to remove. Must be a node of p_owner.
 */
void pifDList_RemoveIterator(PifDList* p_owner, PifDListIterator it);

/**
 * @brief Removes and frees the node whose data is p_data.
 * @param p_owner Target list.
 * @param p_data Payload pointer returned by an Add function of p_owner.
 */
void pifDList_Remove(PifDList* p_owner, void* p_data);

/**
 * @brief Gets the number of nodes.
 * @param p_owner Target list.
 * @return Current node count.
 */
PIF_STATIC_INLINE int pifDList_Size(PifDList* p_owner)
{
	return p_owner ? p_owner->size : 0;
}

/**
 * @brief Returns an iterator pointing to the first valid item in the dlist.
 * @param p_owner Target list.
 * @return Head iterator, or NULL if empty.
 */
PIF_STATIC_INLINE PifDListIterator pifDList_Begin(PifDList* p_owner)
{
	return p_owner ? p_owner->p_head : NULL;
}

/**
 * @brief Moves to the next node, toward the tail.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PIF_STATIC_INLINE PifDListIterator pifDList_Next(PifDListIterator it)
{
	return it ? it->p_next : NULL;
}

/**
 * @brief Returns an iterator pointing to the last valid item in the dlist.
 * @note Unlike an STL end(), this is the last item itself, not one past it.
 * @param p_owner Target list.
 * @return Tail iterator, or NULL if empty.
 */
PIF_STATIC_INLINE PifDListIterator pifDList_End(PifDList* p_owner)
{
	return p_owner ? p_owner->p_tail : NULL;
}

/**
 * @brief Moves an iterator backward and returns the previous valid item in the dlist sequence.
 * @param it Current iterator.
 * @return Previous iterator, or NULL at begin.
 */
PIF_STATIC_INLINE PifDListIterator pifDList_Prev(PifDListIterator it)
{
	return it ? it->p_prev : NULL;
}

/**
 * @brief Gets the node at a position.
 * @param p_owner Target list.
 * @param index Zero-based node index.
 * @return Iterator at index, or NULL if index is negative or out of range.
 */
PifDListIterator pifDList_Find(PifDList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_DLIST_H
