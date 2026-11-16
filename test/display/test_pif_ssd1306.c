// SPDX-License-Identifier: BSD-3-Clause
// PifSsd1306 against a panel model on a fake I2C port: the setup stream, the frame buffer behind the drawing
// surface, and what an update sends.
#include "display/pif_ssd1306.h"

#include <stdio.h>


#define PANEL_WIDTH		128
#define PANEL_PAGES		8
#define CMD_LOG_SIZE	128

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

static BOOL s_failed;
static uint32_t s_now;

static PifI2cPort s_port;
static PifSsd1306 s_oled;

// The panel as the controller keeps it.
static uint8_t s_panel[PANEL_PAGES][PANEL_WIDTH];
static uint8_t s_col_first, s_col_last, s_page_first, s_page_last;
static uint8_t s_col, s_page;
static BOOL s_display_on, s_inverse, s_entire_on;
static uint8_t s_contrast, s_multiplex, s_com_pins, s_memory_mode, s_charge_pump, s_offset, s_start_line;
static uint8_t s_seg_remap, s_com_scan;
static BOOL s_scroll_on;

// What the driver sent, and what the port does with it.
static uint8_t s_cmd_log[CMD_LOG_SIZE];
static int s_cmd_log_count;
static int s_cmd_transfers, s_data_transfers, s_data_bytes;
static BOOL s_fail_write;			// The next transfer fails.
static int s_fail_after;			// Transfers that still go through before s_fail_write counts.

static uint8_t s_pending_cmd;		// Command still waiting for its arguments
static uint8_t s_pending_args[2];
static uint8_t s_pending_count;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _panelReset(void)
{
    memset(s_panel, 0, sizeof(s_panel));
    s_col_first = 0;
    s_col_last = PANEL_WIDTH - 1;
    s_page_first = 0;
    s_page_last = PANEL_PAGES - 1;
    s_col = 0;
    s_page = 0;
    s_display_on = FALSE;
    s_inverse = FALSE;
    s_entire_on = FALSE;
    s_scroll_on = FALSE;
    s_contrast = 0x7F;
    s_multiplex = 0x3F;
    s_com_pins = 0x12;
    s_memory_mode = 0x02;
    s_charge_pump = 0x10;
    s_offset = 0;
    s_start_line = 0;
    s_seg_remap = 0xA0;
    s_com_scan = 0xC0;
    s_pending_cmd = 0;
    s_pending_count = 0;
    s_cmd_log_count = 0;
    s_cmd_transfers = 0;
    s_data_transfers = 0;
    s_data_bytes = 0;
    s_fail_write = FALSE;
    s_fail_after = 0;
}

// How many argument bytes a command takes.
static uint8_t _argCount(uint8_t cmd)
{
    switch (cmd) {
    case SSD1306_CMD_SET_COL_ADDR:
    case SSD1306_CMD_SET_PAGE_ADDR:
        return 2;

    case SSD1306_CMD_SET_MEMORY_MODE:
    case SSD1306_CMD_SET_CONTRAST:
    case SSD1306_CMD_SET_CHARGE_PUMP:
    case SSD1306_CMD_SET_MULTIPLEX:
    case SSD1306_CMD_SET_DISP_OFFSET:
    case SSD1306_CMD_SET_DISP_CLOCK:
    case SSD1306_CMD_SET_PRECHARGE:
    case SSD1306_CMD_SET_COM_PINS:
    case SSD1306_CMD_SET_VCOMH_DESELECT:
        return 1;

    default:
        return 0;
    }
}

static void _panelComplete(uint8_t cmd, const uint8_t *p_args)
{
    switch (cmd) {
    case SSD1306_CMD_SET_COL_ADDR:
        s_col_first = p_args[0];
        s_col_last = p_args[1];
        s_col = s_col_first;
        break;

    case SSD1306_CMD_SET_PAGE_ADDR:
        s_page_first = p_args[0];
        s_page_last = p_args[1];
        s_page = s_page_first;
        break;

    case SSD1306_CMD_SET_MEMORY_MODE:
        s_memory_mode = p_args[0];
        break;

    case SSD1306_CMD_SET_CONTRAST:
        s_contrast = p_args[0];
        break;

    case SSD1306_CMD_SET_CHARGE_PUMP:
        s_charge_pump = p_args[0];
        break;

    case SSD1306_CMD_SET_MULTIPLEX:
        s_multiplex = p_args[0];
        break;

    case SSD1306_CMD_SET_DISP_OFFSET:
        s_offset = p_args[0];
        break;

    case SSD1306_CMD_SET_COM_PINS:
        s_com_pins = p_args[0];
        break;

    default:
        break;
    }
}

