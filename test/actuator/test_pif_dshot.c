// SPDX-License-Identifier: BSD-3-Clause
#include "actuator/motor/pif_dshot.h"

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

#define MOTORS			4
#define CYCLE_US		1000	// One output cycle, so a millisecond of pause is one cycle

#define GCR_BITS		21		// The start bit and the four symbols of an answer
#define PIN_MASK		0x0004	// Bit of the motor in a port sample
#define OTHER_PINS		0x0009	// Bits of other motors, which the decoding has to ignore

static BOOL s_failed;
static uint32_t s_now;

// What act_write was given last
static uint16_t s_frame[PIF_DSHOT_MAX_MOTORS];
static uint8_t s_frame_count;
static int s_write_count;

// What evt_telemetry was given last
static PifDshotTelemetry s_telemetry;
static uint8_t s_telemetry_index;
static int s_telemetry_count;

// Nibble to 5-bit symbol, the table of the protocol the way an ESC uses it.
static const uint8_t c_nibble_symbol[16] = {
	0x19, 0x1B, 0x12, 0x13, 0x1D, 0x15, 0x16, 0x17, 0x1A, 0x09, 0x0A, 0x0B, 0x1E, 0x0D, 0x0E, 0x0F
};


static uint32_t _timer1us(void)
{
    return s_now;
}

static BOOL _portSetup(void)
{
    return TRUE;
}

static void _actWrite(PifDshot *p_owner, const uint16_t *p_frames, uint8_t count)
{
    (void)p_owner;
    memcpy(s_frame, p_frames, count * sizeof(uint16_t));
    s_frame_count = count;
    s_write_count++;
}

static void _evtTelemetry(PifDshot *p_owner, uint8_t index, const PifDshotTelemetry *p_telemetry)
{
    (void)p_owner;
    s_telemetry = *p_telemetry;
    s_telemetry_index = index;
    s_telemetry_count++;
}

static BOOL _setUp(PifDshot *p_dshot, BOOL bidirectional)
{
    memset(s_frame, 0, sizeof(s_frame));
    s_frame_count = 0;
    s_write_count = 0;
    s_telemetry_count = 0;
    s_now = 0;
    pif_error = E_SUCCESS;

    if (!pifDshot_Init(p_dshot, PIF_ID_AUTO, MOTORS, bidirectional, CYCLE_US, _actWrite)) return FALSE;
    p_dshot->evt_telemetry = _evtTelemetry;
    return TRUE;
}

// The 21 bits an ESC sends for a 12-bit value: the start bit, then the symbols of the value and of
// the inverted crc below it.
static uint32_t _encode(uint16_t value)
{
    uint16_t word = (value << 4) | ((~(value ^ (value >> 4) ^ (value >> 8))) & 0x0F);
    uint32_t gcr = 1;
    uint8_t i;

    for (i = 4; i > 0; i--) gcr = (gcr << 5) | c_nibble_symbol[(word >> ((i - 1) * 4)) & 0x0F];
    return gcr;
}

// Capture times of the edges of an answer: the line changes level on every 1 of its bits.
static uint8_t _toEdges(uint32_t gcr, uint32_t base, uint32_t bit_ticks, uint32_t *p_edges)
{
    uint8_t count = 0;
    int8_t i;

    for (i = GCR_BITS - 1; i >= 0; i--) {
        if (gcr & (1UL << i)) p_edges[count++] = base + (GCR_BITS - 1 - i) * bit_ticks;
    }
    return count;
}

// Port samples of an answer: the line idles high, then holds the level of every bit for oversample
// samples, turning over on every 1. The bits of the other motors are there to be ignored.
static uint16_t _toSamples(uint32_t gcr, uint8_t oversample, uint16_t lead, uint16_t *p_samples, uint16_t size)
{
    uint16_t count = 0, j;
    uint16_t level = PIN_MASK;
    int8_t i;

    for (j = 0; j < lead && count < size; j++) p_samples[count++] = PIN_MASK | OTHER_PINS;
    for (i = GCR_BITS - 1; i >= 0; i--) {
        if (gcr & (1UL << i)) level ^= PIN_MASK;
        for (j = 0; j < oversample && count < size; j++) {
            p_samples[count] = level | (count & 1 ? OTHER_PINS : 0);
            count++;
        }
    }
    // The line stays where its last bit left it.
    while (count < size) p_samples[count++] = level;
    return count;
}

