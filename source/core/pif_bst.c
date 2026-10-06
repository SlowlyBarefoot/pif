// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_bst.h"


static uint8_t _height(PifBstNode *p_node)
{
    return p_node ? p_node->height : 0;
}

static int _balance(PifBstNode *p_node)
{
    return (int)_height(p_node->p_small) - (int)_height(p_node->p_big);
}

static void _updateHeight(PifBstNode *p_node)
{
    p_node->height = 1 + MAX(_height(p_node->p_small), _height(p_node->p_big));
}

static PifBstNode *_leftmost(PifBstNode *p_node)
{
    while (p_node->p_small) p_node = p_node->p_small;
    return p_node;
}

static PifBstNode *_rightmost(PifBstNode *p_node)
{
    while (p_node->p_big) p_node = p_node->p_big;
    return p_node;
}

// Puts p_new where p_old hangs under p_parent (or at the root).
static void _replaceChild(PifBst *p_owner, PifBstNode *p_parent, PifBstNode *p_old, PifBstNode *p_new)
{
    if (!p_parent) {
        p_owner->p_root = p_new;
    }
    else if (p_parent->p_small == p_old) {
        p_parent->p_small = p_new;
    }
    else {
        p_parent->p_big = p_new;
    }
    if (p_new) p_new->p_parent = p_parent;
}

static PifBstNode *_rotateRight(PifBst *p_owner, PifBstNode *p_node)
{
    PifBstNode *p_top = p_node->p_small;

    _replaceChild(p_owner, p_node->p_parent, p_node, p_top);
    p_node->p_small = p_top->p_big;
    if (p_node->p_small) p_node->p_small->p_parent = p_node;
    p_top->p_big = p_node;
    p_node->p_parent = p_top;
    _updateHeight(p_node);
    _updateHeight(p_top);
    return p_top;
}

static PifBstNode *_rotateLeft(PifBst *p_owner, PifBstNode *p_node)
{
    PifBstNode *p_top = p_node->p_big;

    _replaceChild(p_owner, p_node->p_parent, p_node, p_top);
    p_node->p_big = p_top->p_small;
    if (p_node->p_big) p_node->p_big->p_parent = p_node;
    p_top->p_small = p_node;
    p_node->p_parent = p_top;
    _updateHeight(p_node);
    _updateHeight(p_top);
    return p_top;
}

// Restores the AVL invariant from p_node up to the root after an insert or a
// removal below it. Stops early once a subtree keeps its height, because no
// ancestor above it can have changed.
static void _rebalance(PifBst *p_owner, PifBstNode *p_node)
{
    uint8_t old_height;
    int balance;

    while (p_node) {
        old_height = p_node->height;
        _updateHeight(p_node);
        balance = _balance(p_node);
        if (balance > 1) {
            if (_balance(p_node->p_small) < 0) _rotateLeft(p_owner, p_node->p_small);
            p_node = _rotateRight(p_owner, p_node);
        }
        else if (balance < -1) {
            if (_balance(p_node->p_big) > 0) _rotateRight(p_owner, p_node->p_big);
            p_node = _rotateLeft(p_owner, p_node);
        }
        else if (p_node->height == old_height) {
            break;
        }
        p_node = p_node->p_parent;
    }
}

// Returns the node holding the key, creating it when missing. *p_added tells
// which happened. NULL means the heap is exhausted.
static PifBstNode *_insert(PifBst *p_owner, PifBstKey key, BOOL *p_added)
{
    PifBstNode *p_parent = NULL;
    PifBstNode **pp_link = &p_owner->p_root;
    PifBstNode *p_node;

    *p_added = FALSE;
    while (*pp_link) {
        p_parent = *pp_link;
        if (key < p_parent->key) {
            pp_link = &p_parent->p_small;
        }
        else if (key > p_parent->key) {
            pp_link = &p_parent->p_big;
        }
        else {
            return p_parent;
        }
    }

    p_node = calloc(1, offsetof(PifBstNode, data) + p_owner->data_size);
    if (!p_node) {
        pif_error = E_OUT_OF_HEAP;
        return NULL;
    }

    p_node->key = key;
    p_node->height = 1;
    p_node->p_parent = p_parent;
    *pp_link = p_node;
    p_owner->size++;
    *p_added = TRUE;

    _rebalance(p_owner, p_parent);
    return p_node;
}

BOOL pifBst_Init(PifBst *p_owner, int data_size)
{
    if (!p_owner || data_size < 0) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

    memset(p_owner, 0, sizeof(PifBst));
    p_owner->data_size = data_size;
    return TRUE;
}

