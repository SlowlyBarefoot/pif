// SPDX-License-Identifier: BSD-3-Clause
#include "codec/pif_encoding.h"

#include <stdio.h>
#include <stdlib.h>
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

static uint8_t s_src[1024];
static uint8_t s_enc[1100];
static uint8_t s_dec[1024];

// Fills p with first, first + 1, ... wrapping from 0xFF to 0x00.
static void _ramp(uint8_t *p, uint16_t length, uint8_t first)
{
    while (length--) *p++ = first++;
}

// Encodes src, compares with expected, and decodes back.
static BOOL _vector(const uint8_t *p_src, uint16_t length, const uint8_t *p_expected, uint16_t expected_length)
{
    uint16_t size, decoded;

    size = pifEncoding_CobsEncode(p_src, length, s_enc, sizeof(s_enc));
    if (size != expected_length || memcmp(s_enc, p_expected, size)) return FALSE;
    if (!pifEncoding_CobsDecode(s_enc, size, s_dec, sizeof(s_dec), &decoded)) return FALSE;
    return decoded == length && !memcmp(s_dec, p_src, length);
}

// ---------------------------------------------------------------- COBS

// The examples of the COBS article of Wikipedia.
static void _testCobsVectors(void)
{
    static const uint8_t k1[] = { 0x00 }, e1[] = { 0x01, 0x01 };
    static const uint8_t k2[] = { 0x00, 0x00 }, e2[] = { 0x01, 0x01, 0x01 };
    static const uint8_t k3[] = { 0x00, 0x11, 0x00 }, e3[] = { 0x01, 0x02, 0x11, 0x01 };
    static const uint8_t k4[] = { 0x11, 0x22, 0x00, 0x33 }, e4[] = { 0x03, 0x11, 0x22, 0x02, 0x33 };
    static const uint8_t k5[] = { 0x11, 0x22, 0x33, 0x44 }, e5[] = { 0x05, 0x11, 0x22, 0x33, 0x44 };
    static const uint8_t k6[] = { 0x11, 0x00, 0x00, 0x00 }, e6[] = { 0x02, 0x11, 0x01, 0x01, 0x01 };
    static const uint8_t e0[] = { 0x01 };
    uint8_t expected[300];

    CHECK(_vector(NULL, 0, e0, 1));
    CHECK(_vector(k1, 1, e1, 2));
    CHECK(_vector(k2, 2, e2, 3));
    CHECK(_vector(k3, 3, e3, 4));
    CHECK(_vector(k4, 4, e4, 5));
    CHECK(_vector(k5, 4, e5, 5));
    CHECK(_vector(k6, 4, e6, 5));

    // 01 .. FE: one full block, and no empty block after it.
    _ramp(s_src, 254, 0x01);
    expected[0] = 0xFF;
    _ramp(expected + 1, 254, 0x01);
    CHECK(_vector(s_src, 254, expected, 255));

    // 00 .. FE
    _ramp(s_src, 255, 0x00);
    expected[0] = 0x01;
    expected[1] = 0xFF;
    _ramp(expected + 2, 254, 0x01);
    CHECK(_vector(s_src, 255, expected, 256));

    // 01 .. FF
    _ramp(s_src, 255, 0x01);
    expected[0] = 0xFF;
    _ramp(expected + 1, 254, 0x01);
    expected[255] = 0x02;
    expected[256] = 0xFF;
    CHECK(_vector(s_src, 255, expected, 257));

    // 02 .. FF 00
    _ramp(s_src, 255, 0x02);
    expected[0] = 0xFF;
    _ramp(expected + 1, 254, 0x02);
    expected[255] = 0x01;
    expected[256] = 0x01;
    CHECK(_vector(s_src, 255, expected, 257));

    // 03 .. FF 00 01
    _ramp(s_src, 255, 0x03);
    expected[0] = 0xFE;
    _ramp(expected + 1, 253, 0x03);
    expected[254] = 0x02;
    expected[255] = 0x01;
    CHECK(_vector(s_src, 255, expected, 256));
}

static void _testCobsRoundTrip(void)
{
    uint16_t length, size, decoded, i;

    srand(7);
    for (length = 0; length <= 1000; length += 1 + length / 50) {
        // Sparse, dense and no zeros.
        for (i = 0; i < length; i++) s_src[i] = (uint8_t)(length % 3 == 0 ? rand() : length % 3 == 1 ? rand() % 4 : 1 + rand() % 255);
        size = pifEncoding_CobsEncode(s_src, length, s_enc, sizeof(s_enc));
        CHECK(size && size <= PIF_COBS_MAX_ENCODED(length));
        CHECK(!memchr(s_enc, 0, size));
        CHECK(pifEncoding_CobsDecode(s_enc, size, s_dec, sizeof(s_dec), &decoded));
        CHECK(decoded == length && !memcmp(s_dec, s_src, length));

        // In place.
        memcpy(s_dec, s_enc, size);
        CHECK(pifEncoding_CobsDecode(s_dec, size, s_dec, sizeof(s_dec), &decoded));
        CHECK(decoded == length && !memcmp(s_dec, s_src, length));
    }
}

