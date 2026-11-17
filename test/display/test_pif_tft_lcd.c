// SPDX-License-Identifier: BSD-3-Clause
// PifTftLcd on a bus that is played here: the setup stream, the address window, the drawing and the rotation.
#include "display/pif_tft_lcd.h"

#include <stdio.h>


#define LOG_SIZE		64
#define MAX_PARAMS		16

#define WIDTH			320
#define HEIGHT			240

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

static PifTftLcd s_lcd;

// What went out on the bus.
static PifTftLcdCmd s_cmd[LOG_SIZE];
static uint32_t s_param[LOG_SIZE][MAX_PARAMS];
static uint8_t s_param_count[LOG_SIZE];
static int s_cmd_count;

static uint32_t s_data[LOG_SIZE];
static int s_data_count;

static uint32_t s_repeat[4];
static uint8_t s_repeat_size;
static uint32_t s_repeat_len;
static int s_repeat_count;

static int s_reset_count, s_select_on, s_select_off;
static uint8_t s_backlight;
static int s_backlight_count;

// What a read of the bus answers, before it is moved back down.
static uint32_t s_answer = 0;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _actReset(void)
{
    s_reset_count++;
}

static void _actChipSelect(SWITCH sw)
{
    if (sw) s_select_on++;
    else s_select_off++;
}

static void _actReadCmd(PifTftLcdCmd cmd, uint32_t *p_data, uint32_t size)
{
    uint32_t i;

    if (s_cmd_count < LOG_SIZE) {
        s_cmd[s_cmd_count] = cmd;
        s_param_count[s_cmd_count] = 0;
    }
    s_cmd_count++;
    for (i = 0; i < size; i++) p_data[i] = s_answer;
}

static void _actWriteCmd(PifTftLcdCmd cmd, uint32_t *p_data, uint32_t size)
{
    uint32_t i;

    if (s_cmd_count < LOG_SIZE) {
        s_cmd[s_cmd_count] = cmd;
        s_param_count[s_cmd_count] = (uint8_t)size;
        for (i = 0; i < size && i < MAX_PARAMS; i++) s_param[s_cmd_count][i] = p_data[i];
    }
    s_cmd_count++;
}

static void _actWriteData(uint32_t *p_data, uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size; i++) {
        if (s_data_count < LOG_SIZE) s_data[s_data_count] = p_data[i];
        s_data_count++;
    }
}

static void _actWriteRepeat(uint32_t *p_data, uint8_t size, uint32_t len)
{
    uint8_t i;

    for (i = 0; i < size && i < 4; i++) s_repeat[i] = p_data[i];
    s_repeat_size = size;
    s_repeat_len = len;
    s_repeat_count++;
}

static void _actBackLight(uint8_t level)
{
    s_backlight = level;
    s_backlight_count++;
}

// A color of two words, so that the size of a pixel can be told apart from its value.
static uint8_t _convertColor(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
    (void)p_owner;

    p_data[0] = (color >> 8) & 0xFF;
    p_data[1] = color & 0xFF;
    return 2;
}

static void _clear(void)
{
    s_cmd_count = 0;
    s_data_count = 0;
    s_repeat_count = 0;
    s_repeat_len = 0;
    s_repeat_size = 0;
    s_reset_count = 0;
    s_select_on = 0;
    s_select_off = 0;
    s_backlight_count = 0;
}

static BOOL _begin(PifTftLcdRotation rotation)
{
    _clear();
    if (!pifTftLcd_Init(&s_lcd, PIF_ID_AUTO, WIDTH, HEIGHT, rotation)) return FALSE;

    s_lcd.__act_reset = _actReset;
    s_lcd.__act_chip_select = _actChipSelect;
    s_lcd.__act_read_cmd = _actReadCmd;
    s_lcd.__act_write_cmd = _actWriteCmd;
    s_lcd.__act_write_data = _actWriteData;
    s_lcd.__act_write_repeat = _actWriteRepeat;
    s_lcd.act_backlight = _actBackLight;

    s_lcd._fn_convert_color = _convertColor;
    s_lcd._fn_set_address = pifTftLcd_SetAddressDcs;
    return TRUE;
}

