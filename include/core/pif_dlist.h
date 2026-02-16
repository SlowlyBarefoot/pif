#ifndef PIF_DLIST_H
#define PIF_DLIST_H


#include "core/pif.h"


/**
 * @struct StPifDListNode
 * @brief Represents the dlist node data structure used by this module.
 */
typedef struct StPifDListNode
{
    struct StPifDListNode* p_prev;
    struct StPifDListNode* p_next;
    char data[];
} PifDListNode;

typedef PifDListNode* PifDListIterator;

/**
 * @struct StPifDList
 * @brief Represents the dlist data structure used by this module.
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
 * @brief Initializes the dlist instance and prepares all internal fields for safe use.
 * @param p_owner List instance to initialize.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifDList_Init(PifDList* p_owner);

/**
 * @fn pifDList_Clear
 * @brief Clears the dlist state and releases resources currently owned by the instance.
 * @param p_owner Target list.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifDList_Clear(PifDList* p_owner, PifEvtDListClear evt_clear);

/**
 * @fn pifDList_AddFirst
 * @brief Adds an item to the dlist and updates internal bookkeeping for subsequent operations.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_AddFirst(PifDList* p_owner, int data_size);

/**
 * @fn pifDList_AddLast
 * @brief Adds an item to the dlist and updates internal bookkeeping for subsequent operations.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_AddLast(PifDList* p_owner, int data_size);

/**
 * @fn pifDList_Add
 * @brief Adds an item to the dlist and updates internal bookkeeping for subsequent operations.
 * @param p_owner Target list.
 * @param data_size Payload size in bytes.
 * @param it Insert position; if NULL, append to tail.
 * @return Pointer to payload area of the inserted node, or NULL on failure.
 */
void* pifDList_Add(PifDList* p_owner, int data_size, PifDListIterator it);

/**
 * @fn pifDList_RemoveFirst
 * @brief Removes an item from the dlist and updates internal bookkeeping for consistency.
 * @param p_owner Target list.
 */
void pifDList_RemoveFirst(PifDList* p_owner);

/**
 * @fn pifDList_RemoveLast
 * @brief Removes an item from the dlist and updates internal bookkeeping for consistency.
 * @param p_owner Target list.
 */
void pifDList_RemoveLast(PifDList* p_owner);

/**
 * @fn pifDList_RemoveIterator
 * @brief Removes an item from the dlist and updates internal bookkeeping for consistency.
 * @param p_owner Target list.
 * @param it Iterator to remove.
 */
void pifDList_RemoveIterator(PifDList* p_owner, PifDListIterator it);

/**
 * @fn pifDList_Remove
 * @brief Removes an item from the dlist and updates internal bookkeeping for consistency.
 * @param p_owner Target list.
 * @param p_data Pointer to node payload.
 */
void pifDList_Remove(PifDList* p_owner, void* p_data);

/**
 * @fn pifDList_Size
 * @brief Returns the current number of valid items managed by the dlist.
 * @param p_owner Target list.
 * @return Current node count.
 */
int pifDList_Size(PifDList* p_owner);

/**
 * @fn pifDList_Begin
 * @brief Returns an iterator pointing to the first valid item in the dlist.
 * @param p_owner Target list.
 * @return Head iterator, or NULL if empty.
 */
PifDListIterator pifDList_Begin(PifDList* p_owner);

/**
 * @fn pifDList_Next
 * @brief Advances an iterator and returns the next valid item in the dlist sequence.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PifDListIterator pifDList_Next(PifDListIterator it);

/**
 * @fn pifDList_End
 * @brief Returns an iterator pointing to the last valid item in the dlist.
 * @param p_owner Target list.
 * @return Tail iterator, or NULL if empty.
 */
PifDListIterator pifDList_End(PifDList* p_owner);

/**
 * @fn pifDList_Prev
 * @brief Moves an iterator backward and returns the previous valid item in the dlist sequence.
 * @param it Current iterator.
 * @return Previous iterator, or NULL at begin.
 */
PifDListIterator pifDList_Prev(PifDListIterator it);

/**
 * @fn pifDList_Find
 * @brief Finds an item in the dlist by index or key and returns its iterator or pointer.
 * @param p_owner Target list.
 * @param index Zero-based node index.
 * @return Iterator at index, or NULL if out of range.
 */
PifDListIterator pifDList_Find(PifDList* p_owner, int index);

#ifdef __cplusplus
}
#endif

#endif	// PIF_DLIST_H
