#ifndef PIF_PTR_ARRAY_H
#define PIF_PTR_ARRAY_H


#include "pif.h"


/**
 * @struct StPifPtrArrayNode
 * @brief
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
 * @brief
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
 * @brief
 * @param p_owner
 * @param max_count
 * @param evt_clear
 * @return
 */
BOOL pifPtrArray_Init(PifPtrArray* p_owner, int max_count, PifEvtPtrArrayClear evt_clear);

/**
 * @fn pifPtrArray_Clear
 * @brief
 * @param p_owner
 */
void pifPtrArray_Clear(PifPtrArray* p_owner);

/**
 * @fn pifPtrArray_Add
 * @brief
 * @param p_owner
 * @return
 */
PifPtrArrayIterator pifPtrArray_Add(PifPtrArray* p_owner);

/**
 * @fn pifPtrArray_Remove
 * @brief
 * @param p_owner
 * @param p_data
 */
void pifPtrArray_Remove(PifPtrArray* p_owner, void* p_data);

/**
 * @fn pifPtrArray_Count
 * @brief
 * @param p_owner
 * @return
 */
#define pifPtrArray_Count(p_owner)	((PifPtrArray*)(p_owner))->_count

/**
 * @fn pifPtrArray_Begin
 * @brief
 * @param p_owner
 * @return
 */
#define pifPtrArray_Begin(p_owner)	((PifPtrArray*)(p_owner))->_p_first

/**
 * @fn pifPtrArray_Next
 * @brief
 * @param it
 * @return
 */
#define pifPtrArray_Next(it)	(it) ? (it)->p_next : NULL

#ifdef __cplusplus
}
#endif

#endif	// PIF_PTR_ARRAY_H
