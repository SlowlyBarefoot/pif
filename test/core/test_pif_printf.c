// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif.h"

#include <stdio.h>
#include <string.h>


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;


static void testInteger(void)
{
    char buffer[32];

    pif_Printf(buffer, sizeof(buffer), "%d,%u,%x,%X,%s,%c", -12, 34u, 0xabu, 0xCDu, "ok", 'z');
    CHECK(!strcmp(buffer, "-12,34,ab,CD,ok,z"));
    pif_Printf(buffer, sizeof(buffer), "%4d|%%", 7);
    CHECK(!strcmp(buffer, "0007|%"));
}

static void testFloat(void)
{
    char buffer[32];

    pif_Printf(buffer, sizeof(buffer), "%2f|%d", 3.25, 9);
#ifdef PIF_USE_PRINTF_FLOAT
    CHECK(!strcmp(buffer, "3.25|9"));
#else
    // Nothing is written for it, and the argument after it is still the right one.
    CHECK(!strcmp(buffer, "|9"));
#endif
}

static void testFloatDropped(void)
{
    char buffer[16];

    // No room for the number, which is given 20 characters: it is dropped, and the argument after it
    // is still the right one.
    pif_Printf(buffer, sizeof(buffer), "ab%f%d", 1.5, 4);
    CHECK(!strcmp(buffer, "ab4"));
}

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char* name;
        void (*fn)(void);
    } tests[] = {
        { "integer", testInteger },
        { "float", testFloat },
        { "float dropped", testFloatDropped },
    };

#ifdef PIF_USE_PRINTF_FLOAT
    printf("pif_printf (PIF_USE_PRINTF_FLOAT)\n");
#else
    printf("pif_printf\n");
#endif
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        (*tests[t].fn)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