// Where a command went out, or -1.
static int _find(PifTftLcdCmd cmd)
{
    int i;

    for (i = 0; i < s_cmd_count && i < LOG_SIZE; i++) {
        if (s_cmd[i] == cmd) return i;
    }
    return -1;
}

static void _testInit(void)
{
    PifTftLcd lcd;

    CHECK(!pifTftLcd_Init(NULL, PIF_ID_AUTO, WIDTH, HEIGHT, TLR_0_DEGREE));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifTftLcd_Init(&lcd, PIF_ID_AUTO, WIDTH, HEIGHT, (PifTftLcdRotation)4));
    CHECK_EQ(pif_error, E_INVALID_PARAM);

    // The surface is as large as the panel, with the sides swapped when it is turned a quarter.
    CHECK(_begin(TLR_0_DEGREE));
    CHECK_EQ(s_lcd.parent._width, WIDTH);
    CHECK_EQ(s_lcd.parent._height, HEIGHT);
    CHECK_EQ(s_lcd._rotation, TLR_0_DEGREE);
    CHECK(pifTftLcd_Graphics(&s_lcd) == &s_lcd.parent);

    CHECK(_begin(TLR_90_DEGREE));
    CHECK_EQ(s_lcd.parent._width, HEIGHT);
    CHECK_EQ(s_lcd.parent._height, WIDTH);

    // The drawing of the surface comes from the base, so a driver only fills in what is its own.
    CHECK(s_lcd.parent._fn_draw_pixel == pifTftLcd_DrawPixel);
    CHECK(s_lcd.parent._fn_draw_hor_line == pifTftLcd_DrawHorLine);
    CHECK(s_lcd.parent._fn_draw_ver_line == pifTftLcd_DrawVerLine);
    CHECK(s_lcd.parent._fn_draw_fill_rect == pifTftLcd_DrawFillRect);
}

static void _testWriteCmd(void)
{
    static const uint8_t param[] = { 0x12, 0x34 };

    CHECK(_begin(TLR_0_DEGREE));

    // With no shift the command and its parameters go out as they are.
    pifTftLcd_WriteCmd(&s_lcd, 0x2A, param, 2);
    CHECK_EQ(s_cmd_count, 1);
    CHECK_EQ(s_cmd[0], 0x2A);
    CHECK_EQ(s_param_count[0], 2);
    CHECK_EQ(s_param[0][0], 0x12);
    CHECK_EQ(s_param[0][1], 0x34);

    // An interface that carries the bus in its high bits moves the parameters up with the command.
    _clear();
    pifTftLcd_SetCommandShift(&s_lcd, 10);
    pifTftLcd_WriteCmd(&s_lcd, 0x2A, param, 2);
    CHECK_EQ(s_cmd[0], 0x2A << 10);
    CHECK_EQ(s_param[0][0], 0x12 << 10);
    CHECK_EQ(s_param[0][1], 0x34 << 10);

    // A command of no parameters.
    _clear();
    pifTftLcd_WriteCmd(&s_lcd, 0x11, NULL, 0);
    CHECK_EQ(s_cmd_count, 1);
    CHECK_EQ(s_param_count[0], 0);

    // An answer comes back down the bus the same way.
    _clear();
    s_answer = 0x55 << 10;
    {
        uint32_t data[4];

        CHECK(pifTftLcd_ReadCmd(&s_lcd, 0x04, data, 4));
        CHECK_EQ(s_cmd[0], 0x04 << 10);
        CHECK_EQ(data[0], 0x55);
        CHECK_EQ(data[3], 0x55);
    }
    // Without a way to read the bus the call says so instead of jumping into nowhere.
    s_lcd.__act_read_cmd = NULL;
    {
        uint32_t data[4];

        CHECK(!pifTftLcd_ReadCmd(&s_lcd, 0x04, data, 4));
    }
}

