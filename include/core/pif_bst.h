// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_BST_H
#define PIF_BST_H


#include "core/pif.h"


// Key width in bits. Every node is padded to the payload alignment anyway, so
// a narrower key rarely saves memory; it only narrows the range of keys.
#ifndef PIF_BST_KEY
#define PIF_BST_KEY		32
#endif

#if PIF_BST_KEY == 8
typedef uint8_t PifBstKey;
#elif PIF_BST_KEY == 16
typedef uint16_t PifBstKey;
#elif PIF_BST_KEY == 32
typedef uint32_t PifBstKey;
#else
#error "PIF_BST_KEY must be 8, 16 or 32"
#endif


/**
 * @union UnPifBstAlign
 * @brief Gives the payload the strictest alignment a stored structure may need.
 */
typedef union UnPifBstAlign
{
    void *p;
    uint32_t u32;
    uint64_t u64;
    float f;
    double d;
} PifBstAlign;

/**
 * @struct StPifBstNode
 * @brief Represents one node of the AVL-balanced binary search tree.
 */
typedef struct StPifBstNode
{
	struct StPifBstNode *p_parent;
	struct StPifBstNode *p_small;
	struct StPifBstNode *p_big;
    PifBstKey key;
    uint8_t height;
	PifBstAlign data[];		// Payload of PifBst.data_size bytes.
} PifBstNode;

typedef PifBstNode *PifBstIterator;

/**
 * @struct StPifBst
 * @brief Represents a sorted key/payload map kept as an AVL-balanced binary search tree.
 */
typedef struct StPifBst
{
	PifBstIterator p_root;
    int data_size;
    int size;
} PifBst;

typedef void (*PifEvtBstClear)(PifBstKey key, void *p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBst_Init
 * @brief Initializes an empty tree whose nodes all carry a payload of the same size.
 * @param p_owner Tree instance to initialize.
 * @param data_size Payload size in bytes for every node. Zero makes a key-only set.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifBst_Init(PifBst *p_owner, int data_size);

/**
 * @fn pifBst_Clear
 * @brief Removes and frees every node without recursion.
 * @param p_owner Target tree.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifBst_Clear(PifBst *p_owner, PifEvtBstClear evt_clear);

/**
 * @fn pifBst_Add
 * @brief Inserts a new key with a zero-filled payload.
 * @param p_owner Target tree.
 * @param key Key to insert.
 * @return Pointer to the payload of the new node, or NULL when the key already
 *         exists (E_ALREADY_ATTACHED) or the heap is exhausted (E_OUT_OF_HEAP).
 */
void *pifBst_Add(PifBst *p_owner, PifBstKey key);

/**
 * @fn pifBst_Set
 * @brief Inserts a key, or reuses its node when it already exists, and copies the payload.
 * @param p_owner Target tree.
 * @param key Key to insert or update.
 * @param p_data data_size bytes to copy into the payload. NULL leaves the payload
 *               as it is (zero-filled for a new node), which makes this a get-or-add.
 * @return Pointer to the payload of the node, or NULL when the heap is exhausted.
 */
void *pifBst_Set(PifBst *p_owner, PifBstKey key, const void *p_data);

/**
 * @fn pifBst_Remove
 * @brief Removes the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to remove.
 * @return TRUE when a node was removed, FALSE when the key does not exist.
 */
BOOL pifBst_Remove(PifBst *p_owner, PifBstKey key);

/**
 * @fn pifBst_Erase
 * @brief Removes the node an iterator points to. Iterators to other nodes stay valid.
 * @param p_owner Target tree.
 * @param it Iterator to remove.
 * @return Iterator following the removed node, or NULL at end.
 */
PifBstIterator pifBst_Erase(PifBst *p_owner, PifBstIterator it);

/**
 * @fn pifBst_Size
 * @brief Returns the number of nodes in the tree.
 * @param p_owner Target tree.
 * @return Current node count.
 */
int pifBst_Size(PifBst *p_owner);

/**
 * @fn pifBst_Find
 * @brief Finds the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to look up.
 * @return Iterator to the node, or NULL if the key does not exist.
 */
PifBstIterator pifBst_Find(PifBst *p_owner, PifBstKey key);

/**
 * @fn pifBst_FindData
 * @brief Finds the payload of the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to look up.
 * @return Pointer to the payload, or NULL if the key does not exist.
 */
void *pifBst_FindData(PifBst *p_owner, PifBstKey key);

/**
 * @fn pifBst_LowerBound
 * @brief Finds the first node whose key is not less than the given key.
 * @param p_owner Target tree.
 * @param key Lower bound of the range.
 * @return Iterator to the node, or NULL if every key is smaller.
 */
PifBstIterator pifBst_LowerBound(PifBst *p_owner, PifBstKey key);

/**
 * @fn pifBst_Begin
 * @brief Returns an iterator to the node with the smallest key.
 * @param p_owner Target tree.
 * @return First iterator, or NULL if empty.
 */
PifBstIterator pifBst_Begin(PifBst *p_owner);

/**
 * @fn pifBst_End
 * @brief Returns an iterator to the node with the largest key.
 * @param p_owner Target tree.
 * @return Last iterator, or NULL if empty.
 */
PifBstIterator pifBst_End(PifBst *p_owner);

/**
 * @fn pifBst_Next
 * @brief Advances an iterator to the node with the next larger key.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PifBstIterator pifBst_Next(PifBstIterator it);

/**
 * @fn pifBst_Prev
 * @brief Moves an iterator to the node with the next smaller key.
 * @param it Current iterator.
 * @return Previous iterator, or NULL at begin.
 */
PifBstIterator pifBst_Prev(PifBstIterator it);

#ifdef __cplusplus
}
#endif


#endif	// PIF_BST_H