static void _panelCmd(uint8_t byte)
{
    if (s_cmd_log_count < CMD_LOG_SIZE) s_cmd_log[s_cmd_log_count] = byte;
    s_cmd_log_count++;

    if (s_pending_cmd) {
        s_pending_args[s_pending_count++] = byte;
        if (s_pending_count >= _argCount(s_pending_cmd)) {
            _panelComplete(s_pending_cmd, s_pending_args);
            s_pending_cmd = 0;
            s_pending_count = 0;
        }
        return;
    }

    if (_argCount(byte)) {
        s_pending_cmd = byte;
        s_pending_count = 0;
        return;
    }

    if (byte >= SSD1306_CMD_SET_START_LINE && byte <= 0x7F) {
        s_start_line = byte & 0x3F;
        return;
    }
    switch (byte) {
    case SSD1306_CMD_SET_DISP_ON:
        s_display_on = TRUE;
        break;

    case SSD1306_CMD_SET_DISP_OFF:
        s_display_on = FALSE;
        break;

    case SSD1306_CMD_SET_INVERSE_DISP:
        s_inverse = TRUE;
        break;

    case SSD1306_CMD_SET_NORMAL_DISP:
        s_inverse = FALSE;
        break;

    case SSD1306_CMD_ENTIRE_DISP_ON:
        s_entire_on = TRUE;
        break;

    case SSD1306_CMD_ENTIRE_DISP_OFF:
        s_entire_on = FALSE;
        break;

    case SSD1306_CMD_SET_SEG_REMAP_OFF:
    case SSD1306_CMD_SET_SEG_REMAP_ON:
        s_seg_remap = byte;
        break;

    case SSD1306_CMD_SET_COM_SCAN_INC:
    case SSD1306_CMD_SET_COM_SCAN_DEC:
        s_com_scan = byte;
        break;

    case SSD1306_CMD_ACTIVATE_SCROLL:
        s_scroll_on = TRUE;
        break;

    case SSD1306_CMD_DEACTIVATE_SCROLL:
        s_scroll_on = FALSE;
        break;

    default:
        break;
    }
}

// Horizontal addressing: along the page inside the column window, then on to the next page.
static void _panelData(uint8_t byte)
{
    if (s_page < PANEL_PAGES && s_col < PANEL_WIDTH) s_panel[s_page][s_col] = byte;
    if (s_col >= s_col_last) {
        s_col = s_col_first;
        s_page = (s_page >= s_page_last) ? s_page_first : s_page + 1;
    }
    else s_col++;
}

static PifI2cReturn _actWrite(PifI2cDevice *p_device, uint32_t iaddr, uint8_t isize, uint8_t *p_data, size_t size)
{
    size_t i;

    (void)p_device;

    if (s_fail_write) {
        if (s_fail_after > 0) s_fail_after--;
        else {
            s_fail_write = FALSE;
            pif_error = E_TRANSFER_FAILED;
            return IR_ERROR;
        }
    }

    // The driver always puts the control byte in the internal address.
    if (isize != 1) {
        printf("  FAIL control byte is missing (isize %u)\n", isize);
        s_failed = TRUE;
        return IR_ERROR;
    }

    if (iaddr == 0x00) {
        s_cmd_transfers++;
        for (i = 0; i < size; i++) _panelCmd(p_data[i]);
    }
    else if (iaddr == 0x40) {
        s_data_transfers++;
        s_data_bytes += (int)size;
        for (i = 0; i < size; i++) _panelData(p_data[i]);
    }
    else {
        printf("  FAIL control byte 0x%02lX\n", (unsigned long)iaddr);
        s_failed = TRUE;
        return IR_ERROR;
    }
    return IR_COMPLETE;
}

