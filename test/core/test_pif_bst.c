// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_bst.h"

#include <stdio.h>


// Only pif_bst.c is linked, so the global normally defined in pif.c lives here.
PifError pif_error = E_SUCCESS;

// Keys must fit in PifBstKey, so the 8-bit build works on a smaller range.
#if PIF_BST_KEY == 8
#define KEY_RANGE		200
#else
#define KEY_RANGE		4000
#endif

#define RANDOM_STEPS	200000

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

typedef struct StPayload
{
    uint32_t tag;
    double value;
} Payload;

static BOOL s_failed;
static unsigned char s_present[KEY_RANGE];
static int s_cleared;


static uint32_t _tag(PifBstKey key)
{
    return (uint32_t)key * 7u + 1u;
}

// Fills *p_count with the node count and returns the subtree height, or -1 on
// a broken invariant (order, parent link, AVL balance, cached height, payload).
static int _checkNode(PifBstNode *p_node, PifBstNode *p_parent, long lo, long hi, int *p_count)
{
    int small, big;

    if (!p_node) return 0;
    if (p_node->p_parent != p_parent) return -1;
    if ((long)p_node->key <= lo || (long)p_node->key >= hi) return -1;
    if ((uintptr_t)p_node->data % sizeof(double)) return -1;
    if (((Payload *)p_node->data)->tag != _tag(p_node->key)) return -1;

    small = _checkNode(p_node->p_small, p_node, lo, p_node->key, p_count);
    big = _checkNode(p_node->p_big, p_node, p_node->key, hi, p_count);
    if (small < 0 || big < 0) return -1;
    if (small - big > 1 || big - small > 1) return -1;
    if (p_node->height != 1 + MAX(small, big)) return -1;

    (*p_count)++;
    return p_node->height;
}

// Compares the whole tree with s_present: invariants, size, and both iteration directions.
static BOOL _verify(PifBst *p_bst)
{
    PifBstIterator it;
    int count = 0, expect = 0, seen, i;
    long last;

    if (_checkNode(p_bst->p_root, NULL, -1, 0x7FFFFFFFL, &count) < 0) return FALSE;
    for (i = 0; i < KEY_RANGE; i++) expect += s_present[i];
    if (count != expect || pifBst_Size(p_bst) != expect) return FALSE;

    seen = 0;
    last = -1;
    for (it = pifBst_Begin(p_bst); it; it = pifBst_Next(it)) {
        if ((long)it->key <= last || !s_present[it->key]) return FALSE;
        last = it->key;
        seen++;
    }
    if (seen != expect) return FALSE;

    seen = 0;
    last = KEY_RANGE;
    for (it = pifBst_End(p_bst); it; it = pifBst_Prev(it)) {
        if ((long)it->key >= last) return FALSE;
        last = it->key;
        seen++;
    }
    return seen == expect;
}

static void *_addTagged(PifBst *p_bst, PifBstKey key)
{
    Payload *p_payload = pifBst_Add(p_bst, key);

    if (p_payload) {
        p_payload->tag = _tag(key);
        s_present[key] = 1;
    }
    return p_payload;
}

static void _onClear(PifBstKey key, void *p_data)
{
    if (((Payload *)p_data)->tag == _tag(key)) s_cleared++;
}

static void _reset(PifBst *p_bst)
{
    pifBst_Clear(p_bst, NULL);
    memset(s_present, 0, sizeof(s_present));
}


static void testInit(void)
{
    PifBst bst;

    CHECK(!pifBst_Init(NULL, 4));
    CHECK(!pifBst_Init(&bst, -1));
    CHECK(pif_error == E_INVALID_PARAM);
    CHECK(pifBst_Init(&bst, sizeof(Payload)));
    CHECK(pifBst_Size(&bst) == 0);
    CHECK(pifBst_Begin(&bst) == NULL);
    CHECK(pifBst_End(&bst) == NULL);
    CHECK(pifBst_Find(&bst, 3) == NULL);
    CHECK(pifBst_FindData(&bst, 3) == NULL);
    CHECK(pifBst_LowerBound(&bst, 0) == NULL);
    CHECK(!pifBst_Remove(&bst, 3));
}

static void testAddAndSet(void)
{
    PifBst bst;
    Payload *p_first;
    Payload *p_again;
    Payload value = { 0, 1.5 };

    CHECK(pifBst_Init(&bst, sizeof(Payload)));

    p_first = pifBst_Add(&bst, 10);
    CHECK(p_first != NULL);
    CHECK(p_first->tag == 0 && p_first->value == 0.0);		// Zero-filled.

    pif_error = E_SUCCESS;
    CHECK(pifBst_Add(&bst, 10) == NULL);
    CHECK(pif_error == E_ALREADY_ATTACHED);
    CHECK(pifBst_Size(&bst) == 1);

    // Set on an existing key overwrites in place.
    value.tag = 77;
    p_again = pifBst_Set(&bst, 10, &value);
    CHECK(p_again == p_first);
    CHECK(p_first->tag == 77 && p_first->value == 1.5);
    CHECK(pifBst_Size(&bst) == 1);

    // Set with NULL is get-or-add.
    CHECK(pifBst_Set(&bst, 10, NULL) == p_first);
    CHECK(p_first->tag == 77);
    p_again = pifBst_Set(&bst, 20, NULL);
    CHECK(p_again != NULL && p_again->tag == 0);
    CHECK(pifBst_Size(&bst) == 2);
    CHECK(pifBst_FindData(&bst, 20) == p_again);

    pifBst_Clear(&bst, NULL);
}

