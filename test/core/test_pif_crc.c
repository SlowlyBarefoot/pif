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

// The check string of the CRC catalogues: each CRC below is compared with its published check value.
static const uint8_t kCheck[] = "123456789";
#define CHECK_LEN	9


static void _testCrc7(void)
{
    CHECK(pifCrc7((uint8_t*)kCheck, CHECK_LEN) == 0xEB);	// CRC-7/MMC 0x75, shifted left with the end bit
}

static void _testCrc8(void)
{
    CHECK(pifCrc8(kCheck, CHECK_LEN, PIF_CRC8_POLY_DVB_S2) == 0xBC);
    CHECK(pifCrc8_Update(pifCrc8(kCheck, 4, PIF_CRC8_POLY_DVB_S2), kCheck + 4, 5, PIF_CRC8_POLY_DVB_S2) == 0xBC);
}

// CRC-8 of poly bit by bit, the reference the functions are checked against.
static uint8_t _crc8Bitwise(uint8_t crc, uint8_t data, uint8_t poly)
{
    int i;

    crc ^= data;
    for (i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ poly) : (uint8_t)(crc << 1);
    return crc;
}

// CRC-8/SMBUS, from the table with PIF_USE_CRC8_SMBUS_TABLE (test_pif_crc_table) and bit by bit without it:
// the check value, the three functions and every byte against the reference. _testCrc8 checks with the table
// that the other polynomials still go bit by bit.
static void _testCrc8Smbus(void)
{
    int i;
    uint8_t byte;

    CHECK(pifCrc8(kCheck, CHECK_LEN, PIF_CRC8_POLY_SMBUS) == 0xF4);
    CHECK(pifCrc8_Update(pifCrc8(kCheck, 4, PIF_CRC8_POLY_SMBUS), kCheck + 4, 5, PIF_CRC8_POLY_SMBUS) == 0xF4);
    CHECK(pifCrc8(kCheck, 0, PIF_CRC8_POLY_SMBUS) == 0);
    for (i = 0; i < 256; i++) {
        byte = (uint8_t)i;
        CHECK(pifCrc8(&byte, 1, PIF_CRC8_POLY_SMBUS) == _crc8Bitwise(0, byte, PIF_CRC8_POLY_SMBUS));
        CHECK(pifCrc8_Add(0x5A, byte, PIF_CRC8_POLY_SMBUS) == _crc8Bitwise(0x5A, byte, PIF_CRC8_POLY_SMBUS));
        CHECK(pifCrc8_Add(0x5A, byte, PIF_CRC8_POLY_DVB_S2) == _crc8Bitwise(0x5A, byte, PIF_CRC8_POLY_DVB_S2));
#ifdef PIF_USE_CRC8_SMBUS_TABLE
        CHECK(pif_crc8_smbus_table[i] == _crc8Bitwise(0, byte, PIF_CRC8_POLY_SMBUS));
#endif
    }
}

static void _testCrc16(void)
{
    CHECK(pifCrc16((uint8_t*)kCheck, CHECK_LEN) == 0x31C3);		// CRC-16/XMODEM
    CHECK(pifCrc16_Update(0xFFFF, kCheck, CHECK_LEN) == 0x29B1);	// CRC-16/CCITT-FALSE
}

static void _testCrc32(void)
{
    uint8_t zeros[300];
    uint32_t i, crc;

    CHECK(pifCrc32(kCheck, CHECK_LEN) == 0xCBF43926UL);
    CHECK(pifCrc32(kCheck, 0) == 0);
    for (i = 0; i <= CHECK_LEN; i++) {
        CHECK(pifCrc32_Update(pifCrc32_Update(0, kCheck, i), kCheck + i, CHECK_LEN - i) == 0xCBF43926UL);
    }
    memset(zeros, 0, sizeof(zeros));
    crc = 0;
    for (i = 0; i < sizeof(zeros); i += 7) crc = pifCrc32_Update(crc, zeros + i, MIN(7, sizeof(zeros) - i));
    CHECK(crc == pifCrc32(zeros, sizeof(zeros)));
}

typedef struct
{
    const char* p_name;
    void (*test)(void);
} TestCase;

int main(void)
{
    static const TestCase kTests[] = {
        { "crc7", _testCrc7 },
        { "crc8", _testCrc8 },
#ifdef PIF_USE_CRC8_SMBUS_TABLE
        { "crc8 smbus (table)", _testCrc8Smbus },
#else
        { "crc8 smbus", _testCrc8Smbus },
#endif
        { "crc16", _testCrc16 },
        { "crc32", _testCrc32 },
    };
    unsigned i, failed = 0;

    printf("test_pif_crc\n");
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    return failed ? 1 : 0;
}