// TRUE when the panel holds exactly what the frame buffer of the driver does.
static BOOL _panelMatches(void)
{
    uint16_t x, y;

    for (y = 0; y < s_oled.parent._height; y++) {
        for (x = 0; x < s_oled.parent._width; x++) {
            BOOL on = (s_panel[y >> 3][x] & (1 << (y & 7))) ? TRUE : FALSE;
            if (on != pifSsd1306_GetPixel(&s_oled, x, y)) return FALSE;
        }
    }
    return TRUE;
}

static BOOL _cmdLogHas(uint8_t cmd)
{
    int i;

    for (i = 0; i < s_cmd_log_count && i < CMD_LOG_SIZE; i++) {
        if (s_cmd_log[i] == cmd) return TRUE;
    }
    return FALSE;
}

static int _pixelCount(void)
{
    int count = 0;
    uint16_t x, y;

    for (y = 0; y < s_oled.parent._height; y++) {
        for (x = 0; x < s_oled.parent._width; x++) {
            if (pifSsd1306_GetPixel(&s_oled, x, y)) count++;
        }
    }
    return count;
}

// Sets up a panel of the size a GM009605 has and runs its startup sequence.
static BOOL _begin(void)
{
    _panelReset();
    if (!pifSsd1306_Init(&s_oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, SSD1306_WIDTH,
            SSD1306_HEIGHT)) return FALSE;
    if (!pifSsd1306_Begin(&s_oled)) {
        pifSsd1306_Clear(&s_oled);
        return FALSE;
    }
    return TRUE;
}

static void _end(void)
{
    pifSsd1306_Clear(&s_oled);
}

static void _testInitParams(void)
{
    PifSsd1306 oled;

    CHECK(!pifSsd1306_Init(NULL, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 128, 64));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifSsd1306_Init(&oled, PIF_ID_AUTO, NULL, PIF_SSD1306_I2C_ADDR, NULL, 128, 64));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    // A width past the panel of the controller and a height it does not drive.
    CHECK(!pifSsd1306_Init(&oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 129, 64));
    CHECK(!pifSsd1306_Init(&oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 0, 64));
    CHECK(!pifSsd1306_Init(&oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 128, 48));
    CHECK(!pifSsd1306_Init(&oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 128, 0));

    _panelReset();
    CHECK(pifSsd1306_Init(&oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, 128, 32));
    CHECK_EQ(oled.parent._width, 128);
    CHECK_EQ(oled.parent._height, 32);
    CHECK_EQ(oled._pages, 4);
    CHECK(pifSsd1306_Graphics(&oled) == &oled.parent);
    // Nothing has been sent to the panel yet: the bus may not even be up at this point.
    CHECK_EQ(s_cmd_transfers, 0);
    CHECK_EQ(s_data_transfers, 0);
    // Drawing works before the startup sequence, it only does not show.
    pifGraphics_DrawPixel(pifSsd1306_Graphics(&oled), 1, 1, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_GetPixel(&oled, 1, 1));
    // Everything that talks to the panel waits for pifSsd1306_Begin().
    CHECK(!pifSsd1306_Update(&oled));
    CHECK_EQ(pif_error, E_INVALID_STATE);
    CHECK(!pifSsd1306_Display(&oled));
    CHECK_EQ(pif_error, E_INVALID_STATE);
    pifSsd1306_Clear(&oled);
}