static void testSequentialStaysBalanced(void)
{
    PifBst bst;
    int count = KEY_RANGE < 1024 ? KEY_RANGE : 1024;
    int limit = 0, i;

    // AVL height bound is about 1.44 * log2(n); the exact log2 ceiling + 1 is a safe floor check.
    while ((1 << limit) <= count) limit++;

    CHECK(pifBst_Init(&bst, sizeof(Payload)));
    memset(s_present, 0, sizeof(s_present));

    for (i = 0; i < count; i++) CHECK(_addTagged(&bst, (PifBstKey)i));
    CHECK(_verify(&bst));
    CHECK(bst.p_root->height <= limit + 1);

    for (i = count - 1; i >= 0; i--) {
        CHECK(pifBst_Remove(&bst, (PifBstKey)i));
        s_present[i] = 0;
        if (i % 37 == 0) CHECK(_verify(&bst));
    }
    CHECK(bst.p_root == NULL);
    CHECK(pifBst_Size(&bst) == 0);
}

static void testRandomAgainstReference(void)
{
    PifBst bst;
    PifBstIterator it;
    PifBstKey key;
    uint32_t expect;
    int step, op;

    CHECK(pifBst_Init(&bst, sizeof(Payload)));
    memset(s_present, 0, sizeof(s_present));
    srand(1234);

    for (step = 0; step < RANDOM_STEPS; step++) {
        key = (PifBstKey)(rand() % KEY_RANGE);
        op = rand() % 3;
        if (op == 0) {
            BOOL existed = s_present[key];
            CHECK((_addTagged(&bst, key) != NULL) == !existed);
        }
        else if (op == 1) {
            CHECK(pifBst_Remove(&bst, key) == s_present[key]);
            s_present[key] = 0;
        }
        else {
            CHECK((pifBst_FindData(&bst, key) != NULL) == s_present[key]);
            it = pifBst_LowerBound(&bst, key);
            for (expect = key; expect < KEY_RANGE && !s_present[expect]; expect++);
            if (expect == KEY_RANGE) CHECK(it == NULL);
            else CHECK(it && it->key == expect);
        }
        if (step % 5000 == 0) CHECK(_verify(&bst));
    }
    CHECK(_verify(&bst));
    _reset(&bst);
}

static void testEraseWhileIterating(void)
{
    PifBst bst;
    PifBstIterator it;
    void *p_kept[KEY_RANGE];
    int i;

    CHECK(pifBst_Init(&bst, sizeof(Payload)));
    memset(s_present, 0, sizeof(s_present));
    for (i = 0; i < KEY_RANGE; i += 3) CHECK(_addTagged(&bst, (PifBstKey)i));
    for (i = 0; i < KEY_RANGE; i++) p_kept[i] = pifBst_FindData(&bst, (PifBstKey)i);

    // Remove every even key in one pass; Erase hands back the next iterator.
    it = pifBst_Begin(&bst);
    while (it) {
        if (it->key % 2 == 0) {
            s_present[it->key] = 0;
            it = pifBst_Erase(&bst, it);
        }
        else {
            it = pifBst_Next(it);
        }
    }
    CHECK(_verify(&bst));

    // Survivors keep their payload address, because nodes are relinked, not copied.
    for (i = 1; i < KEY_RANGE; i += 2) {
        if (s_present[i]) CHECK(pifBst_FindData(&bst, (PifBstKey)i) == p_kept[i]);
    }
    _reset(&bst);
}

static void testClearCallback(void)
{
    PifBst bst;
    int i, count;

    CHECK(pifBst_Init(&bst, sizeof(Payload)));
    memset(s_present, 0, sizeof(s_present));
    for (i = 0; i < KEY_RANGE; i += 2) CHECK(_addTagged(&bst, (PifBstKey)i));
    count = pifBst_Size(&bst);

    s_cleared = 0;
    pifBst_Clear(&bst, _onClear);
    CHECK(s_cleared == count);
    CHECK(bst.p_root == NULL);
    CHECK(pifBst_Size(&bst) == 0);

    // The tree is reusable after Clear.
    CHECK(pifBst_Add(&bst, 1) != NULL);
    pifBst_Clear(&bst, NULL);
    pifBst_Clear(NULL, NULL);
}

static void testKeyOnlySet(void)
{
    PifBst set;

    CHECK(pifBst_Init(&set, 0));
    CHECK(pifBst_Add(&set, 9) != NULL);
    CHECK(pifBst_Add(&set, 9) == NULL);
    CHECK(pifBst_Find(&set, 9) != NULL);
    CHECK(pifBst_Remove(&set, 9));
    CHECK(pifBst_Size(&set) == 0);
    pifBst_Clear(&set, NULL);
}


typedef struct StTestCase
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "init", testInit },
    { "add and set", testAddAndSet },
    { "sequential stays balanced", testSequentialStaysBalanced },
    { "random against reference", testRandomAgainstReference },
    { "erase while iterating", testEraseWhileIterating },
    { "clear callback", testClearCallback },
    { "key-only set", testKeyOnlySet }
};

int main(void)
{
    int failed = 0;
    size_t i;

    printf("pif_bst (PIF_BST_KEY=%d)\n", PIF_BST_KEY);
    for (i = 0; i < sizeof(s_tests) / sizeof(s_tests[0]); i++) {
        s_failed = FALSE;
        (*s_tests[i].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
