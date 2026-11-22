// SPDX-License-Identifier: BSD-3-Clause
// pif_max7456 against a MAX7456 in memory: what it puts in VM0 and DMM, the characters it writes one at a time
// and in an auto-increment run, the brightness of the rows, the patterns it stores in the character NVM, and the
// waits and the failures around all of it.
#include "max7456_emu.h"


// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

#define CHECK_EQ(GOT, EXP) \
    do { \
        if ((long)(GOT) != (long)(EXP)) { \
            printf("  FAIL %s:%d: %s is %ld, expected %ld\n", __FILE__, __LINE__, #GOT, \
                   (long)(GOT), (long)(EXP)); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

// What pifMax7456_Start() leaves in VM0: the standard, the sync of the signal, the characters put up at the
// vertical sync, the picture on and the video through.
#define VM0_PAL		(MAX7456_VIDEO_STANDARD_SELECT_PAL | MAX7456_VERT_SYNC_OSD_VSYNC | \
                     MAX7456_ENABLE_DISP_OSD_IMAGE(1))
#define VM0_NTSC	(MAX7456_VIDEO_STANDARD_SELECT_NTSC | MAX7456_VERT_SYNC_OSD_VSYNC | \
                     MAX7456_ENABLE_DISP_OSD_IMAGE(1))

static BOOL s_failed;
static PifMax7456 s_osd;


static uint32_t _timer1us(void)
{
    return 0;
}

// A failed test returns early, so the device of the one before it is given back here.
static BOOL _setUp(void)
{
    pifMax7456_Clear(&s_osd);
    if (!_emuInit()) return FALSE;
    if (!pifMax7456_Init(&s_osd, PIF_ID_AUTO, &s_emu_port, NULL)) return FALSE;
    return pifMax7456_Detect(&s_osd);
}

static void _fill(uint8_t* p_data, size_t size, uint8_t seed)
{
    size_t i;

    for (i = 0; i < size; i++) p_data[i] = (uint8_t)(seed + i * 5);
}


static void testInit(void)
{
    PifMax7456 second, third;

    CHECK(!pifMax7456_Init(NULL, PIF_ID_AUTO, &s_emu_port, NULL) && pif_error == E_INVALID_PARAM);
    CHECK(!pifMax7456_Init(&s_osd, PIF_ID_AUTO, NULL, NULL) && pif_error == E_INVALID_PARAM);

    CHECK(_setUp());
    CHECK(s_osd._p_device != NULL && s_osd._id != PIF_ID_AUTO);

    // The port of the emulator takes two devices, so the third one finds no room.
    CHECK(pifMax7456_Init(&second, PIF_ID_AUTO, &s_emu_port, NULL));
    CHECK(!pifMax7456_Init(&third, PIF_ID_AUTO, &s_emu_port, NULL));
    CHECK(third._p_device == NULL);
    CHECK(pifMax7456_Clear(&second));

    // The device goes back once, and asking again changes nothing.
    CHECK(pifMax7456_Clear(&s_osd));
    CHECK(s_osd._p_device == NULL);
    CHECK(pifMax7456_Clear(&s_osd));
}

static void testDetect(void)
{
    CHECK(_setUp());
    CHECK(s_osd.detected);

    // OSDM holds something else: not a MAX7456, or one that was written by something else.
    s_emu_osdm = 0x00;
    CHECK(!pifMax7456_Detect(&s_osd) && !s_osd.detected);

    // Nothing on the bus reads as ones.
    s_emu_osdm = EMU_OSDM_POWER_ON;
    s_emu_absent = TRUE;
    CHECK(!pifMax7456_Detect(&s_osd) && !s_osd.detected);

    s_emu_absent = FALSE;
    CHECK(pifMax7456_Detect(&s_osd));

    // A read that fails leaves the chip undetected, with the error of the access.
    s_emu_fail_in = 0;
    CHECK(!pifMax7456_Detect(&s_osd) && !s_osd.detected);
    CHECK(pif_error != E_SUCCESS);
}