static void _testCobsLimits(void)
{
    uint16_t size, decoded = 1234;

    // The encoder stops at the end of the destination, whatever byte it is at.
    memset(s_src, 0x5A, 300);
    size = pifEncoding_CobsEncode(s_src, 300, s_enc, sizeof(s_enc));
    CHECK(size == 302);
    for (size = 0; size < 302; size++) CHECK(!pifEncoding_CobsEncode(s_src, 300, s_enc, size));
    CHECK(pifEncoding_CobsEncode(s_src, 300, s_enc, 302) == 302);

    // The decoder refuses a small destination, a 0x00 inside and a cut block.
    pifEncoding_CobsEncode(s_src, 300, s_enc, sizeof(s_enc));
    CHECK(!pifEncoding_CobsDecode(s_enc, 302, s_dec, 299, &decoded));
    CHECK(pifEncoding_CobsDecode(s_enc, 302, s_dec, 300, &decoded) && decoded == 300);
    s_enc[10] = 0;
    CHECK(!pifEncoding_CobsDecode(s_enc, 302, s_dec, sizeof(s_dec), &decoded));
    s_enc[10] = 10;
    CHECK(!pifEncoding_CobsDecode(s_enc, 200, s_dec, sizeof(s_dec), &decoded));
    CHECK(decoded == 300);
}

static int _putAll(PifCobsDecoder *p_decoder, const uint8_t *p_data, uint16_t length)
{
    int result = PIF_COBS_MORE;

    while (length--) {
        result = pifEncoding_CobsDecoderPut(p_decoder, *p_data++);
        if (length && result != PIF_COBS_MORE) return -100;	// Only the last byte may end a frame
    }
    return result;
}

static void _testCobsDecoder(void)
{
    PifCobsDecoder decoder;
    uint8_t buffer[300];
    uint16_t length, size;
    int result;

    pifEncoding_CobsDecoderInit(&decoder, buffer, sizeof(buffer));

    // Frames of every kind, one after another.
    srand(11);
    for (length = 0; length <= 300; length += 7) {
        for (size = 0; size < length; size++) s_src[size] = (uint8_t)(length % 2 ? rand() % 3 : rand());
        size = pifEncoding_CobsEncode(s_src, length, s_enc, sizeof(s_enc));
        s_enc[size++] = 0;
        result = _putAll(&decoder, s_enc, size);
        CHECK(result == length);
        CHECK(!memcmp(buffer, s_src, length));
    }

    // Delimiters alone are no frames.
    CHECK(pifEncoding_CobsDecoderPut(&decoder, 0) == PIF_COBS_MORE);
    CHECK(pifEncoding_CobsDecoderPut(&decoder, 0) == PIF_COBS_MORE);

    // A frame cut short is dropped, and the next one comes through.
    {
        static const uint8_t cut[] = { 0x05, 0x11, 0x22, 0x00 };
        static const uint8_t good[] = { 0x03, 0x11, 0x22, 0x02, 0x33, 0x00 };
        CHECK(_putAll(&decoder, cut, sizeof(cut)) == PIF_COBS_ERROR);
        CHECK(_putAll(&decoder, good, sizeof(good)) == 4);
        CHECK(buffer[0] == 0x11 && buffer[1] == 0x22 && buffer[2] == 0x00 && buffer[3] == 0x33);
    }

    // A frame longer than the buffer is dropped.
    _ramp(s_src, 301, 0x01);
    size = pifEncoding_CobsEncode(s_src, 301, s_enc, sizeof(s_enc));
    s_enc[size++] = 0;
    CHECK(_putAll(&decoder, s_enc, size) == PIF_COBS_ERROR);
    size = pifEncoding_CobsEncode(s_src, 300, s_enc, sizeof(s_enc));
    s_enc[size++] = 0;
    CHECK(_putAll(&decoder, s_enc, size) == 300);

    // Reset drops what was put, and a start in the middle of a frame costs only that frame.
    pifEncoding_CobsDecoderPut(&decoder, 0x05);
    pifEncoding_CobsDecoderPut(&decoder, 0x11);
    pifEncoding_CobsDecoderReset(&decoder);
    {
        static const uint8_t frame[] = { 0x02, 0x7E, 0x00 };
        CHECK(_putAll(&decoder, frame, sizeof(frame)) == 1 && buffer[0] == 0x7E);
        CHECK(_putAll(&decoder, frame + 1, 2) == PIF_COBS_ERROR);	// Joined mid-frame: 7E 00
        CHECK(_putAll(&decoder, frame, sizeof(frame)) == 1);
    }
}

// ---------------------------------------------------------------- zigzag, varint and float

