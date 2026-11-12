// SPDX-License-Identifier: BSD-3-Clause
// pif_sd_spi on an SD card in memory: the start-up of each card type, byte and block addresses, single and multiple
// block reads and writes, and the failures: no card, a card that never gets ready, a rejected write, bad requests.
#include "storage/pif_sd_spi.h"
#include "sd_spi_emu.h"


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
static PifSdSpi s_sd;
static uint8_t s_buf[4 * PIF_SD_SPI_SECTOR_SIZE];


static uint32_t _timer1us(void)
{
    return 0;
}

static BOOL _setUp(EmuCardKind kind)
{
    _emuReset(kind);
    return pifSdSpi_Init(&s_sd, PIF_ID_AUTO, _emuSelect, _emuTransfer, _emuSetClock);
}

static void _fill(uint8_t *p_data, size_t size, uint8_t seed)
{
    size_t i;

    for (i = 0; i < size; i++) p_data[i] = (uint8_t)(seed + i * 13);
}

static void testInitParams(void)
{
    CHECK(!pifSdSpi_Init(&s_sd, PIF_ID_AUTO, NULL, _emuTransfer, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSdSpi_Init(&s_sd, PIF_ID_AUTO, _emuSelect, NULL, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(_setUp(EMU_SDHC));
    CHECK(s_sd._type == SDST_NONE);
    // Nothing before a start.
    CHECK(!pifSdSpi_Read(&s_sd, 0, s_buf, 1) && pif_error == E_INVALID_STATE);
}

static void testNoCard(void)
{
    CHECK(_setUp(EMU_NO_CARD));
    CHECK(!pifSdSpi_Start(&s_sd));
    CHECK(pif_error == E_CANNOT_FOUND);
    CHECK(s_sd._fail_step == SDSS_CMD0 && s_sd._type == SDST_NONE);
}

static void testSdhc(void)
{
    CHECK(_setUp(EMU_SDHC));
    CHECK(pifSdSpi_Start(&s_sd));
    CHECK(s_sd._type == SDST_SDHC);
    CHECK(s_sd._sectors == EMU_SECTORS);
    CHECK(s_sd._ocr & (1UL << 30));
    CHECK(s_sd._fail_step == SDSS_NONE);
    CHECK(s_emu_fast == 1);								// full clock after the start
    CHECK(s_emu_cmd_count[16] == 0);					// no block length on a high capacity card

    // One sector: block address.
    _fill(s_buf, PIF_SD_SPI_SECTOR_SIZE, 1);
    CHECK(pifSdSpi_Write(&s_sd, 100, s_buf, 1));
    CHECK(!memcmp(s_emu_media[100], s_buf, PIF_SD_SPI_SECTOR_SIZE));
    memset(s_buf, 0, sizeof(s_buf));
    CHECK(pifSdSpi_Read(&s_sd, 100, s_buf, 1));
    CHECK(s_buf[1] == (uint8_t)(1 + 13) && s_buf[511] == (uint8_t)(1 + 511 * 13));
}

static void testMultiBlock(void)
{
    uint8_t expect[4 * PIF_SD_SPI_SECTOR_SIZE];

    CHECK(_setUp(EMU_SDHC));
    CHECK(pifSdSpi_Start(&s_sd));
    _fill(expect, sizeof(expect), 7);
    CHECK(pifSdSpi_Write(&s_sd, 200, expect, 4));
    CHECK(s_emu_cmd_count[25] == 1 && s_emu_cmd_count[24] == 0);
    CHECK(!memcmp(s_emu_media[200], expect, sizeof(expect)));

    memset(s_buf, 0, sizeof(s_buf));
    CHECK(pifSdSpi_Read(&s_sd, 200, s_buf, 4));
    CHECK(s_emu_cmd_count[18] == 1 && s_emu_cmd_count[12] == 1);
    CHECK(!memcmp(s_buf, expect, sizeof(expect)));

    // Still in step with the card after the stop: a single read right after.
    CHECK(pifSdSpi_Read(&s_sd, 201, s_buf, 1));
    CHECK(!memcmp(s_buf, expect + PIF_SD_SPI_SECTOR_SIZE, PIF_SD_SPI_SECTOR_SIZE));
}

static void testStandardCapacity(void)
{
    // Version 2, byte addresses: sector 3 is byte 1536.
    CHECK(_setUp(EMU_SDSC_V2));
    CHECK(pifSdSpi_Start(&s_sd));
    CHECK(s_sd._type == SDST_SDSC_V2);
    CHECK(s_sd._sectors == EMU_SECTORS);
    CHECK(s_emu_cmd_count[16] == 1);
    _fill(s_buf, PIF_SD_SPI_SECTOR_SIZE, 3);
    CHECK(pifSdSpi_Write(&s_sd, 3, s_buf, 1));
    CHECK(!memcmp(s_emu_media[3], s_buf, PIF_SD_SPI_SECTOR_SIZE));

    // Version 1: CMD8 is illegal to it, no OCR.
    CHECK(_setUp(EMU_SDSC_V1));
    CHECK(pifSdSpi_Start(&s_sd));
    CHECK(s_sd._type == SDST_SDSC_V1);
    CHECK(s_sd._ocr == 0 && s_emu_cmd_count[58] == 0);
    CHECK(pifSdSpi_Read(&s_sd, 3, s_buf, 2));
    CHECK(!memcmp(s_buf, s_emu_media[3], 2 * PIF_SD_SPI_SECTOR_SIZE));
}

static void testNeverReady(void)
{
    CHECK(_setUp(EMU_SDHC));
    s_emu_acmd41_needed = -1;
    s_sd.init_timeout = 50;
    CHECK(!pifSdSpi_Start(&s_sd));
    CHECK(pif_error == E_TIMEOUT && s_sd._fail_step == SDSS_ACMD41);
    CHECK(s_sd._type == SDST_NONE);
}

static void testBadRequests(void)
{
    CHECK(_setUp(EMU_SDHC));
    CHECK(pifSdSpi_Start(&s_sd));
    CHECK(!pifSdSpi_Read(&s_sd, EMU_SECTORS, s_buf, 1) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSdSpi_Write(&s_sd, EMU_SECTORS - 1, s_buf, 2) && pif_error == E_INVALID_PARAM);
    CHECK(!pifSdSpi_Read(&s_sd, 0, s_buf, 0) && pif_error == E_INVALID_PARAM);

    // A sector the card rejects; the card is still usable afterwards.
    s_emu_reject_write = TRUE;
    CHECK(!pifSdSpi_Write(&s_sd, 5, s_buf, 1));
    CHECK(pif_error == E_WRONG_DATA && s_sd._fail_step == SDSS_WRITE);
    s_emu_reject_write = FALSE;
    _fill(s_buf, PIF_SD_SPI_SECTOR_SIZE, 9);
    CHECK(pifSdSpi_Write(&s_sd, 5, s_buf, 1));
    CHECK(!memcmp(s_emu_media[5], s_buf, PIF_SD_SPI_SECTOR_SIZE));
}

typedef struct
{
    const char *p_name;
    void (*run)(void);
} TestCase;

static const TestCase s_tests[] = {
    { "init parameters, no start", testInitParams },
    { "no card", testNoCard },
    { "SDHC start, one sector", testSdhc },
    { "multiple blocks", testMultiBlock },
    { "standard capacity v1, v2", testStandardCapacity },
    { "card never ready", testNeverReady },
    { "bad requests, rejected write", testBadRequests },
};

static BOOL _portSetup(void)
{
    return TRUE;
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_sd_spi: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_sd_spi\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
