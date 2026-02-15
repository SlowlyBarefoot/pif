#ifndef PIF_OBJ_ARRAY_H
#define PIF_OBJ_ARRAY_H


#include "core/pif.h"


/**
 * @struct StPifObjArrayNode
 * @brief
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
 * @brief
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
 * @brief
 * @param p_owner
 * @param size
 * @param max_count
 * @param evt_clear
 * @return
 */
BOOL pifObjArray_Init(PifObjArray* p_owner, int size, int max_count, PifEvtObjArrayClear evt_clear);

/**
 * @fn pifObjArray_Clear
 * @brief
 * @param p_owner
 */
void pifObjArray_Clear(PifObjArray* p_owner);

/**
 * @fn pifObjArray_Add
 * @brief
 * @param p_owner
 * @return
 */
PifObjArrayIterator pifObjArray_Add(PifObjArray* p_owner);

/**
 * @fn pifObjArray_Remove
 * @brief
 * @param p_owner
 * @param p_data
 */
void pifObjArray_Remove(PifObjArray* p_owner, void* p_data);

/**
 * @fn pifObjArray_Count
 * @brief
 * @param p_owner
 * @return
 */
#define pifObjArray_Count(p_owner)	((PifObjArray*)(p_owner))->_count

/**
 * @fn pifObjArray_Begin
 * @brief
 * @param p_owner
 * @return
 */
#define pifObjArray_Begin(p_owner)	((PifObjArray*)(p_owner))->_p_first

/**
 * @fn pifObjArray_Next
 * @brief
 * @param it
 * @return
 */
#define pifObjArray_Next(it)	((it) ? (it)->p_next : NULL)

#ifdef __cplusplus
}
#endif

#endif	// PIF_OBJ_ARRAY_H