static void testFrame(void)
{
    // Throttle 48 is 000 0011 0000, so the frame is 0000 0110 000 0 and the three nibbles above the
    // crc XORed together.
    CHECK(pifDshot_MakeFrame(48, FALSE, FALSE) == 0x0606);
    // The telemetry bit sits below the value and goes into the crc with it.
    CHECK(pifDshot_MakeFrame(48, TRUE, FALSE) == 0x0617);
    // Bidirectional DShot inverts the crc and nothing else.
    CHECK(pifDshot_MakeFrame(48, FALSE, TRUE) == 0x0609);
    CHECK(pifDshot_MakeFrame(48, TRUE, TRUE) == 0x0618);

    CHECK(pifDshot_MakeFrame(0, FALSE, FALSE) == 0x0000);
    CHECK(pifDshot_MakeFrame(0, FALSE, TRUE) == 0x000F);
    CHECK(pifDshot_MakeFrame(DSC_BEACON1, FALSE, FALSE) == 0x0022);
    CHECK(pifDshot_MakeFrame(DSC_BEACON1, TRUE, FALSE) == 0x0033);
    CHECK(pifDshot_MakeFrame(PIF_DSHOT_MAX_THROTTLE, FALSE, FALSE) == 0xFFEE);

    // Only the low 11 bits are the value.
    CHECK(pifDshot_MakeFrame(2048, FALSE, FALSE) == 0x0000);
    CHECK(pifDshot_MakeFrame(2048 + 48, FALSE, FALSE) == 0x0606);
}

static void testBuffer(void)
{
    uint32_t buffer[PIF_DSHOT_BUFFER_SIZE * 3];
    uint8_t i;

    // A bit period of 80 ticks, as DShot300 off a 24 MHz timer.
    CHECK(PIF_DSHOT_BIT0_TICKS(80) == 30 && PIF_DSHOT_BIT1_TICKS(80) == 60);

    memset(buffer, 0xFF, sizeof(buffer));
    CHECK(pifDshot_LoadBuffer(buffer, 1, 0x8001, 30, 60) == PIF_DSHOT_BUFFER_SIZE);
    CHECK(buffer[0] == 60 && buffer[15] == 60);
    for (i = 1; i < 15; i++) CHECK(buffer[i] == 30);
    CHECK(buffer[16] == 0 && buffer[17] == 0);

    // One buffer of three channels, this motor on the middle one.
    memset(buffer, 0xFF, sizeof(buffer));
    CHECK(pifDshot_LoadBuffer(buffer + 1, 3, 0x0001, 30, 60) == PIF_DSHOT_BUFFER_SIZE);
    CHECK(buffer[1] == 30 && buffer[1 + 15 * 3] == 60 && buffer[1 + 17 * 3] == 0);
    CHECK(buffer[0] == 0xFFFFFFFF && buffer[2] == 0xFFFFFFFF);

    // ProShot1000 sends a nibble as the width of a pulse.
    memset(buffer, 0xFF, sizeof(buffer));
    CHECK(pifDshot_LoadBufferProshot(buffer, 1, 0x1234, PIF_PROSHOT_BASE_SYMBOL_24MHZ,
            PIF_PROSHOT_BIT_WIDTH_24MHZ) == PIF_PROSHOT_BUFFER_SIZE);
    CHECK(buffer[0] == 24 + 3 && buffer[1] == 24 + 6 && buffer[2] == 24 + 9 && buffer[3] == 24 + 12);
    CHECK(buffer[4] == 0 && buffer[5] == 0);
}