static int s_peek_count;
static uint8_t s_peek_cmd[LOG_SIZE];
static uint8_t s_peek_len[LOG_SIZE];
static uint8_t s_peek_first[LOG_SIZE];

static void _peek(PifTftLcd *p_owner, uint8_t cmd, const uint8_t *p_param, uint8_t len)
{
    (void)p_owner;

    if (s_peek_count < LOG_SIZE) {
        s_peek_cmd[s_peek_count] = cmd;
        s_peek_len[s_peek_count] = len;
        s_peek_first[s_peek_count] = len ? p_param[0] : 0;
    }
    s_peek_count++;
}

static void _testSetup(void)
{
    // Two commands with parameters, a wait of nothing between them, and a command of none.
    static const uint8_t setup[] = {
        0x3A, 1, 0x55,
        TFT_SETUP_DELAY_MS, 0,
        0x36, 2, 0x48, 0x11,
        0x29, 0,
        0
    };
    static const uint8_t too_long[] = { 0x3A, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    CHECK(_begin(TLR_0_DEGREE));

    // The scan sends nothing and shows every entry that is not a wait.
    s_peek_count = 0;
    CHECK(pifTftLcd_ScanSetup(&s_lcd, setup, _peek));
    CHECK_EQ(s_cmd_count, 0);
    CHECK_EQ(s_peek_count, 3);
    CHECK_EQ(s_peek_cmd[0], 0x3A);
    CHECK_EQ(s_peek_len[0], 1);
    CHECK_EQ(s_peek_first[0], 0x55);
    CHECK_EQ(s_peek_cmd[1], 0x36);
    CHECK_EQ(s_peek_len[1], 2);
    CHECK_EQ(s_peek_cmd[2], 0x29);
    CHECK_EQ(s_peek_len[2], 0);

    // An entry of more parameters than a command may carry is turned down.
    CHECK(!pifTftLcd_ScanSetup(&s_lcd, too_long, NULL));
    CHECK_EQ(pif_error, E_WRONG_DATA);

    // A scan without a peek only looks the stream over.
    CHECK(pifTftLcd_ScanSetup(&s_lcd, setup, NULL));

    // Running it sends the commands in order, and the wait sends nothing.
    _clear();
    pifTftLcd_RunSetup(&s_lcd, setup);
    CHECK_EQ(s_cmd_count, 3);
    CHECK_EQ(s_cmd[0], 0x3A);
    CHECK_EQ(s_param[0][0], 0x55);
    CHECK_EQ(s_cmd[1], 0x36);
    CHECK_EQ(s_param[1][1], 0x11);
    CHECK_EQ(s_cmd[2], 0x29);
    CHECK_EQ(s_param_count[2], 0);
}

static void _testSetAddress(void)
{
    CHECK(_begin(TLR_0_DEGREE));

    pifTftLcd_SetAddressDcs(&s_lcd, 0x0123, 0x0456, 0x0789, 0x0ABC);
    CHECK_EQ(s_cmd_count, 3);
    CHECK_EQ(s_cmd[0], PIF_TFT_LCD_CMD_COL_ADDR);
    CHECK_EQ(s_param[0][0], 0x01);
    CHECK_EQ(s_param[0][1], 0x23);
    CHECK_EQ(s_param[0][2], 0x07);
    CHECK_EQ(s_param[0][3], 0x89);
    CHECK_EQ(s_cmd[1], PIF_TFT_LCD_CMD_PAGE_ADDR);
    CHECK_EQ(s_param[1][0], 0x04);
    CHECK_EQ(s_param[1][1], 0x56);
    CHECK_EQ(s_param[1][2], 0x0A);
    CHECK_EQ(s_param[1][3], 0xBC);
    CHECK_EQ(s_cmd[2], PIF_TFT_LCD_CMD_MEM_WRITE);
    CHECK_EQ(s_param_count[2], 0);
}

static void _testDrawing(void)
{
    PifGraphics *p_gfx;

    CHECK(_begin(TLR_0_DEGREE));
    p_gfx = pifTftLcd_Graphics(&s_lcd);

    // One pixel: the window of that one pixel, then the words of its color.
    pifGraphics_DrawPixel(p_gfx, 10, 20, 0xABCD);
    CHECK_EQ(s_select_on, 1);
    CHECK_EQ(s_select_off, 1);
    CHECK_EQ(s_cmd_count, 3);
    CHECK_EQ(s_param[0][1], 10);
    CHECK_EQ(s_param[0][3], 10);
    CHECK_EQ(s_param[1][1], 20);
    CHECK_EQ(s_param[1][3], 20);
    CHECK_EQ(s_data_count, 2);
    CHECK_EQ(s_data[0], 0xAB);
    CHECK_EQ(s_data[1], 0xCD);

    // A pixel off the panel is not drawn, and the bus stays quiet.
    _clear();
    pifGraphics_DrawPixel(p_gfx, WIDTH, 0, 0xABCD);
    pifGraphics_DrawPixel(p_gfx, 0, HEIGHT, 0xABCD);
    CHECK_EQ(s_cmd_count, 0);
    CHECK_EQ(s_select_on, 0);

    // A horizontal line is one window and one repeat of the pixel, cut at the right edge.
    _clear();
    pifGraphics_DrawHorLine(p_gfx, WIDTH - 4, 5, 100, 0xABCD);
    CHECK_EQ(s_repeat_count, 1);
    CHECK_EQ(s_repeat_len, 4);
    CHECK_EQ(s_repeat_size, 2);
    CHECK_EQ(s_param[0][1], (WIDTH - 4) & 0xFF);
    CHECK_EQ(s_param[0][3], (WIDTH - 1) & 0xFF);
    CHECK_EQ(s_param[1][1], 5);
    CHECK_EQ(s_param[1][3], 5);

    // A vertical line the same way at the bottom edge.
    _clear();
    pifGraphics_DrawVerLine(p_gfx, 7, HEIGHT - 2, 100, 0xABCD);
    CHECK_EQ(s_repeat_count, 1);
    CHECK_EQ(s_repeat_len, 2);
    CHECK_EQ(s_param[1][1], (HEIGHT - 2) & 0xFF);
    CHECK_EQ(s_param[1][3], (HEIGHT - 1) & 0xFF);

    // A filled rectangle is one window over the whole area and one repeat of every pixel in it.
    _clear();
    pifGraphics_DrawFillRect(p_gfx, 0, 0, 9, 19, 0xABCD);
    CHECK_EQ(s_cmd_count, 3);
    CHECK_EQ(s_repeat_count, 1);
    CHECK_EQ(s_repeat_len, 10 * 20);

    // The corners may come in any order, and what hangs over the panel is cut away.
    _clear();
    pifGraphics_DrawFillRect(p_gfx, WIDTH + 100, HEIGHT + 100, WIDTH - 2, HEIGHT - 2, 0xABCD);
    CHECK_EQ(s_repeat_len, 2 * 2);

    // One that misses the panel is not sent at all.
    _clear();
    pifGraphics_DrawFillRect(p_gfx, WIDTH, 0, WIDTH + 5, 10, 0xABCD);
    CHECK_EQ(s_repeat_count, 0);
    CHECK_EQ(s_cmd_count, 0);
}

static void _testDrawArea(void)
{
    static PifColor map[6] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666 };
    PifGraphics *p_gfx;

    CHECK(_begin(TLR_0_DEGREE));
    p_gfx = pifTftLcd_Graphics(&s_lcd);

    // Three columns of two rows, two words a pixel, in the order the map holds them.
    pifTftLcd_DrawArea(p_gfx, 0, 0, 2, 1, map);
    CHECK_EQ(s_cmd_count, 3);
    CHECK_EQ(s_data_count, 12);
    CHECK_EQ(s_data[0], 0x11);
    CHECK_EQ(s_data[1], 0x11);
    CHECK_EQ(s_data[10], 0x66);
    CHECK_EQ(s_data[11], 0x66);

    // An area that does not lie entirely on the panel is not drawn, since the map has no stride.
    _clear();
    pifTftLcd_DrawArea(p_gfx, WIDTH - 1, 0, WIDTH, 1, map);
    CHECK_EQ(s_cmd_count, 0);
    pifTftLcd_DrawArea(p_gfx, 2, 0, 0, 1, map);
    CHECK_EQ(s_cmd_count, 0);
    pifTftLcd_DrawArea(p_gfx, 0, 0, 2, 1, NULL);
    CHECK_EQ(s_cmd_count, 0);
}

