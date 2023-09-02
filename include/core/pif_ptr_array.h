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
    int max_count;
    int count;
    PifPtrArrayIterator p_node;

    PifPtrArrayIterator p_first;
    PifPtrArrayIterator p_free;

    PifEvtPtrArrayClear evt_clear;
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
#ifdef __PIF_NO_USE_INLINE__
	int pifPtrArray_Count(PifPtrArray* p_owner);
#else
	inline int pifPtrArray_Count(PifPtrArray* p_owner) { return p_owner->count; }
#endif

/**
 * @fn pifPtrArray_Begin
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifPtrArrayIterator pifPtrArray_Begin(PifPtrArray* p_owner);
#else
	inline PifPtrArrayIterator pifPtrArray_Begin(PifPtrArray* p_owner) { return p_owner->p_first; }
#endif

/**
 * @fn pifPtrArray_Next
 * @brief
 * @param it
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PifPtrArrayIterator pifPtrArray_Next(PifPtrArrayIterator it);
#else
	inline PifPtrArrayIterator pifPtrArray_Next(PifPtrArrayIterator it) { return it ? it->p_next : NULL; }
#endif

#ifdef __cplusplus
}
#endif

#endif	// PIF_PTR_ARRAY_H
