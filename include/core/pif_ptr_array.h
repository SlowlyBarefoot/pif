#ifndef PIF_PTR_ARRAY_H
#define PIF_PTR_ARRAY_H


#include "pif.h"


/**
 * @struct StPifPtrArrayNode
 * @brief Represents the ptr array node data structure used by this module.
 */
typedef struct StPifPtrArrayNode
{
	struct StPifPtrArrayNode* p_next;
	struct StPifPtrArrayNode* p_prev;
	char* p_data;
} PifPtrArrayNode;

typedef PifPtrArrayNode* PifPtrArrayIterator;

typedef void (*PifEvtPtrArrayClear)(PifPtrArrayIterator it);

/**
 * @struct StPifPtrArray
 * @brief Represents the ptr array data structure used by this module.
 */
typedef struct StPifPtrArray
{
	// Read-only Member Variable
    int _max_count;
    int _count;
    PifPtrArrayIterator _p_node;

    PifPtrArrayIterator _p_first;
    PifPtrArrayIterator _p_free;

	// Private Event Function
    PifEvtPtrArrayClear __evt_clear;
} PifPtrArray;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPtrArray_Init
 * @brief Initializes the ptr array instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param max_count Maximum number of elements to manage.
 * @param evt_clear Optional callback invoked before removing each element.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPtrArray_Init(PifPtrArray* p_owner, int max_count, PifEvtPtrArrayClear evt_clear);

/**
 * @fn pifPtrArray_Clear
 * @brief Clears the ptr array state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifPtrArray_Clear(PifPtrArray* p_owner);

/**
 * @fn pifPtrArray_Add
 * @brief Adds an item to the ptr array and updates internal bookkeeping for subsequent operations.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 * @return Return value of this API.
 */
PifPtrArrayIterator pifPtrArray_Add(PifPtrArray* p_owner, void *p_data);

/**
 * @fn pifPtrArray_Remove
 * @brief Removes an item from the ptr array and updates internal bookkeeping for consistency.
 * @param p_owner Pointer to the target object instance.
 * @param p_node Pointer to the node instance to remove or process.
 */
void pifPtrArray_Remove(PifPtrArray* p_owner, PifPtrArrayIterator p_node);

/**
 * @fn pifPtrArray_Find
 * @brief Finds an item in the ptr array by index or key and returns its iterator or pointer.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 * @return Return value of this API.
 */
PifPtrArrayIterator pifPtrArray_Find(PifPtrArray* p_owner, void *p_data);

/**
 * @fn pifPtrArray_Count
 * @brief Returns the current number of valid items managed by the ptr array.
 * @param p_owner Pointer to the target object instance.
 * @return Current number of elements in the pointer array.
 */
#define pifPtrArray_Count(p_owner)	((PifPtrArray*)(p_owner))->_count

/**
 * @fn pifPtrArray_Begin
 * @brief Returns an iterator pointing to the first valid item in the ptr array.
 * @param p_owner Pointer to the target object instance.
 * @return Iterator to the first element, or NULL if the array is empty.
 */
#define pifPtrArray_Begin(p_owner)	((PifPtrArray*)(p_owner))->_p_first

/**
 * @fn pifPtrArray_Next
 * @brief Advances an iterator and returns the next valid item in the ptr array sequence.
 * @param it Iterator pointing to the current element.
 * @return Iterator to the next element, or NULL if there is no next element.
 */
#define pifPtrArray_Next(it)	((it) ? (it)->p_next : NULL)

#ifdef __cplusplus
}
#endif

#endif	// PIF_PTR_ARRAY_H
