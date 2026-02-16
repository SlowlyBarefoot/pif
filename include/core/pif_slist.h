#ifndef PIF_SLIST_H
#define PIF_SLIST_H


#include "core/pif.h"


/**
 * @struct StPifSListNode
 * @brief Represents the slist node data structure used by this module.
 */
typedef struct StPifSListNode
{
	struct StPifSListNode* p_next;
	char data[];
} PifSListNode;

typedef PifSListNode* PifSListIterator;

/**
 * @struct StPifSList
 * @brief Represents the slist data structure used by this module.
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
 * @brief Initializes the slist instance and prepares all internal fields for safe use.
 * @param p_owner List instance to initialize.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSList_Init(PifSList* p_owner);

/**
 * @fn pifSList_Clear
 * @brief Clears the slist state and releases resources currently owned by the instance.
 * @param p_owner Target list.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifSList_Clear(PifSList* p_owner, PifEvtSListClear evt_clear);

/**
 * @fn pifSList_AddFirst
 * @brief Adds an item to the slist and updates internal bookkeeping for subsequent operations.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifSList_AddFirst(PifSList* p_owner, int data_size);

/**
 * @fn pifSList_AddLast
 * @brief Adds an item to the slist and updates internal bookkeeping for subsequent operations.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifSList_AddLast(PifSList* p_owner, int data_size);

/**
 * @fn pifSList_RemoveFirst
 * @brief Removes an item from the slist and updates internal bookkeeping for consistency.
 * @param p_owner Target list.
 */
void pifSList_RemoveFirst(PifSList* p_owner);

/**
 * @fn pifSList_Size
 * @brief Returns the current number of valid items managed by the slist.
 * @param p_owner Target list.
 * @return Current node count.
 */
int pifSList_Size(PifSList* p_owner);

/**
 * @fn pifSList_Begin
 * @brief Returns an iterator pointing to the first valid item in the slist.
 * @param p_owner Target list.
 * @return Head iterator, or NULL if empty.
 */
PifSListIterator pifSList_Begin(PifSList* p_owner);

/**
 * @fn pifSList_End
 * @brief Returns an iterator pointing to the last valid item in the slist.
 * @param p_owner Target list.
 * @return Tail iterator, or NULL if empty.
 */
PifSListIterator pifSList_End(PifSList* p_owner);

/**
 * @fn pifSList_Next
 * @brief Advances an iterator and returns the next valid item in the slist sequence.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PifSListIterator pifSList_Next(PifSListIterator it);

/**
 * @fn pifSList_Find
 * @brief Finds an item in the slist by index or key and returns its iterator or pointer.
 * @param p_owner Target list.
 * @param index Zero-based node index.
 * @return Iterator at index, or NULL if out of range.
 */
PifSListIterator pifSList_Find(PifSList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_SLIST_H
