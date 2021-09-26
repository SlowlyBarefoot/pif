#ifndef PIF_LIST_H
#define PIF_LIST_H


#include "pif.h"


// ---------- PIF Singly Linked List ----------

/**
 * @struct _PIF_SListNode
 * @brief
 */
typedef struct _PIF_SListNode
{
	struct _PIF_SListNode* p_next;
	char data[1];
} PIF_SListNode;

typedef PIF_SListNode* PIF_SListIterator;

/**
 * @struct _PIF_SList
 * @brief
 */
typedef struct _PIF_SList
{
	PIF_SListIterator p_head;
	PIF_SListIterator p_tail;
    int size;
} PIF_SList;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSList_Create
 * @brief
 * @return
 */
PIF_SList* pifSList_Create();

/**
 * @fn pifSList_Destroy
 * @brief
 * @param pp_owner
 */
void pifSList_Destroy(PIF_SList** pp_owner);

/**
 * @fn pifSList_Init
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifSList_Init(PIF_SList* p_owner);

/**
 * @fn pifSList_Clear
 * @brief
 * @param p_owner
 */
void pifSList_Clear(PIF_SList* p_owner);

/**
 * @fn pifSList_AddFirst
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifSList_AddFirst(PIF_SList* p_owner, int data_size);

/**
 * @fn pifSList_AddLast
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifSList_AddLast(PIF_SList* p_owner, int data_size);

/**
 * @fn pifSList_RemoveFirst
 * @brief
 * @param p_owner
 */
void pifSList_RemoveFirst(PIF_SList* p_owner);

/**
 * @fn pifSList_Size
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	int pifSList_Size(PIF_SList* p_owner);
#else
	inline int pifSList_Size(PIF_SList* p_owner) { return p_owner->size; }
#endif

/**
 * @fn pifSList_Begin
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_SListIterator pifSList_Begin(PIF_SList* p_owner);
#else
	inline PIF_SListIterator pifSList_Begin(PIF_SList* p_owner) { return p_owner->p_head; }
#endif

/**
 * @fn pifSList_End
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_SListIterator pifSList_End(PIF_SList* p_owner);
#else
	inline PIF_SListIterator pifSList_End(PIF_SList* p_owner) {	return p_owner->p_tail; }
#endif

/**
 * @fn pifSList_Next
 * @brief
 * @param it
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_SListIterator pifSList_Next(PIF_SListIterator it);
#else
	inline PIF_SListIterator pifSList_Next(PIF_SListIterator it) { return it ? it->p_next : NULL; }
#endif

/**
 * @fn pifSList_Find
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
PIF_SListIterator pifSList_Find(PIF_SList* p_owner, int index);

#ifdef __cplusplus
}
#endif


// ---------- PIF Doubly Linked List ----------

/**
 * @struct _PIF_DListNode
 * @brief
 */
typedef struct _PIF_DListNode
{
    struct _PIF_DListNode* p_prev;
    struct _PIF_DListNode* p_next;
    char data[1];
} PIF_DListNode;

typedef PIF_DListNode* PIF_DListIterator;

/**
 * @struct _PIF_DList
 * @brief
 */
typedef struct _PIF_DList
{
	PIF_DListIterator p_head;
	PIF_DListIterator p_tail;
	int size;
} PIF_DList;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDList_Create
 * @brief
 * @return
 */
PIF_DList* pifDList_Create();

/**
 * @fn pifDList_Destroy
 * @brief
 * @param pp_owner
 */
void pifDList_Destroy(PIF_DList** pp_owner);

/**
 * @fn pifDList_Init
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifDList_Init(PIF_DList* p_owner);

/**
 * @fn pifDList_Clear
 * @brief
 * @param p_owner
 */
void pifDList_Clear(PIF_DList* p_owner);

/**
 * @fn pifDList_AddFirst
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifDList_AddFirst(PIF_DList* p_owner, int data_size);

/**
 * @fn pifDList_AddLast
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifDList_AddLast(PIF_DList* p_owner, int data_size);

/**
 * @fn pifDList_Add
 * @brief
 * @param p_owner
 * @param data_size
 * @param it
 * @return
 */
void* pifDList_Add(PIF_DList* p_owner, int data_size, PIF_DListIterator it);

/**
 * @fn pifDList_RemoveFirst
 * @brief
 * @param p_owner
 */
void pifDList_RemoveFirst(PIF_DList* p_owner);

/**
 * @fn pifDList_RemoveLast
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
void pifDList_RemoveLast(PIF_DList* p_owner);

/**
 * @fn pifDList_Remove
 * @brief
 * @param p_owner
 * @param it
 */
void pifDList_Remove(PIF_DList* p_owner, PIF_DListIterator it);

/**
 * @fn pifDList_Size
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	int pifDList_Size(PIF_DList* p_owner);
#else
	inline int pifDList_Size(PIF_DList* p_owner) { return p_owner->size; }
#endif

/**
 * @fn pifDList_Begin
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_DListIterator pifDList_Begin(PIF_DList* p_owner);
#else
	inline PIF_DListIterator pifDList_Begin(PIF_DList* p_owner) { return p_owner->p_head; }
#endif

/**
 * @fn pifDList_Next
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_DListIterator pifDList_Next(PIF_DListIterator it);
#else
	inline PIF_DListIterator pifDList_Next(PIF_DListIterator it) { return it ? it->p_next : NULL; }
#endif

/**
 * @fn pifDList_End
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_DListIterator pifDList_End(PIF_DList* p_owner);
#else
	inline PIF_DListIterator pifDList_End(PIF_DList* p_owner) { return p_owner->p_tail; }
#endif

/**
 * @fn pifDList_Prev
 * @brief
 * @param p_owner
 * @return
 */
#ifdef __PIF_NO_USE_INLINE__
	PIF_DListIterator pifDList_Prev(PIF_DListIterator it);
#else
	inline PIF_DListIterator pifDList_Prev(PIF_DListIterator it) { return it ? it->p_prev : NULL; }
#endif

/**
 * @fn pifDList_Find
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
PIF_DListIterator pifDList_Find(PIF_DList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_LIST_H