static void testBitbangBuffer(void)
{
    uint32_t buffer[PIF_DSHOT_BITBANG_BUFFER_SIZE];
    uint8_t i;

    // Two motors on pin 0 and pin 1 of the same port.
    memset(buffer, 0, sizeof(buffer));
    pifDshot_InitBitbangBuffer(buffer, 0x0001, 0x0001UL << 16, FALSE);
    pifDshot_InitBitbangBuffer(buffer, 0x0002, 0x0002UL << 16, FALSE);

    for (i = 0; i < 16; i++) {
        CHECK(buffer[i * 3] == 0x0003);				// Every bit opens with both pins high
        CHECK(buffer[i * 3 + 1] == 0);				// and has nothing in the middle yet
        CHECK(buffer[i * 3 + 2] == 0x0003UL << 16);	// and both of them low again
    }
    // The hold bit leaves the pins at rest, so it writes nothing.
    CHECK(buffer[48] == 0 && buffer[49] == 0 && buffer[50] == 0);

    // Pin 0 gets a 1 and 15 zeros, pin 1 15 zeros and a 1.
    pifDshot_LoadBitbangBuffer(buffer, 0x0001, 0x0001UL << 16, 0x8000, FALSE);
    pifDshot_LoadBitbangBuffer(buffer, 0x0002, 0x0002UL << 16, 0x0001, FALSE);
    CHECK(buffer[1] == 0x0002UL << 16);		// Bit 15: pin 1 ends early, pin 0 does not
    CHECK(buffer[4] == 0x0003UL << 16);		// Bit 14: both of them do
    CHECK(buffer[46] == 0x0001UL << 16);	// Bit 0: pin 0 ends early now
    CHECK(buffer[0] == 0x0003 && buffer[2] == 0x0003UL << 16);

    pifDshot_ClearBitbangBuffer(buffer);
    for (i = 0; i < 16; i++) CHECK(buffer[i * 3 + 1] == 0);
    CHECK(buffer[0] == 0x0003 && buffer[2] == 0x0003UL << 16);

    // Bidirectional DShot rests high and pulses low, which swaps the two masks.
    memset(buffer, 0, sizeof(buffer));
    pifDshot_InitBitbangBuffer(buffer, 0x0001, 0x0001UL << 16, TRUE);
    CHECK(buffer[0] == 0x0001UL << 16 && buffer[2] == 0x0001);
    pifDshot_LoadBitbangBuffer(buffer, 0x0001, 0x0001UL << 16, 0x7FFF, TRUE);
    CHECK(buffer[1] == 0x0001 && buffer[4] == 0);
}

static void testGcr(void)
{
    static const uint16_t c_value[] = { 0x000, 0x001, 0x100, 0x1AB, 0x555, 0x7FF, 0x800, 0xABC, 0xFFF };
    uint32_t edges[GCR_BITS];
    uint16_t samples[128];
    uint32_t gcr;
    uint16_t value, count;
    uint8_t edge_count, t, i;

    for (t = 0; t < sizeof(c_value) / sizeof(c_value[0]); t++) {
        gcr = _encode(c_value[t]);
        CHECK(pifDshot_DecodeGcr(gcr, &value) && value == c_value[t]);

        // A telemetry bit of 40 capture ticks.
        edge_count = _toEdges(gcr, 12345, 40, edges);
        CHECK(pifDshot_EdgesToGcr(edges, edge_count, 40) == gcr);

        // The clock of the ESC is not ours, so every edge comes a little early or late.
        for (i = 0; i < edge_count; i++) edges[i] += (i & 1) ? 9 : 0;
        CHECK(pifDshot_EdgesToGcr(edges, edge_count, 40) == gcr);

        count = _toSamples(gcr, 3, 5, samples, 100);
        CHECK(pifDshot_SamplesToGcr(samples, count, PIN_MASK, 3) == gcr);

        count = _toSamples(gcr, 5, 17, samples, 128);
        CHECK(pifDshot_SamplesToGcr(samples, count, PIN_MASK, 5) == gcr);
    }

    // p_value may be left out to only check an answer.
    CHECK(pifDshot_DecodeGcr(_encode(0x1AB), NULL));

    // A symbol that no nibble maps to, and a symbol that does but breaks the crc.
    gcr = _encode(0x100);
    CHECK(!pifDshot_DecodeGcr(gcr & ~0x1FUL, NULL));
    CHECK(!pifDshot_DecodeGcr((gcr & ~0x1FUL) | c_nibble_symbol[0x0D], NULL));
    CHECK(!pifDshot_DecodeGcr(PIF_DSHOT_GCR_NONE, NULL));
    CHECK(!pifDshot_DecodeGcr(PIF_DSHOT_GCR_INVALID, NULL));
}