static void _testBegin(void)
{
    CHECK(_begin());

    CHECK_EQ(s_multiplex, SSD1306_HEIGHT - 1);
    CHECK_EQ(s_com_pins, 0x12);					// 64 rows, so the alternative layout
    CHECK_EQ(s_charge_pump, SSD1306_CHARGE_PUMP_ON);
    CHECK_EQ(s_memory_mode, SSD1306_MM_HORIZONTAL);
    CHECK_EQ(s_seg_remap, SSD1306_CMD_SET_SEG_REMAP_ON);
    CHECK_EQ(s_com_scan, SSD1306_CMD_SET_COM_SCAN_DEC);
    CHECK_EQ(s_contrast, 0xCF);
    CHECK_EQ(s_offset, 0);
    CHECK_EQ(s_start_line, 0);
    CHECK(s_display_on);
    CHECK(!s_inverse);
    CHECK(!s_entire_on);
    CHECK(!s_scroll_on);
    CHECK(_cmdLogHas(SSD1306_CMD_SET_DISP_OFF));	// The display is off while the panel is set up
    // The whole frame memory has been cleared, so nothing of what it held at power up is left.
    CHECK_EQ(s_data_bytes, SSD1306_WIDTH * SSD1306_MAX_PAGES);
    CHECK(_panelMatches());
    CHECK_EQ(_pixelCount(), 0);

    // Nothing changed since, so an update sends nothing at all.
    s_data_transfers = 0;
    s_cmd_transfers = 0;
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 0);
    CHECK_EQ(s_cmd_transfers, 0);

    _end();
}

static void _testBeginOf32Rows(void)
{
    _panelReset();
    CHECK(pifSsd1306_Init(&s_oled, PIF_ID_AUTO, &s_port, PIF_SSD1306_I2C_ADDR, NULL, SSD1306_WIDTH, 32));
    CHECK(pifSsd1306_Begin(&s_oled));

    CHECK_EQ(s_multiplex, 31);
    CHECK_EQ(s_com_pins, 0x02);					// 32 rows, so the sequential layout
    CHECK_EQ(s_data_bytes, SSD1306_WIDTH * 4);
    _end();
}

static void _testUpdateSendsOnlyWhatChanged(void)
{
    PifGraphics *p_gfx;

    CHECK(_begin());
    p_gfx = pifSsd1306_Graphics(&s_oled);

    // One pixel: one page, one column.
    s_data_transfers = 0;
    s_data_bytes = 0;
    pifGraphics_DrawPixel(p_gfx, 100, 20, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 1);
    CHECK_EQ(s_data_bytes, 1);
    CHECK_EQ(s_col_first, 100);
    CHECK_EQ(s_col_last, 100);
    CHECK_EQ(s_page_first, 2);					// Row 20 is in page 2
    CHECK_EQ(s_page_last, 2);
    CHECK(_panelMatches());

    // The same value again changes nothing, so nothing is sent.
    s_data_transfers = 0;
    pifGraphics_DrawPixel(p_gfx, 100, 20, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 0);

    // A horizontal line over three columns of one page.
    s_data_transfers = 0;
    s_data_bytes = 0;
    pifGraphics_DrawHorLine(p_gfx, 10, 0, 3, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 1);
    CHECK_EQ(s_data_bytes, 3);
    CHECK_EQ(s_col_first, 10);
    CHECK_EQ(s_col_last, 12);
    CHECK(_panelMatches());

    // A vertical line across two pages comes as one transfer per page.
    s_data_transfers = 0;
    s_data_bytes = 0;
    pifGraphics_DrawVerLine(p_gfx, 60, 30, 10, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 2);
    CHECK_EQ(s_data_bytes, 2);
    CHECK(_panelMatches());

    // Everything again, whether it changed or not.
    s_data_bytes = 0;
    CHECK(pifSsd1306_UpdateAll(&s_oled));
    CHECK_EQ(s_data_bytes, SSD1306_WIDTH * SSD1306_MAX_PAGES);
    CHECK(_panelMatches());

    _end();
}

static void _testUpdateInPieces(void)
{
    CHECK(_begin());

    // A port that takes only short transfers gets the pages in pieces, each with the control byte again.
    s_oled._p_device->max_transfer_size = 7;
    pifGraphics_DrawClear(pifSsd1306_Graphics(&s_oled), SSD1306_COLOR_WHITE);
    s_data_transfers = 0;
    s_data_bytes = 0;
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_bytes, SSD1306_WIDTH * SSD1306_MAX_PAGES);
    CHECK_EQ(s_data_transfers, SSD1306_MAX_PAGES * ((SSD1306_WIDTH + 6) / 7));
    CHECK(_panelMatches());
    CHECK_EQ(_pixelCount(), SSD1306_WIDTH * SSD1306_HEIGHT);

    s_oled._p_device->max_transfer_size = 0;
    _end();
}