static BOOL _setRotation(PifTftLcd *p_parent, PifTftLcdRotation rotation)
{
    static const uint8_t mode[] = { 0x48, 0x28, 0x88, 0xE8 };

    if (rotation & 1) pifGraphics_SetSize(&p_parent->parent, HEIGHT, WIDTH);
    else pifGraphics_SetSize(&p_parent->parent, WIDTH, HEIGHT);
    p_parent->_rotation = rotation;
    pifTftLcd_WriteCmd(p_parent, 0x36, mode + rotation, 1);
    return TRUE;
}

static void _testRotationAndBackLight(void)
{
    CHECK(_begin(TLR_0_DEGREE));

    // A driver that does not turn says so instead of doing nothing quietly.
    CHECK(!pifTftLcd_SetRotation(&s_lcd, TLR_90_DEGREE));
    CHECK_EQ(pif_error, E_CANNOT_USE);

    s_lcd._fn_set_rotation = _setRotation;
    CHECK(!pifTftLcd_SetRotation(&s_lcd, (PifTftLcdRotation)4));
    CHECK_EQ(pif_error, E_INVALID_PARAM);

    _clear();
    CHECK(pifTftLcd_SetRotation(&s_lcd, TLR_90_DEGREE));
    CHECK_EQ(s_lcd._rotation, TLR_90_DEGREE);
    CHECK_EQ(s_lcd.parent._width, HEIGHT);
    CHECK_EQ(s_lcd.parent._height, WIDTH);
    CHECK(_find(0x36) >= 0);
    CHECK_EQ(s_param[_find(0x36)][0], 0x28);

    // What is drawn after that follows the sides the panel now has.
    _clear();
    pifGraphics_DrawPixel(pifTftLcd_Graphics(&s_lcd), HEIGHT - 1, WIDTH - 1, 0xABCD);
    CHECK_EQ(s_cmd_count, 3);

    CHECK(pifTftLcd_SetRotation(&s_lcd, TLR_180_DEGREE));
    CHECK_EQ(s_lcd.parent._width, WIDTH);
    CHECK_EQ(s_lcd.parent._height, HEIGHT);

    // The backlight, and a board that does not drive one.
    _clear();
    CHECK(pifTftLcd_SetBackLight(&s_lcd, 70));
    CHECK_EQ(s_backlight_count, 1);
    CHECK_EQ(s_backlight, 70);
    s_lcd.act_backlight = NULL;
    CHECK(!pifTftLcd_SetBackLight(&s_lcd, 70));
    CHECK_EQ(pif_error, E_CANNOT_USE);
}

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInit, "init" },
    { _testWriteCmd, "command and answer on the bus" },
    { _testSetup, "setup stream" },
    { _testSetAddress, "address window" },
    { _testDrawing, "drawing" },
    { _testDrawArea, "area from a color map" },
    { _testRotationAndBackLight, "rotation and backlight" },
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
        printf("pif_tft_lcd: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_tft_lcd\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