static void testGcrFailure(void)
{
    uint32_t edges[GCR_BITS];
    uint16_t samples[128];
    uint32_t gcr = _encode(0x100);
    uint8_t edge_count = _toEdges(gcr, 1000, 40, edges);
    uint16_t i;

    CHECK(pifDshot_EdgesToGcr(NULL, edge_count, 40) == PIF_DSHOT_GCR_NONE);
    CHECK(pifDshot_EdgesToGcr(edges, 0, 40) == PIF_DSHOT_GCR_NONE);
    CHECK(pifDshot_EdgesToGcr(edges, edge_count, 0) == PIF_DSHOT_GCR_NONE);

    // A run longer than a symbol allows, and edges that stop in the middle of the answer.
    edges[1] = edges[0] + 5 * 40;
    CHECK(pifDshot_EdgesToGcr(edges, edge_count, 40) == PIF_DSHOT_GCR_INVALID);
    CHECK(pifDshot_EdgesToGcr(edges, 3, 40) == PIF_DSHOT_GCR_INVALID);

    CHECK(pifDshot_SamplesToGcr(NULL, 100, PIN_MASK, 3) == PIF_DSHOT_GCR_NONE);
    CHECK(pifDshot_SamplesToGcr(samples, 100, PIN_MASK, 0) == PIF_DSHOT_GCR_NONE);

    // Too few samples to hold an answer at all.
    _toSamples(gcr, 3, 5, samples, 128);
    CHECK(pifDshot_SamplesToGcr(samples, 50, PIN_MASK, 3) == PIF_DSHOT_GCR_NONE);

    // An ESC that had nothing to say leaves the line high.
    for (i = 0; i < 128; i++) samples[i] = PIN_MASK | OTHER_PINS;
    CHECK(pifDshot_SamplesToGcr(samples, 128, PIN_MASK, 3) == PIF_DSHOT_GCR_NONE);

    // One that broke off after two bits.
    for (i = 5; i < 11; i++) samples[i] = OTHER_PINS;
    CHECK(pifDshot_SamplesToGcr(samples, 128, PIN_MASK, 3) == PIF_DSHOT_GCR_NONE);
}