static void testStartEnable(void)
{
    CHECK(_setUp());

    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));
    CHECK_EQ(s_emu_vm0, VM0_PAL);

    // Only the picture goes off and on again; the standard and the sync stay as they were.
    CHECK(pifMax7456_Enable(&s_osd, FALSE));
    CHECK_EQ(s_emu_vm0, VM0_PAL & ~MAX7456_ENABLE_DISP_OSD_IMAGE_MASK);
    CHECK(pifMax7456_Enable(&s_osd, TRUE));
    CHECK_EQ(s_emu_vm0, VM0_PAL);

    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_NTSC));
    CHECK_EQ(s_emu_vm0, VM0_NTSC);

    s_emu_fail_in = 0;
    CHECK(!pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));
    CHECK(!pifMax7456_Enable(&s_osd, FALSE));
}

static void testVideoSignal(void)
{
    CHECK(_setUp());

    CHECK_EQ(pifMax7456_VideoSignal(&s_osd), MAX7456_VIDEO_NONE);

    s_emu_stat = MAX7456_PAL_SIGNAL_DETECT(1);
    CHECK_EQ(pifMax7456_VideoSignal(&s_osd), MAX7456_VIDEO_PAL);

    s_emu_stat = MAX7456_NTSC_SIGNAL_DETECT(1);
    CHECK_EQ(pifMax7456_VideoSignal(&s_osd), MAX7456_VIDEO_NTSC);

    // A read that fails is no signal.
    s_emu_stat = MAX7456_PAL_SIGNAL_DETECT(1);
    s_emu_fail_in = 0;
    CHECK_EQ(pifMax7456_VideoSignal(&s_osd), MAX7456_VIDEO_NONE);
}

static void testReset(void)
{
    CHECK(_setUp());
    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));
    CHECK(pifMax7456_WriteChar(&s_osd, 5, 'A'));
    CHECK(pifMax7456_Brightness(&s_osd, 0, 2));

    CHECK(pifMax7456_Reset(&s_osd));
    // The chip is at its power-on values, the display memory with them, and the wait saw the bit clear.
    CHECK_EQ(s_emu_vm0, 0);
    CHECK_EQ(s_emu_display[5], 0);
    CHECK_EQ(s_emu_reset_busy, 0);

    // The driver holds nothing of the chip any more: the same levels are written again, and the picture goes
    // back on from scratch.
    s_emu_writes = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 0, 2));
    CHECK_EQ(s_emu_writes, 16);
    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));
    CHECK_EQ(s_emu_vm0, VM0_PAL);

    // A bit that never clears is a chip that does not come back.
    CHECK(_setUp());
    s_emu_stuck = TRUE;
    CHECK(!pifMax7456_Reset(&s_osd) && pif_error == E_TIMEOUT);

    // The write goes through and the read of the wait fails.
    CHECK(_setUp());
    s_emu_fail_in = 1;
    CHECK(!pifMax7456_Reset(&s_osd) && pif_error != E_TIMEOUT);
}

static void testClearDisplay(void)
{
    uint8_t chars[4] = { 1, 2, 3, 4 };

    CHECK(_setUp());
    CHECK(pifMax7456_WriteChars(&s_osd, 0, chars, sizeof(chars)));
    CHECK_EQ(s_emu_display[2], 3);

    CHECK(pifMax7456_ClearDisplay(&s_osd));
    CHECK_EQ(s_emu_display[0], 0);
    CHECK_EQ(s_emu_display[2], 0);
    CHECK_EQ(s_emu_clear_busy, 0);

    // The inversion asked for is kept by the clear, which only adds its own bit to DMM.
    CHECK(pifMax7456_Invert(&s_osd, TRUE));
    CHECK(pifMax7456_ClearDisplay(&s_osd));
    CHECK_EQ(s_emu_dmm & ~MAX7456_CLEAR_DISP_MEM_MASK,
             MAX7456_OPER_MODE_SELECT_8BIT | MAX7456_INVERT_BIT_INVERT);

    CHECK(_setUp());
    s_emu_stuck = TRUE;
    CHECK(!pifMax7456_ClearDisplay(&s_osd) && pif_error == E_TIMEOUT);
}

