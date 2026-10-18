// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_slist.h
 * @brief Singly linked list with the data stored in each node.
 *
 * Each node is allocated with room for the data of its item. Adding at the tail and walking from the head are
 * O(1) per item.
 */
#ifndef PIF_SLIST_H
#define PIF_SLIST_H


#include "core/pif.h"


/**
 * @struct StPifSListNode
 * @brief One node of a list, followed by the data of its item.
 */
typedef struct StPifSListNode
{
	struct StPifSListNode* p_next;	///< Next node, NULL at the tail
	char data[];	///< Data of the item
} PifSListNode;

/**
 * @brief Position in a list, a node. NULL is past the end.
 */
typedef PifSListNode* PifSListIterator;

/**
 * @struct StPifSList
 * @brief Singly linked list of items of any size.
 */
typedef struct StPifSList
{
	PifSListIterator p_head;	///< First node, NULL for an empty list
	PifSListIterator p_tail;	///< Last node, NULL for an empty list
    int size;	///< Number of nodes
} PifSList;

/**
 * @brief Called for each node removed by pifSList_Clear() to free what its data refers to.
 */
typedef void (*PifEvtSListClear)(char* p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Makes an empty list.
 * @note Nodes left in a non-empty list are not freed; call pifSList_Clear() first.
 * @param p_owner List instance to initialize.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSList_Init(PifSList* p_owner);

/**
 * @brief Removes and frees every node.
 * @param p_owner Target list.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifSList_Clear(PifSList* p_owner, PifEvtSListClear evt_clear);

/**
 * @brief Allocates a node with data_size bytes of data and puts it at the head.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes. Must not be negative.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifSList_AddFirst(PifSList* p_owner, int data_size);

/**
 * @brief Allocates a node with data_size bytes of data and puts it at the tail.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes. Must not be negative.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifSList_AddLast(PifSList* p_owner, int data_size);

/**
 * @brief Removes and frees the node at the head. Nothing happens on an empty list.
 * @param p_owner Target list.
 */
void pifSList_RemoveFirst(PifSList* p_owner);

/**
 * @brief Gets the number of nodes.
 * @param p_owner Target list.
 * @return Current node count.
 */
PIF_STATIC_INLINE int pifSList_Size(PifSList* p_owner)
{
	return p_owner ? p_owner->size : 0;
}

/**
 * @brief Returns an iterator pointing to the first valid item in the slist.
 * @param p_owner Target list.
 * @return Head iterator, or NULL if empty.
 */
PIF_STATIC_INLINE PifSListIterator pifSList_Begin(PifSList* p_owner)
{
	return p_owner ? p_owner->p_head : NULL;
}

/**
 * @brief Returns an iterator pointing to the last valid item in the slist.
 * @note Unlike an STL end(), this is the last item itself, not one past it.
 * @param p_owner Target list.
 * @return Tail iterator, or NULL if empty.
 */
PIF_STATIC_INLINE PifSListIterator pifSList_End(PifSList* p_owner)
{
	return p_owner ? p_owner->p_tail : NULL;
}

/**
 * @brief Moves to the next node, toward the tail.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PIF_STATIC_INLINE PifSListIterator pifSList_Next(PifSListIterator it)
{
	return it ? it->p_next : NULL;
}

/**
 * @brief Gets the node at a position.
 * @param p_owner Target list.
 * @param index Zero-based node index.
 * @return Iterator at index, or NULL if index is negative or out of range.
 */
PifSListIterator pifSList_Find(PifSList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_SLIST_H
