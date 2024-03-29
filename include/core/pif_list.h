#ifndef PIF_LIST_H
#define PIF_LIST_H


#include "core/pif.h"


// ---------- PIF Singly Linked List ----------

/**
 * @struct StPifSListNode
 * @brief
 */
typedef struct StPifSListNode
{
	struct StPifSListNode* p_next;
	char data[1];
} PifSListNode;

typedef PifSListNode* PifSListIterator;

/**
 * @struct StPifSList
 * @brief
 */
typedef struct StPifSList
{
	PifSListIterator p_head;
	PifSListIterator p_tail;
    int size;
} PifSList;

typedef void (*PifEvtSListClear)(char* p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSList_Init
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifSList_Init(PifSList* p_owner);

/**
 * @fn pifSList_Clear
 * @brief
 * @param p_owner
 * @param evt_clear
 */
void pifSList_Clear(PifSList* p_owner, PifEvtSListClear* evt_clear);

/**
 * @fn pifSList_AddFirst
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifSList_AddFirst(PifSList* p_owner, int data_size);

/**
 * @fn pifSList_AddLast
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifSList_AddLast(PifSList* p_owner, int data_size);

/**
 * @fn pifSList_RemoveFirst
 * @brief
 * @param p_owner
 */
void pifSList_RemoveFirst(PifSList* p_owner);

/**
 * @fn pifSList_Size
 * @brief
 * @param p_owner
 * @return
 */
int pifSList_Size(PifSList* p_owner);

/**
 * @fn pifSList_Begin
 * @brief
 * @param p_owner
 * @return
 */
PifSListIterator pifSList_Begin(PifSList* p_owner);

/**
 * @fn pifSList_End
 * @brief
 * @param p_owner
 * @return
 */
PifSListIterator pifSList_End(PifSList* p_owner);

/**
 * @fn pifSList_Next
 * @brief
 * @param it
 * @return
 */
PifSListIterator pifSList_Next(PifSListIterator it);

/**
 * @fn pifSList_Find
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
PifSListIterator pifSList_Find(PifSList* p_owner, int index);

#ifdef __cplusplus
}
#endif


// ---------- PIF Doubly Linked List ----------

/**
 * @struct StPifDListNode
 * @brief
 */
typedef struct StPifDListNode
{
    struct StPifDListNode* p_prev;
    struct StPifDListNode* p_next;
    char data[1];
} PifDListNode;

typedef PifDListNode* PifDListIterator;

/**
 * @struct StPifDList
 * @brief
 */
typedef struct StPifDList
{
	PifDListIterator p_head;
	PifDListIterator p_tail;
	int size;
} PifDList;

typedef void (*PifEvtDListClear)(char* p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifDList_Init
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifDList_Init(PifDList* p_owner);

/**
 * @fn pifDList_Clear
 * @brief
 * @param p_owner
 * @param evt_clear
 */
void pifDList_Clear(PifDList* p_owner, PifEvtDListClear* evt_clear);

/**
 * @fn pifDList_AddFirst
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifDList_AddFirst(PifDList* p_owner, int data_size);

/**
 * @fn pifDList_AddLast
 * @brief
 * @param p_owner
 * @param data_size
 * @return
 */
void* pifDList_AddLast(PifDList* p_owner, int data_size);

/**
 * @fn pifDList_Add
 * @brief
 * @param p_owner
 * @param data_size
 * @param it
 * @return
 */
void* pifDList_Add(PifDList* p_owner, int data_size, PifDListIterator it);

/**
 * @fn pifDList_RemoveFirst
 * @brief
 * @param p_owner
 */
void pifDList_RemoveFirst(PifDList* p_owner);

/**
 * @fn pifDList_RemoveLast
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
void pifDList_RemoveLast(PifDList* p_owner);

/**
 * @fn pifDList_RemoveIterator
 * @brief
 * @param p_owner
 * @param it
 */
void pifDList_RemoveIterator(PifDList* p_owner, PifDListIterator it);

/**
 * @fn pifDList_Remove
 * @brief
 * @param p_owner
 * @param p_data
 */
void pifDList_Remove(PifDList* p_owner, void* p_data);

/**
 * @fn pifDList_Size
 * @brief
 * @param p_owner
 * @return
 */
int pifDList_Size(PifDList* p_owner);

/**
 * @fn pifDList_Begin
 * @brief
 * @param p_owner
 * @return
 */
PifDListIterator pifDList_Begin(PifDList* p_owner);

/**
 * @fn pifDList_Next
 * @brief
 * @param p_owner
 * @return
 */
PifDListIterator pifDList_Next(PifDListIterator it);

/**
 * @fn pifDList_End
 * @brief
 * @param p_owner
 * @return
 */
PifDListIterator pifDList_End(PifDList* p_owner);

/**
 * @fn pifDList_Prev
 * @brief
 * @param p_owner
 * @return
 */
PifDListIterator pifDList_Prev(PifDListIterator it);

/**
 * @fn pifDList_Find
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
PifDListIterator pifDList_Find(PifDList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_LIST_H