static void _testFailedUpdateIsSentAgain(void)
{
    PifGraphics *p_gfx;

    CHECK(_begin());
    p_gfx = pifSsd1306_Graphics(&s_oled);

    pifGraphics_DrawPixel(p_gfx, 0, 0, SSD1306_COLOR_WHITE);
    pifGraphics_DrawPixel(p_gfx, 5, 40, SSD1306_COLOR_WHITE);

    // The first page goes through with its two commands and its data, then the first command of the second
    // page fails.
    s_fail_write = TRUE;
    s_fail_after = 3;
    CHECK(!pifSsd1306_Update(&s_oled));

    // What did not get through is still marked, so the next update sends it.
    s_data_transfers = 0;
    s_data_bytes = 0;
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK_EQ(s_data_transfers, 1);
    CHECK_EQ(s_data_bytes, 1);
    CHECK(_panelMatches());

    _end();
}

static void _testColors(void)
{
    PifGraphics *p_gfx;

    CHECK(_begin());
    p_gfx = pifSsd1306_Graphics(&s_oled);

    // The panel has one bit per pixel: black turns it off, white turns it on and inverse turns it round.
    pifGraphics_DrawPixel(p_gfx, 0, 0, SSD1306_COLOR_WHITE);
    CHECK(pifSsd1306_GetPixel(&s_oled, 0, 0));
    pifGraphics_DrawPixel(p_gfx, 0, 0, SSD1306_COLOR_INVERSE);
    CHECK(!pifSsd1306_GetPixel(&s_oled, 0, 0));
    pifGraphics_DrawPixel(p_gfx, 0, 0, SSD1306_COLOR_INVERSE);
    CHECK(pifSsd1306_GetPixel(&s_oled, 0, 0));
    pifGraphics_DrawPixel(p_gfx, 0, 0, SSD1306_COLOR_BLACK);
    CHECK(!pifSsd1306_GetPixel(&s_oled, 0, 0));

    // Code written for a color display works here too, since black is off and any other color is on.
    pifGraphics_DrawPixel(p_gfx, 1, 1, PIF_COLOR_WHITE);
    pifGraphics_DrawPixel(p_gfx, 2, 2, PIF_COLOR_RED);
    pifGraphics_DrawPixel(p_gfx, 3, 3, PIF_COLOR_BLACK);
    CHECK(pifSsd1306_GetPixel(&s_oled, 1, 1));
    CHECK(pifSsd1306_GetPixel(&s_oled, 2, 2));
    CHECK(!pifSsd1306_GetPixel(&s_oled, 3, 3));

    // A pixel off the panel never reaches the frame buffer.
    pifGraphics_DrawPixel(p_gfx, SSD1306_WIDTH, 0, SSD1306_COLOR_WHITE);
    pifGraphics_DrawPixel(p_gfx, 0, SSD1306_HEIGHT, SSD1306_COLOR_WHITE);
    CHECK(!pifSsd1306_GetPixel(&s_oled, SSD1306_WIDTH, 0));
    CHECK_EQ(_pixelCount(), 2);

    CHECK(pifSsd1306_Update(&s_oled));
    CHECK(_panelMatches());

    _end();
}

static void _testDrawingReachesThePanel(void)
{
    PifGraphics *p_gfx;

    CHECK(_begin());
    p_gfx = pifSsd1306_Graphics(&s_oled);

    // The shapes and the text of the surface all end up on the panel as the frame buffer holds them.
    pifGraphics_DrawRect(p_gfx, 0, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, SSD1306_COLOR_WHITE);
    pifGraphics_DrawLine(p_gfx, 0, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, SSD1306_COLOR_WHITE);
    pifGraphics_DrawFillCircle(p_gfx, 100, 40, 12, SSD1306_COLOR_WHITE);
    pifGraphics_SetCursor(p_gfx, 4, 10);
    pifGraphics_Printf(p_gfx, SSD1306_COLOR_WHITE, "%d.%02d V", 3, 30);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK(_panelMatches());
    CHECK(_pixelCount() > 400);

    // Clearing the surface wipes the panel with it.
    pifGraphics_DrawClear(p_gfx, SSD1306_COLOR_BLACK);
    CHECK(pifSsd1306_Update(&s_oled));
    CHECK(_panelMatches());
    CHECK_EQ(_pixelCount(), 0);

    _end();
}

