// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_bst.h
 * @brief Sorted map from an integer key to a fixed-size payload, kept as an AVL-balanced binary search tree.
 *
 * Each node holds its key and data_size bytes of payload. Lookups, inserts and removals take O(log n), and the
 * iterator functions walk the keys in order.
 */
#ifndef PIF_BST_H
#define PIF_BST_H


#include "core/pif.h"


/// Key width in bits. Every node is padded to the payload alignment anyway, so
/// a narrower key rarely saves memory; it only narrows the range of keys.
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
    void *p;	///< Pointer alignment
    uint32_t u32;	///< 32 bit alignment
    uint64_t u64;	///< 64 bit alignment
    float f;	///< float alignment
    double d;	///< double alignment
} PifBstAlign;

/**
 * @struct StPifBstNode
 * @brief Represents one node of the AVL-balanced binary search tree.
 */
typedef struct StPifBstNode
{
	struct StPifBstNode *p_parent;	///< Parent node, NULL for the root
	struct StPifBstNode *p_small;	///< Subtree of the smaller keys
	struct StPifBstNode *p_big;	///< Subtree of the bigger keys
    PifBstKey key;	///< Key of the node
    uint8_t height;	///< Height of the subtree, for the balancing
	PifBstAlign data[];		///< Payload of PifBst.data_size bytes.
} PifBstNode;

/**
 * @brief Position in a tree, a node. NULL is past the end.
 */
typedef PifBstNode *PifBstIterator;

/**
 * @struct StPifBst
 * @brief Represents a sorted key/payload map kept as an AVL-balanced binary search tree.
 */
typedef struct StPifBst
{
	PifBstIterator p_root;	///< Root node, NULL for an empty tree
    int data_size;	///< Payload bytes of each node
    int size;	///< Number of nodes
} PifBst;

/**
 * @brief Called for each node removed by pifBst_Clear() to free what its payload refers to.
 */
typedef void (*PifEvtBstClear)(PifBstKey key, void *p_data);


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an empty tree whose nodes all carry a payload of the same size.
 * @param p_owner Tree instance to initialize.
 * @param data_size Payload size in bytes for every node. Zero makes a key-only set.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifBst_Init(PifBst *p_owner, int data_size);

/**
 * @brief Removes and frees every node without recursion.
 * @param p_owner Target tree.
 * @param evt_clear Optional callback invoked for each node payload before free.
 */
void pifBst_Clear(PifBst *p_owner, PifEvtBstClear evt_clear);

/**
 * @brief Inserts a new key with a zero-filled payload.
 * @param p_owner Target tree.
 * @param key Key to insert.
 * @return Pointer to the payload of the new node, or NULL when the key already
 *         exists (E_ALREADY_ATTACHED) or the heap is exhausted (E_OUT_OF_HEAP).
 */
void *pifBst_Add(PifBst *p_owner, PifBstKey key);

/**
 * @brief Inserts a key, or reuses its node when it already exists, and copies the payload.
 * @param p_owner Target tree.
 * @param key Key to insert or update.
 * @param p_data data_size bytes to copy into the payload. NULL leaves the payload
 *               as it is (zero-filled for a new node), which makes this a get-or-add.
 * @return Pointer to the payload of the node, or NULL when the heap is exhausted.
 */
void *pifBst_Set(PifBst *p_owner, PifBstKey key, const void *p_data);

/**
 * @brief Removes the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to remove.
 * @return TRUE when a node was removed, FALSE when the key does not exist.
 */
BOOL pifBst_Remove(PifBst *p_owner, PifBstKey key);

/**
 * @brief Removes the node an iterator points to. Iterators to other nodes stay valid.
 * @param p_owner Target tree.
 * @param it Iterator to remove.
 * @return Iterator following the removed node, or NULL at end.
 */
PifBstIterator pifBst_Erase(PifBst *p_owner, PifBstIterator it);

/**
 * @brief Returns the number of nodes in the tree.
 * @param p_owner Target tree.
 * @return Current node count.
 */
int pifBst_Size(PifBst *p_owner);

/**
 * @brief Finds the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to look up.
 * @return Iterator to the node, or NULL if the key does not exist.
 */
PifBstIterator pifBst_Find(PifBst *p_owner, PifBstKey key);

/**
 * @brief Finds the payload of the node holding the key.
 * @param p_owner Target tree.
 * @param key Key to look up.
 * @return Pointer to the payload, or NULL if the key does not exist.
 */
void *pifBst_FindData(PifBst *p_owner, PifBstKey key);

/**
 * @brief Finds the first node whose key is not less than the given key.
 * @param p_owner Target tree.
 * @param key Lower bound of the range.
 * @return Iterator to the node, or NULL if every key is smaller.
 */
PifBstIterator pifBst_LowerBound(PifBst *p_owner, PifBstKey key);

/**
 * @brief Returns an iterator to the node with the smallest key.
 * @param p_owner Target tree.
 * @return First iterator, or NULL if empty.
 */
PifBstIterator pifBst_Begin(PifBst *p_owner);

/**
 * @brief Returns an iterator to the node with the largest key.
 * @param p_owner Target tree.
 * @return Last iterator, or NULL if empty.
 */
PifBstIterator pifBst_End(PifBst *p_owner);

/**
 * @brief Advances an iterator to the node with the next larger key.
 * @param it Current iterator.
 * @return Next iterator, or NULL at end.
 */
PifBstIterator pifBst_Next(PifBstIterator it);

/**
 * @brief Moves an iterator to the node with the next smaller key.
 * @param it Current iterator.
 * @return Previous iterator, or NULL at begin.
 */
PifBstIterator pifBst_Prev(PifBstIterator it);

#ifdef __cplusplus
}
#endif


#endif	// PIF_BST_H