void pifBst_Clear(PifBst *p_owner, PifEvtBstClear evt_clear)
{
    PifBstNode *p_node;
    PifBstNode *p_parent;

    if (!p_owner) return;

    // Post-order walk over the parent links: free a node once both children are gone.
    p_node = p_owner->p_root;
    while (p_node) {
        if (p_node->p_small) {
            p_node = p_node->p_small;
        }
        else if (p_node->p_big) {
            p_node = p_node->p_big;
        }
        else {
            p_parent = p_node->p_parent;
            if (p_parent) {
                if (p_parent->p_small == p_node) p_parent->p_small = NULL;
                else p_parent->p_big = NULL;
            }
            if (evt_clear) (*evt_clear)(p_node->key, p_node->data);
            free(p_node);
            p_node = p_parent;
        }
    }
    p_owner->p_root = NULL;
    p_owner->size = 0;
}

void *pifBst_Add(PifBst *p_owner, PifBstKey key)
{
    BOOL added;
    PifBstNode *p_node = _insert(p_owner, key, &added);

    if (!p_node) return NULL;
    if (!added) {
        pif_error = E_ALREADY_ATTACHED;
        return NULL;
    }
    return p_node->data;
}

void *pifBst_Set(PifBst *p_owner, PifBstKey key, const void *p_data)
{
    BOOL added;
    PifBstNode *p_node = _insert(p_owner, key, &added);

    if (!p_node) return NULL;
    if (p_data) memcpy(p_node->data, p_data, p_owner->data_size);
    return p_node->data;
}

BOOL pifBst_Remove(PifBst *p_owner, PifBstKey key)
{
    PifBstIterator it = pifBst_Find(p_owner, key);

    if (!it) return FALSE;
    pifBst_Erase(p_owner, it);
    return TRUE;
}

PifBstIterator pifBst_Erase(PifBst *p_owner, PifBstIterator it)
{
    PifBstIterator next = pifBst_Next(it);
    PifBstNode *p_successor;
    PifBstNode *p_child;
    PifBstNode *p_from;

    if (it->p_small && it->p_big) {
        // Move the in-order successor into the removed node's place. Nodes are
        // relinked rather than their payloads copied, so pointers to other
        // payloads stay valid.
        p_successor = next;
        if (p_successor->p_parent == it) {
            p_from = p_successor;
        }
        else {
            p_from = p_successor->p_parent;
            p_from->p_small = p_successor->p_big;
            if (p_successor->p_big) p_successor->p_big->p_parent = p_from;
            p_successor->p_big = it->p_big;
            p_successor->p_big->p_parent = p_successor;
        }
        p_successor->p_small = it->p_small;
        p_successor->p_small->p_parent = p_successor;
        p_successor->height = it->height;
        _replaceChild(p_owner, it->p_parent, it, p_successor);
    }
    else {
        p_child = it->p_small ? it->p_small : it->p_big;
        p_from = it->p_parent;
        _replaceChild(p_owner, p_from, it, p_child);
    }

    free(it);
    p_owner->size--;
    _rebalance(p_owner, p_from);
    return next;
}

int pifBst_Size(PifBst *p_owner)
{
    return p_owner->size;
}

PifBstIterator pifBst_Find(PifBst *p_owner, PifBstKey key)
{
    PifBstNode *p_node = p_owner->p_root;

    while (p_node) {
        if (key < p_node->key) {
            p_node = p_node->p_small;
        }
        else if (key > p_node->key) {
            p_node = p_node->p_big;
        }
        else {
            return p_node;
        }
    }
    return NULL;
}

void *pifBst_FindData(PifBst *p_owner, PifBstKey key)
{
    PifBstIterator it = pifBst_Find(p_owner, key);

    return it ? it->data : NULL;
}

PifBstIterator pifBst_LowerBound(PifBst *p_owner, PifBstKey key)
{
    PifBstNode *p_node = p_owner->p_root;
    PifBstNode *p_bound = NULL;

    while (p_node) {
        if (p_node->key >= key) {
            p_bound = p_node;
            p_node = p_node->p_small;
        }
        else {
            p_node = p_node->p_big;
        }
    }
    return p_bound;
}

PifBstIterator pifBst_Begin(PifBst *p_owner)
{
    return p_owner->p_root ? _leftmost(p_owner->p_root) : NULL;
}

PifBstIterator pifBst_End(PifBst *p_owner)
{
    return p_owner->p_root ? _rightmost(p_owner->p_root) : NULL;
}

PifBstIterator pifBst_Next(PifBstIterator it)
{
    PifBstNode *p_parent;

    if (it->p_big) return _leftmost(it->p_big);

    // Climb until we leave a small subtree; that parent is the successor.
    p_parent = it->p_parent;
    while (p_parent && it == p_parent->p_big) {
        it = p_parent;
        p_parent = p_parent->p_parent;
    }
    return p_parent;
}

PifBstIterator pifBst_Prev(PifBstIterator it)
{
    PifBstNode *p_parent;

    if (it->p_small) return _rightmost(it->p_small);

    p_parent = it->p_parent;
    while (p_parent && it == p_parent->p_small) {
        it = p_parent;
        p_parent = p_parent->p_parent;
    }
    return p_parent;
}