static void _testPanelControl(void)
{
    CHECK(_begin());

    CHECK(pifSsd1306_NoDisplay(&s_oled));
    CHECK(!s_display_on);
    CHECK(pifSsd1306_Display(&s_oled));
    CHECK(s_display_on);

    CHECK(pifSsd1306_Inverse(&s_oled));
    CHECK(s_inverse);
    CHECK(pifSsd1306_NoInverse(&s_oled));
    CHECK(!s_inverse);

    CHECK(pifSsd1306_SetContrast(&s_oled, 0x20));
    CHECK_EQ(s_contrast, 0x20);

    CHECK(pifSsd1306_SetFlip(&s_oled, TRUE, TRUE));
    CHECK_EQ(s_seg_remap, SSD1306_CMD_SET_SEG_REMAP_OFF);
    CHECK_EQ(s_com_scan, SSD1306_CMD_SET_COM_SCAN_INC);
    CHECK(pifSsd1306_SetFlip(&s_oled, FALSE, FALSE));
    CHECK_EQ(s_seg_remap, SSD1306_CMD_SET_SEG_REMAP_ON);
    CHECK_EQ(s_com_scan, SSD1306_CMD_SET_COM_SCAN_DEC);

    // A failed transfer is reported and leaves the panel as it was.
    s_fail_write = TRUE;
    s_fail_after = 0;
    CHECK(!pifSsd1306_NoDisplay(&s_oled));
    CHECK(s_display_on);

    _end();
}

static void _testClear(void)
{
    CHECK(_begin());

    pifSsd1306_Clear(&s_oled);
    // Everything is turned down after that, and nothing reaches the bus any more.
    s_cmd_transfers = 0;
    CHECK(!pifSsd1306_Update(&s_oled));
    CHECK(!pifSsd1306_Display(&s_oled));
    CHECK_EQ(s_cmd_transfers, 0);
    // The surface has nowhere to put its pixels, so drawing does nothing instead of writing into freed memory.
    pifGraphics_DrawPixel(pifSsd1306_Graphics(&s_oled), 0, 0, SSD1306_COLOR_WHITE);
    pifGraphics_DrawFillRect(pifSsd1306_Graphics(&s_oled), 0, 0, 10, 10, SSD1306_COLOR_WHITE);
    CHECK(!pifSsd1306_GetPixel(&s_oled, 0, 0));
    pifSsd1306_Clear(&s_oled);									// Twice over is no harm

    // The device is off the port, so another panel fits in its place.
    CHECK(_begin());
    _end();
}

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInitParams, "init parameters" },
    { _testBegin, "begin" },
    { _testBeginOf32Rows, "begin of a panel of 32 rows" },
    { _testUpdateSendsOnlyWhatChanged, "update sends only what changed" },
    { _testUpdateInPieces, "update in pieces" },
    { _testFailedUpdateIsSentAgain, "failed update is sent again" },
    { _testColors, "colors of the panel" },
    { _testDrawingReachesThePanel, "drawing reaches the panel" },
    { _testPanelControl, "panel control" },
    { _testClear, "clear" },
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
        printf("pif_ssd1306: setup failed (%d)\n", pif_error);
        return 1;
    }
    if (!pifI2cPort_Init(&s_port, PIF_ID_AUTO, 1)) {
        printf("pif_ssd1306: port failed (%d)\n", pif_error);
        return 1;
    }
    s_port.act_write = _actWrite;

    printf("pif_ssd1306\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        // A test that gave up in the middle left its panel on the port, which only takes one.
        pifSsd1306_Clear(&s_oled);
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    pifI2cPort_Clear(&s_port);
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
