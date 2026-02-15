#ifndef PIF_OBJ_ARRAY_H
#define PIF_OBJ_ARRAY_H


#include "core/pif.h"


/**
 * @struct StPifObjArrayNode
 * @brief Represents the obj array node data structure used by this module.
 */
typedef struct StPifObjArrayNode
{
	struct StPifObjArrayNode* p_next;
	struct StPifObjArrayNode* p_prev;
	char data[];
} PifObjArrayNode;

typedef PifObjArrayNode* PifObjArrayIterator;

typedef void (*PifEvtObjArrayClear)(PifObjArrayIterator it);

/**
 * @struct StPifObjArray
 * @brief Represents the obj array data structure used by this module.
 */
typedef struct StPifObjArray
{
	// Read-only Member Variable
	int _size;
    int _max_count;
    int _count;
    PifObjArrayIterator _p_node;

    PifObjArrayIterator _p_first;
    PifObjArrayIterator _p_free;

	// Private Event Function
    PifEvtObjArrayClear __evt_clear;
} PifObjArray;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifObjArray_Init
 * @brief Initializes the obj array instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @param size Size value used for allocation or capacity.
 * @param max_count Maximum number of elements to manage.
 * @param evt_clear Optional callback invoked before removing each element.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifObjArray_Init(PifObjArray* p_owner, int size, int max_count, PifEvtObjArrayClear evt_clear);

/**
 * @fn pifObjArray_Clear
 * @brief Clears the obj array state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifObjArray_Clear(PifObjArray* p_owner);

/**
 * @fn pifObjArray_Add
 * @brief Adds an item to the obj array and updates internal bookkeeping for subsequent operations.
 * @param p_owner Pointer to the target object instance.
 * @return Return value of this API.
 */
PifObjArrayIterator pifObjArray_Add(PifObjArray* p_owner);

/**
 * @fn pifObjArray_Remove
 * @brief Removes an item from the obj array and updates internal bookkeeping for consistency.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to input or output data buffer.
 */
void pifObjArray_Remove(PifObjArray* p_owner, void* p_data);

/**
 * @fn pifObjArray_Count
 * @brief Returns the current number of valid items managed by the obj array.
 * @param p_owner Pointer to the target object instance.
 * @return Current number of elements in the object array.
 */
#define pifObjArray_Count(p_owner)	((PifObjArray*)(p_owner))->_count

/**
 * @fn pifObjArray_Begin
 * @brief Returns an iterator pointing to the first valid item in the obj array.
 * @param p_owner Pointer to the target object instance.
 * @return Iterator to the first element, or NULL if the array is empty.
 */
#define pifObjArray_Begin(p_owner)	((PifObjArray*)(p_owner))->_p_first

/**
 * @fn pifObjArray_Next
 * @brief Advances an iterator and returns the next valid item in the obj array sequence.
 * @param it Iterator pointing to the current element.
 * @return Iterator to the next element, or NULL if there is no next element.
 */
#define pifObjArray_Next(it)	((it) ? (it)->p_next : NULL)

#ifdef __cplusplus
}
#endif

#endif	// PIF_OBJ_ARRAY_H
