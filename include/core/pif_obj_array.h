#ifndef PIF_OBJ_ARRAY_H
#define PIF_OBJ_ARRAY_H


#include "pif.h"


/**
 * @struct StPifObjArrayNode
 * @brief
 */
typedef struct StPifObjArrayNode
{
	struct StPifObjArrayNode* p_next;
	struct StPifObjArrayNode* p_prev;
	char data[1];
} PifObjArrayNode;

typedef PifObjArrayNode* PifObjArrayIterator;

typedef void (*PifEvtObjArrayClear)(PifObjArrayIterator it);

/**
 * @struct StPifObjArray
 * @brief
 */
typedef struct StPifObjArray
{
	int size;
    int max_count;
    int count;
    PifObjArrayIterator p_node;

    PifObjArrayIterator p_first;
    PifObjArrayIterator p_free;

    PifEvtObjArrayClear evt_clear;
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
#ifdef __PIF_NO_USE_INLINE__
	int pifObjArray_Count(PifObjArray* p_owner);
#else
	inline int pifObjArray_Count(PifObjArray* p_owner) { return p_owner->count; }
#endif

/**
 * @fn pifObjArray_Begin
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifObjArrayIterator pifObjArray_Begin(PifObjArray* p_owner);
#else
	inline PifObjArrayIterator pifObjArray_Begin(PifObjArray* p_owner) { return p_owner->p_first; }
#endif

/**
 * @fn pifObjArray_Next
 * @brief
 * @param it
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifObjArrayIterator pifObjArray_Next(PifObjArrayIterator it);
#else
	inline PifObjArrayIterator pifObjArray_Next(PifObjArrayIterator it) { return it ? it->p_next : NULL; }
#endif

#ifdef __cplusplus
}
#endif

#endif	// PIF_OBJ_ARRAY_H