static void testWriteChar(void)
{
    CHECK(_setUp());

    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChar(&s_osd, 0, 'A'));
    CHECK_EQ(s_emu_display[0], 'A');
    CHECK_EQ(s_emu_transfers, 1);
    // 8 bit operation, and the character of the place, not its attribute.
    CHECK_EQ(s_emu_dmm, MAX7456_OPER_MODE_SELECT_8BIT);
    CHECK(!s_emu_attr_byte);

    // The ninth bit of the address reaches DMAH.
    CHECK(pifMax7456_WriteChar(&s_osd, 300, 7));
    CHECK_EQ(s_emu_display[300], 7);
    CHECK(pifMax7456_WriteChar(&s_osd, EMU_DISPLAY_SIZE - 1, 9));
    CHECK_EQ(s_emu_display[EMU_DISPLAY_SIZE - 1], 9);
    CHECK_EQ(s_emu_display[44], 0);
}

static void testWriteChars(void)
{
    uint8_t chars[MAX7456_COLUMNS + 1];
    uint16_t i;

    CHECK(_setUp());
    _fill(chars, sizeof(chars), 1);

    CHECK(!pifMax7456_WriteChars(&s_osd, 0, NULL, 4) && pif_error == E_INVALID_PARAM);
    CHECK(!pifMax7456_WriteChars(&s_osd, 0, chars, 0) && pif_error == E_INVALID_PARAM);

    // A whole row in one transfer, the chip stepping the address on by itself.
    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChars(&s_osd, MAX7456_COLUMNS, chars, MAX7456_COLUMNS));
    CHECK_EQ(s_emu_transfers, 1);
    CHECK(memcmp(s_emu_display + MAX7456_COLUMNS, chars, MAX7456_COLUMNS) == 0);
    CHECK_EQ(s_emu_display[MAX7456_COLUMNS - 1], 0);
    CHECK_EQ(s_emu_display[2 * MAX7456_COLUMNS], 0);
    // The escape left auto-increment mode again.
    CHECK(!s_emu_auto);
    CHECK_EQ(s_emu_dmm & MAX7456_AUTO_INCR_MODE_MASK, 0);

    // A run that crosses the 256th place: the chip carries into the ninth bit of the address.
    CHECK(pifMax7456_WriteChars(&s_osd, 250, chars, 10));
    CHECK(memcmp(s_emu_display + 250, chars, 10) == 0);

    // More than a row goes character by character, each its own transfer.
    CHECK(_setUp());
    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChars(&s_osd, 0, chars, MAX7456_COLUMNS + 1));
    CHECK_EQ(s_emu_transfers, MAX7456_COLUMNS + 1);
    CHECK(memcmp(s_emu_display, chars, MAX7456_COLUMNS + 1) == 0);

    // So does a run that holds the escape, which would end the run instead of reaching the picture.
    chars[3] = MAX7456_ESCAPE;
    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChars(&s_osd, 60, chars, 6));
    CHECK_EQ(s_emu_transfers, 6);
    CHECK(memcmp(s_emu_display + 60, chars, 6) == 0);
    CHECK_EQ(s_emu_display[63], MAX7456_ESCAPE);

    // And so does a run too long for the port, while one that just fits goes whole.
    _fill(chars, sizeof(chars), 1);
    s_osd._p_device->max_transfer_size = 2 * (8 + 4);
    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChars(&s_osd, 120, chars, 8));
    CHECK_EQ(s_emu_transfers, 1);
    s_emu_transfers = 0;
    CHECK(pifMax7456_WriteChars(&s_osd, 150, chars, 9));
    CHECK_EQ(s_emu_transfers, 9);
    CHECK(memcmp(s_emu_display + 120, chars, 8) == 0);
    CHECK(memcmp(s_emu_display + 150, chars, 9) == 0);
    s_osd._p_device->max_transfer_size = 0;

    // The place of every character of a run, written one by one, is its own.
    CHECK(_setUp());
    for (i = 0; i < MAX7456_COLUMNS; i++) chars[i] = (uint8_t)('a' + i % 26);
    chars[0] = MAX7456_ESCAPE;
    CHECK(pifMax7456_WriteChars(&s_osd, 90, chars, MAX7456_COLUMNS));
    for (i = 0; i < MAX7456_COLUMNS; i++) CHECK_EQ(s_emu_display[90 + i], chars[i]);
}

