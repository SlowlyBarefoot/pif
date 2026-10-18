// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_ptr_array.h
 * @brief Fixed pool of pointers kept as a list.
 *
 * Like pif_obj_array, but each node holds a pointer to an object the caller owns instead of the object itself.
 */
#ifndef PIF_PTR_ARRAY_H
#define PIF_PTR_ARRAY_H


#include "pif.h"


/**
 * @struct StPifPtrArrayNode
 * @brief One node of the pool, holding a pointer.
 */
typedef struct StPifPtrArrayNode
{
	struct StPifPtrArrayNode* p_next;	///< Next node in the list it is on
	struct StPifPtrArrayNode* p_prev;	///< Previous node in the list it is on
	char* p_data;	///< Pointer to the object
} PifPtrArrayNode;

/**
 * @brief Position in an array, a node. NULL is past the end.
 */
typedef PifPtrArrayNode* PifPtrArrayIterator;

/**
 * @brief Called for each pointer in use when the array is cleared, to free what it refers to.
 */
typedef void (*PifEvtPtrArrayClear)(PifPtrArrayIterator it);

/**
 * @struct StPifPtrArray
 * @brief Pool of _max_count pointers.
 */
typedef struct StPifPtrArray
{
	// Read-only Member Variable
    int _max_count;	///< Number of nodes in the pool
    int _count;	///< Number of pointers in use
    PifPtrArrayIterator _p_node;	///< Memory of all nodes

    PifPtrArrayIterator _p_first;	///< First pointer in use, NULL when none
    PifPtrArrayIterator _p_free;	///< First free node, NULL when the pool is full

	// Private Event Function
    PifEvtPtrArrayClear __evt_clear;
} PifPtrArray;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates a pool of max_count nodes for pointers.
 * @param p_owner Pointer to the target object instance.
 * @param max_count Maximum number of elements to manage.
 * @param evt_clear Called for each pointer still in use when the array is cleared. May be NULL.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPtrArray_Init(PifPtrArray* p_owner, int max_count, PifEvtPtrArrayClear evt_clear);

/**
 * @brief Calls evt_clear for each pointer in use and frees the pool.
 * @param p_owner Pointer to the target object instance.
 */
void pifPtrArray_Clear(PifPtrArray* p_owner);

/**
 * @brief Takes a free node for p_data and puts it first in the list of pointers in use.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to keep.
 * @return The node, or NULL with E_OVERFLOW_BUFFER when the pool is full.
 */
PifPtrArrayIterator pifPtrArray_Add(PifPtrArray* p_owner, void *p_data);

/**
 * @brief Gives a node back to the pool.
 * @param p_owner Pointer to the target object instance.
 * @param p_node Node from pifPtrArray_Add().
 */
void pifPtrArray_Remove(PifPtrArray* p_owner, PifPtrArrayIterator p_node);

/**
 * @brief Finds the node that holds a pointer.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to look for.
 * @return The node, or NULL when the pointer is not in the array.
 */
PifPtrArrayIterator pifPtrArray_Find(PifPtrArray* p_owner, void *p_data);

/**
 * @brief Gets the number of pointers in use.
 * @param p_owner Pointer to the target object instance.
 * @return Current number of elements in the pointer array.
 */
#define pifPtrArray_Count(p_owner)	((PifPtrArray*)(p_owner))->_count

/**
 * @brief Returns an iterator pointing to the first valid item in the ptr array.
 * @param p_owner Pointer to the target object instance.
 * @return Iterator to the first element, or NULL if the array is empty.
 */
#define pifPtrArray_Begin(p_owner)	((PifPtrArray*)(p_owner))->_p_first

/**
 * @brief Moves to the next node in use.
 * @param it Iterator pointing to the current element.
 * @return Iterator to the next element, or NULL if there is no next element.
 */
#define pifPtrArray_Next(it)	((it) ? (it)->p_next : NULL)

#ifdef __cplusplus
}
#endif

#endif	// PIF_PTR_ARRAY_H