static void _testZigzag(void)
{
    static const struct { int32_t value; uint32_t code; } kCases[] = {
        { 0, 0 }, { -1, 1 }, { 1, 2 }, { -2, 3 }, { 2, 4 },
        { 2147483647L, 0xFFFFFFFEUL }, { -2147483647L - 1, 0xFFFFFFFFUL },
    };
    int32_t value;
    unsigned i;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
        CHECK(pifEncoding_ZigzagEncode(kCases[i].value) == kCases[i].code);
        CHECK(pifEncoding_ZigzagDecode(kCases[i].code) == kCases[i].value);
    }
    for (value = -100000; value <= 100000; value += 7) {
        CHECK(pifEncoding_ZigzagDecode(pifEncoding_ZigzagEncode(value)) == value);
    }
}

static void _testUvarint(void)
{
    static const struct { uint32_t value; uint8_t size; uint8_t bytes[5]; } kCases[] = {
        { 0, 1, { 0x00 } },
        { 1, 1, { 0x01 } },
        { 127, 1, { 0x7F } },
        { 128, 2, { 0x80, 0x01 } },
        { 300, 2, { 0xAC, 0x02 } },
        { 16383, 2, { 0xFF, 0x7F } },
        { 16384, 3, { 0x80, 0x80, 0x01 } },
        { 0x0FFFFFFFUL, 4, { 0xFF, 0xFF, 0xFF, 0x7F } },
        { 0x10000000UL, 5, { 0x80, 0x80, 0x80, 0x80, 0x01 } },
        { 0xFFFFFFFFUL, 5, { 0xFF, 0xFF, 0xFF, 0xFF, 0x0F } },
    };
    uint8_t buffer[8];
    uint32_t value;
    unsigned i;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
        memset(buffer, 0xEE, sizeof(buffer));
        CHECK(pifEncoding_UvarintEncode(kCases[i].value, buffer, sizeof(buffer)) == kCases[i].size);
        CHECK(!memcmp(buffer, kCases[i].bytes, kCases[i].size));
        CHECK(buffer[kCases[i].size] == 0xEE);
        value = 0xDEADBEEFUL;
        CHECK(pifEncoding_UvarintDecode(&value, buffer, sizeof(buffer)) == kCases[i].size);
        CHECK(value == kCases[i].value);

        // One byte short on either side.
        CHECK(!pifEncoding_UvarintEncode(kCases[i].value, buffer, kCases[i].size - 1));
        value = 0xDEADBEEFUL;
        CHECK(!pifEncoding_UvarintDecode(&value, buffer, kCases[i].size - 1));
        CHECK(value == 0xDEADBEEFUL);
    }
}

static void _testUvarintBroken(void)
{
    static const uint8_t kLong[] = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x01 };		// More than 5 bytes
    static const uint8_t kHigh[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0x1F };			// More than 32 bits
    static const uint8_t kPadded[] = { 0x81, 0x80, 0x00 };					// Redundant, but 1
    uint32_t value = 0xDEADBEEFUL;

    CHECK(!pifEncoding_UvarintDecode(&value, kLong, sizeof(kLong)));
    CHECK(!pifEncoding_UvarintDecode(&value, kHigh, sizeof(kHigh)));
    CHECK(value == 0xDEADBEEFUL);
    CHECK(pifEncoding_UvarintDecode(&value, kPadded, sizeof(kPadded)) == 3 && value == 1);
    CHECK(!pifEncoding_UvarintDecode(&value, kPadded, 0));
}

static void _testFloatBits(void)
{
    static const struct { float value; uint32_t bits; } kCases[] = {
        { 0.0f, 0x00000000UL }, { -0.0f, 0x80000000UL }, { 1.0f, 0x3F800000UL }, { -2.0f, 0xC0000000UL },
        { 0.1f, 0x3DCCCCCDUL }, { 3.4028235e38f, 0x7F7FFFFFUL }, { 1.0e-45f, 0x00000001UL },
    };
    unsigned i;
    float value;

    for (i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
        CHECK(pifEncoding_FloatToBits(kCases[i].value) == kCases[i].bits);
        CHECK(pifEncoding_BitsToFloat(kCases[i].bits) == kCases[i].value);
    }
    // NaN and infinity keep their bits.
    CHECK(pifEncoding_FloatToBits(pifEncoding_BitsToFloat(0x7FC00001UL)) == 0x7FC00001UL);
    CHECK(pifEncoding_FloatToBits(pifEncoding_BitsToFloat(0xFF800000UL)) == 0xFF800000UL);
    value = pifEncoding_BitsToFloat(0x7F800000UL);
    CHECK(value > 3.4028235e38f);
}

typedef struct
{
    const char *p_name;
    void (*test)(void);
} TestCase;

int main(void)
{
    static const TestCase kTests[] = {
        { "zigzag", _testZigzag },
        { "uvarint", _testUvarint },
        { "uvarint broken", _testUvarintBroken },
        { "float bits", _testFloatBits },
        { "cobs vectors", _testCobsVectors },
        { "cobs round trip", _testCobsRoundTrip },
        { "cobs limits", _testCobsLimits },
        { "cobs decoder", _testCobsDecoder },
    };
    unsigned i, failed = 0;

    printf("test_pif_encoding\n");
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%u failed\n", failed);
    return failed ? 1 : 0;
}