static void testInvert(void)
{
    CHECK(_setUp());

    s_emu_writes = 0;
    CHECK(pifMax7456_Invert(&s_osd, TRUE));
    CHECK_EQ(s_emu_dmm, MAX7456_OPER_MODE_SELECT_8BIT | MAX7456_INVERT_BIT_INVERT);
    CHECK_EQ(s_emu_writes, 1);

    // Every call writes DMM, so a chip that was reset or written elsewhere comes back to the inversion asked for.
    CHECK(pifMax7456_Invert(&s_osd, TRUE));
    CHECK_EQ(s_emu_writes, 2);

    // The bit goes out with the characters as well.
    CHECK(pifMax7456_WriteChar(&s_osd, 1, 'B'));
    CHECK_EQ(s_emu_dmm, MAX7456_OPER_MODE_SELECT_8BIT | MAX7456_INVERT_BIT_INVERT);
    CHECK_EQ(s_emu_display[1], 'B');

    CHECK(pifMax7456_Invert(&s_osd, FALSE));
    CHECK_EQ(s_emu_dmm, MAX7456_OPER_MODE_SELECT_8BIT);

    // A call that failed still changed what the driver asks for, so the next write carries it.
    s_emu_fail_in = 0;
    CHECK(!pifMax7456_Invert(&s_osd, TRUE));
    s_emu_fail_in = -1;
    CHECK(pifMax7456_WriteChar(&s_osd, 2, 'C'));
    CHECK_EQ(s_emu_dmm, MAX7456_OPER_MODE_SELECT_8BIT | MAX7456_INVERT_BIT_INVERT);
}

static void testBrightness(void)
{
    int i;

    CHECK(_setUp());

    // Black 0% and white 100%: the white level counts the other way round in the register.
    s_emu_transfers = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 0, 2));
    CHECK_EQ(s_emu_transfers, 1);
    for (i = 0; i < 16; i++) {
        CHECK_EQ(s_emu_rb[i], MAX7456_CHAR_BLACK_LEVEL_0 | MAX7456_CHAR_WHITE_LEVEL_100);
    }

    // The same levels again are already there.
    s_emu_transfers = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 0, 2));
    CHECK_EQ(s_emu_transfers, 0);

    // Other levels go out, and so do the same ones once the driver has forgotten them.
    CHECK(pifMax7456_Brightness(&s_osd, 3, 0));
    for (i = 0; i < 16; i++) {
        CHECK_EQ(s_emu_rb[i], MAX7456_CHAR_BLACK_LEVEL_30 | MAX7456_CHAR_WHITE_LEVEL_80);
    }
    pifMax7456_InitBrightness(&s_osd);
    s_emu_transfers = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 3, 0));
    CHECK_EQ(s_emu_transfers, 1);

    // Nothing above the four levels of the register.
    s_emu_writes = 0;
    CHECK(!pifMax7456_Brightness(&s_osd, 4, 0) && pif_error == E_INVALID_PARAM);
    CHECK(!pifMax7456_Brightness(&s_osd, 0, 4) && pif_error == E_INVALID_PARAM);
    CHECK_EQ(s_emu_writes, 0);

    // A port that cannot take the whole burst writes a row at a time.
    CHECK(_setUp());
    s_osd._p_device->max_transfer_size = 2;
    s_emu_transfers = s_emu_writes = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 1, 1));
    CHECK_EQ(s_emu_transfers, 0);
    CHECK_EQ(s_emu_writes, 16);
    for (i = 0; i < 16; i++) {
        CHECK_EQ(s_emu_rb[i], MAX7456_CHAR_BLACK_LEVEL_10 | MAX7456_CHAR_WHITE_LEVEL_90);
    }

    // A row that did not go out leaves the levels to the next call.
    CHECK(_setUp());
    s_osd._p_device->max_transfer_size = 2;
    s_emu_fail_in = 3;
    CHECK(!pifMax7456_Brightness(&s_osd, 0, 2));
    s_emu_fail_in = -1;
    s_emu_writes = 0;
    CHECK(pifMax7456_Brightness(&s_osd, 0, 2));
    CHECK_EQ(s_emu_writes, 16);
    s_osd._p_device->max_transfer_size = 0;
}

