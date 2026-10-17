// SPDX-License-Identifier: BSD-3-Clause
// pif_nor_flash over an SPI NOR flash in memory: detection, programs cut at the page boundaries, erases waited for
// and left alone.
#include "storage/pif_nor_flash.h"
#include "nor_flash_emu.h"


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
static PifNorFlash s_flash;


static uint32_t _timer1us(void)
{
    return 0;
}

static BOOL _setUp(void)
{
    // A failed test returns early, so free what it left behind.
    pifNorFlash_Clear(&s_flash);
    if (!_emuInit()) return FALSE;
    s_emu_jedec[0] = 0xEF;
    s_emu_jedec[1] = 0x40;
    s_emu_jedec[2] = 0x10;
    s_emu_busy_reads = 2;
    return pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0);
}

static void _fill(uint8_t* p_data, size_t size, uint8_t seed)
{
    size_t i;

    for (i = 0; i < size; i++) p_data[i] = (uint8_t)(seed + i * 7);
}

static void testInit(void)
{
    CHECK(_setUp());
    CHECK(s_flash._capacity == EMU_CAPACITY);
    CHECK(s_flash._jedec_id[0] == 0xEF && s_flash._jedec_id[1] == 0x40 && s_flash._jedec_id[2] == 0x10);

    // Given, the capacity is taken as it is, up to what three address bytes reach.
    pifNorFlash_Clear(&s_flash);
    CHECK(pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0x8000));
    CHECK(s_flash._capacity == 0x8000);
    pifNorFlash_Clear(&s_flash);
    s_emu_jedec[2] = 0x19;
    CHECK(pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0));
    CHECK(s_flash._capacity == PIF_NOR_FLASH_MAX_CAPACITY);

    // An ID that does not give the capacity as a power of two needs it given.
    pifNorFlash_Clear(&s_flash);
    s_emu_jedec[2] = 0x05;
    CHECK(!pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0) && pif_error == E_WRONG_DATA);
    CHECK(s_flash._p_device == NULL);

    // Nothing on the bus.
    memset(s_emu_jedec, 0xFF, sizeof(s_emu_jedec));
    CHECK(!pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0) && pif_error == E_CANNOT_FOUND);
    memset(s_emu_jedec, 0x00, sizeof(s_emu_jedec));
    CHECK(!pifNorFlash_Init(&s_flash, PIF_ID_AUTO, &s_emu_port, NULL, 0) && pif_error == E_CANNOT_FOUND);
}

static void testProgramRead(void)
{
    uint8_t data[600], read[600];

    CHECK(_setUp());
    _fill(data, sizeof(data), 0x11);

    // From the middle of a page into the fourth: 56, 256, 256 and 32 bytes.
    CHECK(pifNorFlash_Program(&s_flash, 200, data, sizeof(data)));
    CHECK(s_emu_program_count == 4 && !s_emu_crossed_page && !s_emu_ignored);
    CHECK(memcmp(s_emu_media + 200, data, sizeof(data)) == 0);
    CHECK(s_emu_media[199] == 0xFF && s_emu_media[800] == 0xFF);

    memset(read, 0, sizeof(read));
    CHECK(pifNorFlash_Read(&s_flash, 200, read, sizeof(read)));
    CHECK(memcmp(read, data, sizeof(data)) == 0);

    // A port that takes less than a page gets programs of its size, each with its own write enable.
    s_emu_program_count = 0;
    s_flash._p_device->max_transfer_size = 64;
    CHECK(pifNorFlash_Program(&s_flash, 0x1000, data, 256));
    CHECK(s_emu_program_count == 4 && !s_emu_crossed_page && !s_emu_ignored);
    CHECK(memcmp(s_emu_media + 0x1000, data, 256) == 0);
    memset(read, 0, sizeof(read));
    CHECK(pifNorFlash_Read(&s_flash, 0x1000, read, 256));
    CHECK(memcmp(read, data, 256) == 0);

    // Nothing past the end.
    CHECK(!pifNorFlash_Program(&s_flash, EMU_CAPACITY - 4, data, 8) && pif_error == E_INVALID_PARAM);
    CHECK(!pifNorFlash_Read(&s_flash, EMU_CAPACITY, read, 1) && pif_error == E_INVALID_PARAM);
    CHECK(pifNorFlash_Read(&s_flash, EMU_CAPACITY - 1, read, 1));
}

static void testEraseSector(void)
{
    uint8_t data[64];

    CHECK(_setUp());
    _fill(data, sizeof(data), 0x22);
    CHECK(pifNorFlash_Program(&s_flash, 0x0FE0, data, sizeof(data)));

    // Any address in the sector erases the whole of it and nothing else.
    CHECK(pifNorFlash_EraseSector(&s_flash, 0x0123));
    CHECK(s_emu_erase_count == 1 && !s_emu_ignored);
    CHECK(s_emu_media[0x0FE0] == 0xFF && s_emu_media[0x0FFF] == 0xFF);
    CHECK(memcmp(s_emu_media + 0x1000, data + 0x20, 0x20) == 0);

    // Programmed again after the erase, it holds the new bytes.
    CHECK(pifNorFlash_Program(&s_flash, 0x0FE0, data, 0x20));
    CHECK(memcmp(s_emu_media + 0x0FE0, data, 0x20) == 0);
}

static void testEraseLeftAlone(void)
{
    uint8_t read[4];
    BOOL busy;
    int polls;

    CHECK(_setUp());
    s_emu_busy_reads = 4;
    CHECK(pifNorFlash_StartEraseSector(&s_flash, 0x2000));

    // Everything else waits for it, without touching the media.
    CHECK(!pifNorFlash_Read(&s_flash, 0, read, sizeof(read)) && pif_error == E_INVALID_STATE);
    CHECK(!pifNorFlash_Program(&s_flash, 0, read, sizeof(read)) && pif_error == E_INVALID_STATE);
    CHECK(!pifNorFlash_StartEraseChip(&s_flash) && pif_error == E_INVALID_STATE);
    CHECK(!s_emu_ignored);

    for (polls = 0; polls < 10; polls++) {
        CHECK(pifNorFlash_CheckBusy(&s_flash, &busy));
        if (!busy) break;
    }
    // Three of its four busy reads went to the refused calls above.
    CHECK(polls == 1);
    CHECK(pifNorFlash_Read(&s_flash, 0, read, sizeof(read)));

    // The whole device, the same way.
    CHECK(pifNorFlash_Program(&s_flash, 0x8000, read, sizeof(read)));
    s_emu_media[0x8000] = 0x00;
    CHECK(pifNorFlash_StartEraseChip(&s_flash));
    do {
        CHECK(pifNorFlash_CheckBusy(&s_flash, &busy));
    } while (busy);
    CHECK(s_emu_media[0x8000] == 0xFF && !s_emu_ignored);
}

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { testInit, "init" },
    { testProgramRead, "program and read" },
    { testEraseSector, "erase sector" },
    { testEraseLeftAlone, "erase left alone" },
};

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us)) {
        printf("pif_nor_flash: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_nor_flash\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifNorFlash_Clear(&s_flash);
    pifSpiPort_Clear(&s_emu_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
