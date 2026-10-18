// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_obj_array.h
 * @brief Fixed pool of equal-size objects kept as a list.
 *
 * All nodes are allocated at once by pifObjArray_Init(). Adding takes a node from the free list and removing
 * gives it back, so no memory is allocated afterwards.
 */
#ifndef PIF_OBJ_ARRAY_H
#define PIF_OBJ_ARRAY_H


#include "core/pif.h"


/**
 * @struct StPifObjArrayNode
 * @brief One node of the pool, followed by the data of its object.
 */
typedef struct StPifObjArrayNode
{
	struct StPifObjArrayNode* p_next;	///< Next node in the list it is on
	struct StPifObjArrayNode* p_prev;	///< Previous node in the list it is on
	char data[];	///< Data of the object
} PifObjArrayNode;

/**
 * @brief Position in an array, a node. NULL is past the end.
 */
typedef PifObjArrayNode* PifObjArrayIterator;

/**
 * @brief Called for each object in use when the array is cleared, to free what it refers to.
 */
typedef void (*PifEvtObjArrayClear)(PifObjArrayIterator it);

/**
 * @struct StPifObjArray
 * @brief Pool of _max_count objects of _size bytes.
 */
typedef struct StPifObjArray
{
	// Read-only Member Variable
	int _size;	///< Bytes of one object
    int _max_count;	///< Number of nodes in the pool
    int _count;	///< Number of objects in use
    PifObjArrayIterator _p_node;	///< Memory of all nodes

    PifObjArrayIterator _p_first;	///< First object in use, NULL when none
    PifObjArrayIterator _p_free;	///< First free node, NULL when the pool is full

	// Private Event Function
    PifEvtObjArrayClear __evt_clear;
} PifObjArray;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates a pool of max_count objects of size bytes.
 * @param p_owner Pointer to the target object instance.
 * @param size Bytes of one object.
 * @param max_count Maximum number of elements to manage.
 * @param evt_clear Called for each object still in use when the array is cleared. May be NULL.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifObjArray_Init(PifObjArray* p_owner, int size, int max_count, PifEvtObjArrayClear evt_clear);

/**
 * @brief Calls evt_clear for each object in use and frees the pool.
 * @param p_owner Pointer to the target object instance.
 */
void pifObjArray_Clear(PifObjArray* p_owner);

/**
 * @brief Takes a free object and puts it first in the list of objects in use. Its data is in it->data.
 * @param p_owner Pointer to the target object instance.
 * @return The node of the object, or NULL with E_OVERFLOW_BUFFER when the pool is full.
 */
PifObjArrayIterator pifObjArray_Add(PifObjArray* p_owner);

/**
 * @brief Gives an object back to the pool.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Data of the object, it->data of its node.
 */
void pifObjArray_Remove(PifObjArray* p_owner, void* p_data);

/**
 * @brief Gets the number of objects in use.
 * @param p_owner Pointer to the target object instance.
 * @return Current number of elements in the object array.
 */
#define pifObjArray_Count(p_owner)	((PifObjArray*)(p_owner))->_count

/**
 * @brief Returns an iterator pointing to the first valid item in the obj array.
 * @param p_owner Pointer to the target object instance.
 * @return Iterator to the first element, or NULL if the array is empty.
 */
#define pifObjArray_Begin(p_owner)	((PifObjArray*)(p_owner))->_p_first

/**
 * @brief Moves to the next object in use.
 * @param it Iterator pointing to the current element.
 * @return Iterator to the next element, or NULL if there is no next element.
 */
#define pifObjArray_Next(it)	((it) ? (it)->p_next : NULL)

#ifdef __cplusplus
}
#endif

#endif	// PIF_OBJ_ARRAY_H