static void testWriteNvm(void)
{
    uint8_t font[MAX7456_CHAR_DATA_SIZE];
    uint8_t byte = 0;

    CHECK(_setUp());
    _fill(font, sizeof(font), 0x21);
    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));

    CHECK(!pifMax7456_WriteNvm(&s_osd, 7, NULL) && pif_error == E_INVALID_PARAM);

    CHECK(pifMax7456_WriteNvm(&s_osd, 7, font));
    CHECK(memcmp(s_emu_nvm[7], font, sizeof(font)) == 0);
    CHECK(memcmp(s_emu_shadow, font, sizeof(font)) == 0);
    // The picture was off while the NVM was written, with the rest of VM0 as it stood, and is on again now.
    CHECK_EQ(s_emu_vm0_on_nvm, VM0_PAL & ~MAX7456_ENABLE_DISP_OSD_IMAGE_MASK);
    CHECK_EQ(s_emu_vm0, VM0_PAL);
    CHECK_EQ(s_emu_nvm_busy, 0);

    // Not for a chip that was not found.
    s_osd.detected = FALSE;
    CHECK(!pifMax7456_WriteNvm(&s_osd, 7, font) && pif_error == E_INVALID_STATE);
    s_osd.detected = TRUE;

    // Nor while a background transfer holds the port.
    CHECK(pifDeviceReg_StartWriteRegBytes(s_osd._p_device, MAX7456_REG_W_VM1, &byte, 1));
    CHECK(pifDeviceReg_IsBusy(s_osd._p_device));
    CHECK(!pifMax7456_WriteNvm(&s_osd, 7, font) && pif_error == E_INVALID_STATE);
    CHECK_EQ(pifDeviceReg_CheckTransfer(s_osd._p_device), DRS_COMPLETE);
    CHECK(pifMax7456_WriteNvm(&s_osd, 7, font));

    // A pattern that did not reach the shadow RAM whole is not stored: the NVM write is never asked for.
    CHECK(_setUp());
    s_emu_fail_in = 5;
    CHECK(!pifMax7456_WriteNvm(&s_osd, 3, font));
    CHECK_EQ(s_emu_vm0_on_nvm, 0xFF);
    CHECK_EQ(s_emu_nvm[3][0], 0);

    // An NVM that stays busy is not to be interrupted, so the picture is left off.
    CHECK(_setUp());
    CHECK(pifMax7456_Start(&s_osd, MAX7456_VIDEO_STANDARD_SELECT_PAL));
    s_emu_stuck = TRUE;
    CHECK(!pifMax7456_WriteNvm(&s_osd, 9, font) && pif_error == E_TIMEOUT);
    CHECK_EQ(s_emu_vm0, VM0_PAL & ~MAX7456_ENABLE_DISP_OSD_IMAGE_MASK);
}

static const struct {
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { testInit, "init" },
    { testDetect, "detect" },
    { testStartEnable, "start and enable" },
    { testVideoSignal, "video signal" },
    { testReset, "reset" },
    { testClearDisplay, "clear display" },
    { testWriteChar, "write char" },
    { testWriteChars, "write chars" },
    { testInvert, "invert" },
    { testBrightness, "brightness" },
    { testWriteNvm, "write nvm" },
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
        printf("pif_max7456: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_max7456\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifMax7456_Clear(&s_osd);
    pifSpiPort_Clear(&s_emu_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