static void testTelemetryValue(void)
{
    PifDshotTelemetry telemetry;

    // An eRPM is a period in microseconds, the mantissa shifted up by the exponent, and a minute
    // holds 60,000,000 of them.
    CHECK(pifDshot_DecodeTelemetry(0x100, FALSE, &telemetry));
    CHECK(telemetry.type == DTT_ERPM && telemetry.value == 234375);
    CHECK(pifDshot_DecodeTelemetry(0x700, FALSE, &telemetry) && telemetry.value == 29297);

    // The longest period there is means the motor stands still.
    CHECK(pifDshot_DecodeTelemetry(0x0FFF, FALSE, &telemetry));
    CHECK(telemetry.type == DTT_ERPM && telemetry.value == 0);

    // No ESC sends a period of 0.
    CHECK(!pifDshot_DecodeTelemetry(0x000, FALSE, &telemetry));
    CHECK(!pifDshot_DecodeTelemetry(0x200, FALSE, &telemetry));

    // The shapes EDT uses are an eRPM to an ESC that does not send EDT.
    CHECK(pifDshot_DecodeTelemetry(0x225, FALSE, &telemetry));
    CHECK(telemetry.type == DTT_ERPM && telemetry.value == 810811);
    CHECK(pifDshot_DecodeTelemetry(0x225, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_TEMPERATURE && telemetry.value == 37);

    CHECK(pifDshot_DecodeTelemetry(0x4C8, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_VOLTAGE && telemetry.value == 50000);
    CHECK(pifDshot_DecodeTelemetry(0x60A, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_CURRENT && telemetry.value == 10000);
    CHECK(pifDshot_DecodeTelemetry(0x8FF, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_DEBUG1 && telemetry.value == 255);
    CHECK(pifDshot_DecodeTelemetry(0xA01, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_DEBUG2 && telemetry.value == 1);
    CHECK(pifDshot_DecodeTelemetry(0xC80, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_STRESS && telemetry.value == 128);
    CHECK(pifDshot_DecodeTelemetry(0xE03, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_STATUS && telemetry.value == 3);

    // With EDT on, a value of a shape an eRPM takes is still an eRPM.
    CHECK(pifDshot_DecodeTelemetry(0x300, TRUE, &telemetry));
    CHECK(telemetry.type == DTT_ERPM && telemetry.value == 117188);

    pif_error = E_SUCCESS;
    CHECK(!pifDshot_DecodeTelemetry(0x100, FALSE, NULL) && pif_error == E_INVALID_PARAM);
}

static void testInit(void)
{
    PifDshot dshot;
    uint8_t i;

    pif_error = E_SUCCESS;
    CHECK(!pifDshot_Init(NULL, PIF_ID_AUTO, MOTORS, FALSE, CYCLE_US, _actWrite));
    CHECK(!pifDshot_Init(&dshot, PIF_ID_AUTO, 0, FALSE, CYCLE_US, _actWrite));
    CHECK(!pifDshot_Init(&dshot, PIF_ID_AUTO, PIF_DSHOT_MAX_MOTORS + 1, FALSE, CYCLE_US, _actWrite));
    CHECK(!pifDshot_Init(&dshot, PIF_ID_AUTO, MOTORS, FALSE, 0, _actWrite));
    CHECK(!pifDshot_Init(&dshot, PIF_ID_AUTO, MOTORS, FALSE, CYCLE_US, NULL));
    CHECK(pif_error == E_INVALID_PARAM);

    CHECK(_setUp(&dshot, TRUE));
    CHECK(dshot._id != PIF_ID_AUTO && dshot._motor_count == MOTORS);
    CHECK(dshot._bidirectional && dshot._cycle_us == CYCLE_US);
    CHECK(!dshot.extended_telemetry && !pifDshot_IsCommandBusy(&dshot));
    for (i = 0; i < MOTORS; i++) {
        CHECK(dshot._motor[i].value == 0 && !dshot._motor[i].request_telemetry);
        CHECK(dshot._motor[i]._erpm == 0 && dshot._motor[i]._telemetry_count == 0);
    }

    pifDshot_SetCyclePeriod(&dshot, 0);
    CHECK(dshot._cycle_us == CYCLE_US);
    pifDshot_SetCyclePeriod(&dshot, 2000);
    CHECK(dshot._cycle_us == 2000);
    pifDshot_Clear(&dshot);
}

static void testThrottle(void)
{
    PifDshot dshot;

    CHECK(_setUp(&dshot, FALSE));

    pifDshot_SetThrottle(&dshot, 1, 1000);
    CHECK(dshot._motor[1].value == 1000 && dshot._motor[0].value == 0);

    // 1 to 47 would read as a command, and above the range the throttle is held at the top.
    pifDshot_SetThrottle(&dshot, PIF_DSHOT_ALL_MOTORS, 20);
    CHECK(dshot._motor[0].value == PIF_DSHOT_MIN_THROTTLE && dshot._motor[3].value == PIF_DSHOT_MIN_THROTTLE);
    pifDshot_SetThrottle(&dshot, PIF_DSHOT_ALL_MOTORS, 5000);
    CHECK(dshot._motor[0].value == PIF_DSHOT_MAX_THROTTLE);

    // A motor the instance does not have is no motor of it.
    pifDshot_SetThrottle(&dshot, MOTORS, 500);
    CHECK(dshot._motor[0].value == PIF_DSHOT_MAX_THROTTLE);

    pifDshot_SetThrottle(&dshot, PIF_DSHOT_ALL_MOTORS, 0);
    pifDshot_SetThrottle(&dshot, 2, 1000);
    CHECK(pifDshot_Update(&dshot));
    CHECK(s_write_count == 1 && s_frame_count == MOTORS);
    CHECK(s_frame[2] == pifDshot_MakeFrame(1000, FALSE, FALSE));
    CHECK(s_frame[0] == pifDshot_MakeFrame(0, FALSE, FALSE));

    // The telemetry bit is asked for one frame at a time.
    dshot._motor[1].request_telemetry = TRUE;
    CHECK(pifDshot_Update(&dshot));
    CHECK(s_frame[1] == pifDshot_MakeFrame(0, TRUE, FALSE) && !dshot._motor[1].request_telemetry);
    CHECK(pifDshot_Update(&dshot));
    CHECK(s_frame[1] == pifDshot_MakeFrame(0, FALSE, FALSE));
    pifDshot_Clear(&dshot);
}

static void testCommandOnce(void)
{
    PifDshot dshot;
    int i;

    CHECK(_setUp(&dshot, FALSE));
    CHECK(pifDshot_Command(&dshot, PIF_DSHOT_ALL_MOTORS, DSC_BEACON1));
    CHECK(pifDshot_IsCommandBusy(&dshot));

    // 10 ms of held output before the frame.
    for (i = 0; i < 10; i++) CHECK(!pifDshot_Update(&dshot));
    CHECK(s_write_count == 0);

    // The command itself, which an ESC only takes with the telemetry bit set.
    CHECK(pifDshot_Update(&dshot));
    CHECK(s_write_count == 1 && s_frame[0] == pifDshot_MakeFrame(DSC_BEACON1, TRUE, FALSE));
    CHECK(s_frame[3] == s_frame[0]);

    // 100 ms of held output while the ESC beeps.
    for (i = 0; i < 100; i++) CHECK(!pifDshot_Update(&dshot));
    CHECK(s_write_count == 1 && pifDshot_IsCommandBusy(&dshot));

    CHECK(pifDshot_Update(&dshot));
    CHECK(s_write_count == 2 && s_frame[0] == pifDshot_MakeFrame(0, FALSE, FALSE));
    CHECK(!pifDshot_IsCommandBusy(&dshot));
    pifDshot_Clear(&dshot);
}

static void testCommandRepeated(void)
{
    PifDshot dshot;
    int i;

    CHECK(_setUp(&dshot, FALSE));

    // A command the ESC stores goes out ten times, a millisecond apart.
    CHECK(pifDshot_Command(&dshot, 2, DSC_SPIN_DIRECTION_NORMAL));
    for (i = 0; i < 10; i++) CHECK(!pifDshot_Update(&dshot));

    for (i = 0; i < 10; i++) {
        CHECK(pifDshot_Update(&dshot));
        CHECK(s_frame[2] == pifDshot_MakeFrame(DSC_SPIN_DIRECTION_NORMAL, TRUE, FALSE));
        // Motors the command is not meant for are told to stand still.
        CHECK(s_frame[0] == pifDshot_MakeFrame(DSC_MOTOR_STOP, FALSE, FALSE));
        if (i < 9) CHECK(!pifDshot_Update(&dshot));
    }
    CHECK(s_write_count == 10);

    // A millisecond after the last frame, then the throttle again.
    CHECK(!pifDshot_Update(&dshot));
    CHECK(pifDshot_Update(&dshot) && s_write_count == 11);
    CHECK(!pifDshot_IsCommandBusy(&dshot));
    pifDshot_Clear(&dshot);
}

static void testCommandWaitsForStop(void)
{
    PifDshot dshot;
    int i;

    CHECK(_setUp(&dshot, FALSE));
    pifDshot_SetThrottle(&dshot, PIF_DSHOT_ALL_MOTORS, 500);
    CHECK(pifDshot_Command(&dshot, PIF_DSHOT_ALL_MOTORS, DSC_BEACON2));

    // The motors keep their throttle until they have all come to a stop.
    for (i = 0; i < 5; i++) {
        CHECK(pifDshot_Update(&dshot));
        CHECK(s_frame[0] == pifDshot_MakeFrame(500, FALSE, FALSE));
    }
    CHECK(pifDshot_IsCommandBusy(&dshot));

    pifDshot_SetThrottle(&dshot, PIF_DSHOT_ALL_MOTORS, 0);
    for (i = 0; i < 10; i++) CHECK(!pifDshot_Update(&dshot));
    CHECK(pifDshot_Update(&dshot));
    CHECK(s_frame[0] == pifDshot_MakeFrame(DSC_BEACON2, TRUE, FALSE));
    pifDshot_Clear(&dshot);
}

static void testCommandQueue(void)
{
    PifDshot dshot;
    int i, t;

    CHECK(_setUp(&dshot, FALSE));

    pif_error = E_SUCCESS;
    CHECK(!pifDshot_Command(&dshot, 0, PIF_DSHOT_MAX_COMMAND + 1));
    CHECK(!pifDshot_Command(&dshot, MOTORS, DSC_LED0_ON));
    CHECK(pif_error == E_INVALID_PARAM);

    for (i = 0; i < PIF_DSHOT_COMMAND_QUEUE_SIZE; i++) CHECK(pifDshot_Command(&dshot, 0, DSC_LED0_ON + i));
    pif_error = E_SUCCESS;
    CHECK(!pifDshot_Command(&dshot, 0, DSC_LED0_ON));
    CHECK(pif_error == E_OVERFLOW_BUFFER);

    // Every command of the queue keeps the 10 ms before its frame and the millisecond after it.
    for (t = 0; t < PIF_DSHOT_COMMAND_QUEUE_SIZE; t++) {
        for (i = 0; i < 10; i++) CHECK(!pifDshot_Update(&dshot));
        CHECK(pifDshot_Update(&dshot));
        CHECK(s_write_count == t + 1 && s_frame[0] == pifDshot_MakeFrame(DSC_LED0_ON + t, TRUE, FALSE));
        CHECK(!pifDshot_Update(&dshot));
    }
    CHECK(pifDshot_IsCommandBusy(&dshot));
    CHECK(pifDshot_Update(&dshot));
    CHECK(!pifDshot_IsCommandBusy(&dshot) && s_frame[0] == pifDshot_MakeFrame(0, FALSE, FALSE));
    pifDshot_Clear(&dshot);
}

static void testCyclePeriod(void)
{
    PifDshot dshot;
    int i;

    CHECK(_setUp(&dshot, FALSE));

    // A pause is rounded up to whole cycles: 10 ms of 3 ms cycles is 4 of them.
    pifDshot_SetCyclePeriod(&dshot, 3000);
    CHECK(pifDshot_Command(&dshot, 0, DSC_LED0_ON));
    for (i = 0; i < 4; i++) CHECK(!pifDshot_Update(&dshot));
    CHECK(pifDshot_Update(&dshot) && s_write_count == 1);
    pifDshot_Clear(&dshot);
}

static void testPutGcr(void)
{
    PifDshot dshot;

    CHECK(_setUp(&dshot, TRUE));

    CHECK(pifDshot_PutGcr(&dshot, 1, _encode(0x100)));
    CHECK(s_telemetry_count == 1 && s_telemetry_index == 1);
    CHECK(s_telemetry.type == DTT_ERPM && s_telemetry.value == 234375);
    CHECK(dshot._motor[1]._erpm == 234375 && dshot._motor[1]._telemetry_count == 1);
    CHECK(dshot._motor[0]._telemetry_count == 0);

    // An ESC that said nothing is not an ESC that said something wrong.
    CHECK(!pifDshot_PutGcr(&dshot, 1, PIF_DSHOT_GCR_NONE));
    CHECK(dshot._motor[1]._missing_count == 1 && dshot._motor[1]._error_count == 0);
    CHECK(!pifDshot_PutGcr(&dshot, 1, PIF_DSHOT_GCR_INVALID));
    CHECK(!pifDshot_PutGcr(&dshot, 1, _encode(0x100) ^ 1));
    CHECK(dshot._motor[1]._error_count == 2 && dshot._motor[1]._missing_count == 1);
    CHECK(s_telemetry_count == 1);

    pif_error = E_SUCCESS;
    CHECK(!pifDshot_PutGcr(&dshot, MOTORS, _encode(0x100)) && pif_error == E_INVALID_PARAM);

    // A temperature is only a temperature once the client says the ESC sends EDT.
    dshot.extended_telemetry = TRUE;
    CHECK(pifDshot_PutGcr(&dshot, 0, _encode(0x225)));
    CHECK(s_telemetry.type == DTT_TEMPERATURE && s_telemetry.value == 37);
    CHECK(dshot._motor[0]._erpm == 0 && dshot._motor[0]._telemetry_count == 1);
    pifDshot_Clear(&dshot);
}

#ifdef COLLECT_SIGNAL_DSHOT

static void testCollectSignal(void)
{
    PifDshot dshot;

    CHECK(_setUp(&dshot, FALSE));
    CHECK(pifDshot_SetCsFlag(&dshot, PIF_CS_INDEX_ALL, DS_CSF_ALL_BIT));

    CHECK(strcmp(dshot.__cs[DS_CSF_STATE_IDX]._name, "DSS") == 0);
    CHECK(dshot.__cs[DS_CSF_STATE_IDX]._id == dshot._id && dshot.__cs[DS_CSF_STATE_IDX]._width == 3);
    CHECK(dshot.__cs[DS_CSF_STATE_IDX]._value == PIF_DSHOT_CS_NO_COMMAND);
    CHECK(strcmp(dshot.__cs[DS_CSF_COMMAND_IDX]._name, "DSC") == 0);
    CHECK(dshot.__cs[DS_CSF_COMMAND_IDX]._width == 6 && dshot.__cs[DS_CSF_COMMAND_IDX]._value == DSC_MOTOR_STOP);

    // The step and the command of the cycle that is going out.
    CHECK(pifDshot_Command(&dshot, 0, DSC_BEACON3));
    CHECK(!pifDshot_Update(&dshot));
    CHECK(dshot.__cs[DS_CSF_STATE_IDX]._value == DCS_START_DELAY);
    CHECK(dshot.__cs[DS_CSF_COMMAND_IDX]._value == DSC_BEACON3);

    while (pifDshot_IsCommandBusy(&dshot)) pifDshot_Update(&dshot);
    CHECK(dshot.__cs[DS_CSF_STATE_IDX]._value == PIF_DSHOT_CS_NO_COMMAND);
    CHECK(dshot.__cs[DS_CSF_COMMAND_IDX]._value == DSC_MOTOR_STOP);

    pifDshot_ResetCsFlag(&dshot, PIF_CS_INDEX_ALL, DS_CSF_ALL_BIT);
    CHECK(pifDshot_SetCsFlag(&dshot, PIF_CS_INDEX_ALL, DS_CSF_STATE_BIT));
    pifDshot_Clear(&dshot);
}

#endif

int main(void)
{
    int failed = 0;
    size_t t;
    static const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        { "frame", testFrame },
        { "buffer", testBuffer },
        { "bit-bang buffer", testBitbangBuffer },
        { "gcr", testGcr },
        { "gcr failure", testGcrFailure },
        { "telemetry value", testTelemetryValue },
        { "init", testInit },
        { "throttle", testThrottle },
        { "command once", testCommandOnce },
        { "command repeated", testCommandRepeated },
        { "command waits for stop", testCommandWaitsForStop },
        { "command queue", testCommandQueue },
        { "cycle period", testCyclePeriod },
        { "put gcr", testPutGcr },
#ifdef COLLECT_SIGNAL_DSHOT
        { "collect signal", testCollectSignal },
#endif
    };

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_dshot: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_dshot\n");
    for (t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        s_failed = FALSE;
        tests[t].fn();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", tests[t].name);
        if (s_failed) failed++;
    }
    return failed ? 1 : 0;
}
