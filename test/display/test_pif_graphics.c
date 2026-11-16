// SPDX-License-Identifier: BSD-3-Clause
// PifGraphics on a canvas of its own: the shapes, the clipping and the text, with no display under it.
#include "display/pif_graphics.h"

#include <stdio.h>


#define CANVAS_WIDTH	64
#define CANVAS_HEIGHT	32

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

static PifGraphics s_gfx;
static PifColor s_canvas[CANVAS_HEIGHT][CANVAS_WIDTH];

// What the surface asked the display to do, so that the faster way of drawing a shape can be checked.
static int s_pixel_calls, s_hor_calls, s_ver_calls, s_fill_calls;


static uint32_t _timer1us(void)
{
    return s_now;
}

static void _drawPixel(PifGraphics *p_owner, uint16_t x, uint16_t y, PifColor color)
{
    (void)p_owner;

    s_pixel_calls++;
    if (x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT) {
        printf("  FAIL pixel (%u, %u) is off the canvas\n", x, y);
        s_failed = TRUE;
        return;
    }
    s_canvas[y][x] = color;
}

static void _drawHorLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
    uint16_t i;

    s_hor_calls++;
    for (i = 0; i < len; i++) _drawPixel(p_owner, (uint16_t)(x + i), y, color);
    s_pixel_calls -= len;
}

static void _drawVerLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
    uint16_t i;

    s_ver_calls++;
    for (i = 0; i < len; i++) _drawPixel(p_owner, x, (uint16_t)(y + i), color);
    s_pixel_calls -= len;
}

static void _drawFillRect(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
    uint16_t y;

    s_fill_calls++;
    for (y = y1; y <= y2; y++) {
        _drawHorLine(p_owner, x1, y, (uint16_t)(x2 - x1 + 1), color);
        s_hor_calls--;
    }
}

static void _reset(void)
{
    memset(s_canvas, 0, sizeof(s_canvas));
    s_pixel_calls = 0;
    s_hor_calls = 0;
    s_ver_calls = 0;
    s_fill_calls = 0;
}

static BOOL _begin(void)
{
    _reset();
    if (!pifGraphics_Init(&s_gfx, PIF_ID_AUTO, CANVAS_WIDTH, CANVAS_HEIGHT)) return FALSE;
    s_gfx._fn_draw_pixel = _drawPixel;
    s_gfx._fn_draw_hor_line = _drawHorLine;
    s_gfx._fn_draw_ver_line = _drawVerLine;
    s_gfx._fn_draw_fill_rect = _drawFillRect;
    return TRUE;
}

static int _countColor(PifColor color)
{
    int count = 0;
    uint16_t x, y;

    for (y = 0; y < CANVAS_HEIGHT; y++) {
        for (x = 0; x < CANVAS_WIDTH; x++) {
            if (s_canvas[y][x] == color) count++;
        }
    }
    return count;
}

#define COUNT_SET()		_countColor(PIF_COLOR_WHITE)

static void _testInitParams(void)
{
    PifGraphics gfx;

    CHECK(!pifGraphics_Init(NULL, PIF_ID_AUTO, 64, 32));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    CHECK(!pifGraphics_Init(&gfx, PIF_ID_AUTO, 0, 32));
    CHECK(!pifGraphics_Init(&gfx, PIF_ID_AUTO, 64, 0));

    CHECK(pifGraphics_Init(&gfx, PIF_ID_AUTO, 64, 32));
    CHECK_EQ(gfx._width, 64);
    CHECK_EQ(gfx._height, 32);

    // A surface a display has not filled in draws nothing instead of jumping into nowhere.
    pifGraphics_DrawPixel(&gfx, 1, 1, PIF_COLOR_WHITE);
    pifGraphics_DrawLine(&gfx, 0, 0, 10, 10, PIF_COLOR_WHITE);
    pifGraphics_DrawFillRect(&gfx, 0, 0, 10, 10, PIF_COLOR_WHITE);
    pifGraphics_DrawCircle(&gfx, 5, 5, 3, PIF_COLOR_WHITE);
    pifGraphics_DrawString(&gfx, "x", PIF_COLOR_WHITE);
    pifGraphics_DrawClear(&gfx, PIF_COLOR_WHITE);

    pifGraphics_SetSize(&gfx, 10, 20);
    CHECK_EQ(gfx._width, 10);
    CHECK_EQ(gfx._height, 20);
    // A size of nothing is turned down, so what the display says stays.
    pifGraphics_SetSize(&gfx, 0, 20);
    CHECK_EQ(gfx._width, 10);
}

static void _testClipArea(void)
{
    uint16_t x1, y1, x2, y2;

    CHECK(_begin());

    // The corners come back in order and cut down to the canvas.
    x1 = 50;
    y1 = 20;
    x2 = 10;
    y2 = 5;
    CHECK(pifGraphics_ClipArea(&s_gfx, &x1, &y1, &x2, &y2));
    CHECK_EQ(x1, 10);
    CHECK_EQ(y1, 5);
    CHECK_EQ(x2, 50);
    CHECK_EQ(y2, 20);

    x1 = 0;
    y1 = 0;
    x2 = 1000;
    y2 = 1000;
    CHECK(pifGraphics_ClipArea(&s_gfx, &x1, &y1, &x2, &y2));
    CHECK_EQ(x2, CANVAS_WIDTH - 1);
    CHECK_EQ(y2, CANVAS_HEIGHT - 1);

    // Nothing of it is on the canvas.
    x1 = CANVAS_WIDTH;
    y1 = 0;
    x2 = 1000;
    y2 = 10;
    CHECK(!pifGraphics_ClipArea(&s_gfx, &x1, &y1, &x2, &y2));
}

static void _testPixelAndLines(void)
{
    CHECK(_begin());

    pifGraphics_DrawPixel(&s_gfx, 0, 0, PIF_COLOR_WHITE);
    pifGraphics_DrawPixel(&s_gfx, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 2);

    // A pixel off the canvas never reaches the display.
    s_pixel_calls = 0;
    pifGraphics_DrawPixel(&s_gfx, CANVAS_WIDTH, 0, PIF_COLOR_WHITE);
    pifGraphics_DrawPixel(&s_gfx, 0, CANVAS_HEIGHT, PIF_COLOR_WHITE);
    CHECK_EQ(s_pixel_calls, 0);
    CHECK_EQ(COUNT_SET(), 2);

    // A horizontal line is cut at the right edge, and goes out as one line.
    _reset();
    pifGraphics_DrawHorLine(&s_gfx, CANVAS_WIDTH - 4, 3, 100, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4);
    CHECK_EQ(s_hor_calls, 1);

    // A vertical line the same way at the bottom edge.
    _reset();
    pifGraphics_DrawVerLine(&s_gfx, 7, CANVAS_HEIGHT - 2, 100, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 2);
    CHECK_EQ(s_ver_calls, 1);

    // A line that misses the canvas draws nothing.
    _reset();
    pifGraphics_DrawHorLine(&s_gfx, 0, CANVAS_HEIGHT, 10, PIF_COLOR_WHITE);
    pifGraphics_DrawVerLine(&s_gfx, CANVAS_WIDTH, 0, 10, PIF_COLOR_WHITE);
    pifGraphics_DrawHorLine(&s_gfx, 0, 0, 0, PIF_COLOR_WHITE);
    CHECK_EQ(s_hor_calls, 0);
    CHECK_EQ(s_ver_calls, 0);
}

static void _testLine(void)
{
    CHECK(_begin());

    // A line along a row or a column goes out as the one line the display draws fastest.
    pifGraphics_DrawLine(&s_gfx, 10, 4, 2, 4, PIF_COLOR_WHITE);
    CHECK_EQ(s_hor_calls, 1);
    CHECK_EQ(COUNT_SET(), 9);
    _reset();
    pifGraphics_DrawLine(&s_gfx, 3, 10, 3, 2, PIF_COLOR_WHITE);
    CHECK_EQ(s_ver_calls, 1);
    CHECK_EQ(COUNT_SET(), 9);

    // A diagonal has one pixel per step of its longer axis.
    _reset();
    pifGraphics_DrawLine(&s_gfx, 0, 0, 20, 10, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 21);
    CHECK(s_canvas[0][0] == PIF_COLOR_WHITE);
    CHECK(s_canvas[10][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[5][10] == PIF_COLOR_WHITE);

    // The same line the other way round covers the same pixels, so drawing it in black rubs it out.
    pifGraphics_DrawLine(&s_gfx, 20, 10, 0, 0, PIF_COLOR_BLACK);
    CHECK_EQ(COUNT_SET(), 0);

    // A steep line steps along its vertical axis.
    _reset();
    pifGraphics_DrawLine(&s_gfx, 0, 0, 5, 25, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 26);
    pifGraphics_DrawLine(&s_gfx, 5, 25, 0, 0, PIF_COLOR_BLACK);
    CHECK_EQ(COUNT_SET(), 0);

    // What hangs over the edge is cut away, and the rest is still drawn.
    _reset();
    pifGraphics_DrawLine(&s_gfx, 0, CANVAS_HEIGHT - 2, 40, CANVAS_HEIGHT + 18, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4);			// Two rows of the line are still on the canvas, two pixels each
}

static void _testRect(void)
{
    CHECK(_begin());

    // The corners may come in any order.
    pifGraphics_DrawRect(&s_gfx, 12, 20, 3, 5, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 2 * 10 + 2 * 16 - 4);
    CHECK(s_canvas[5][3] == PIF_COLOR_WHITE);
    CHECK(s_canvas[20][12] == PIF_COLOR_WHITE);
    CHECK(s_canvas[6][4] == PIF_COLOR_BLACK);

    // An edge that lies off the canvas is left out instead of drawn along the border.
    _reset();
    pifGraphics_DrawRect(&s_gfx, 0, 0, CANVAS_WIDTH + 10, CANVAS_HEIGHT + 10, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), CANVAS_WIDTH + CANVAS_HEIGHT - 1);
    CHECK(s_canvas[CANVAS_HEIGHT - 1][CANVAS_WIDTH - 1] == PIF_COLOR_BLACK);

    // A filled rectangle is one call to the display, which fills it in its own way.
    _reset();
    pifGraphics_DrawFillRect(&s_gfx, 12, 20, 3, 5, PIF_COLOR_WHITE);
    CHECK_EQ(s_fill_calls, 1);
    CHECK_EQ(COUNT_SET(), 10 * 16);

    // One that misses the canvas is not handed over at all.
    _reset();
    pifGraphics_DrawFillRect(&s_gfx, CANVAS_WIDTH, 0, CANVAS_WIDTH + 5, 10, PIF_COLOR_WHITE);
    CHECK_EQ(s_fill_calls, 0);
    CHECK_EQ(COUNT_SET(), 0);

    // Clearing fills the whole canvas and puts the text cursor back.
    pifGraphics_SetCursor(&s_gfx, 10, 10);
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), CANVAS_WIDTH * CANVAS_HEIGHT);
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT
    pifGraphics_DrawChar(&s_gfx, '|', PIF_COLOR_WHITE);
    CHECK(s_canvas[0][2] == PIF_COLOR_WHITE);
#endif
}

static void _testCircle(void)
{
    int16_t dx, dy;
    uint16_t x, y;
    int32_t d2;

    CHECK(_begin());

    // A radius of nothing is the center pixel alone.
    pifGraphics_DrawCircle(&s_gfx, 10, 10, 0, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 1);
    CHECK(s_canvas[10][10] == PIF_COLOR_WHITE);

    // Every pixel of an outline lies about one radius from the center, and the four ends are on it.
    _reset();
    pifGraphics_DrawCircle(&s_gfx, 20, 15, 10, PIF_COLOR_WHITE);
    CHECK(s_canvas[5][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[25][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][10] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][30] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][20] == PIF_COLOR_BLACK);			// The middle stays empty
    for (y = 0; y < CANVAS_HEIGHT; y++) {
        for (x = 0; x < CANVAS_WIDTH; x++) {
            if (s_canvas[y][x] != PIF_COLOR_WHITE) continue;
            dx = (int16_t)x - 20;
            dy = (int16_t)y - 15;
            d2 = (int32_t)dx * dx + (int32_t)dy * dy;
            CHECK(d2 >= 9 * 9 && d2 <= 11 * 11);
        }
    }

    // A filled circle covers its outline and its middle, and goes out as vertical lines.
    _reset();
    pifGraphics_DrawFillCircle(&s_gfx, 20, 15, 10, PIF_COLOR_WHITE);
    CHECK(s_canvas[15][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[5][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][10] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][31] == PIF_COLOR_BLACK);
    CHECK(s_ver_calls > 0);
    CHECK_EQ(s_pixel_calls, 0);
    // The disc is 21 pixels across and about as many square as a circle of that width is.
    CHECK_EQ(_countColor(PIF_COLOR_WHITE) > 330, TRUE);
    CHECK_EQ(_countColor(PIF_COLOR_WHITE) < 370, TRUE);
    for (x = 10; x <= 30; x++) CHECK(s_canvas[15][x] == PIF_COLOR_WHITE);
    CHECK(s_canvas[15][9] == PIF_COLOR_BLACK);
    CHECK(s_canvas[15][31] == PIF_COLOR_BLACK);
    for (y = 5; y <= 25; y++) CHECK(s_canvas[y][20] == PIF_COLOR_WHITE);
    CHECK(s_canvas[4][20] == PIF_COLOR_BLACK);
    CHECK(s_canvas[26][20] == PIF_COLOR_BLACK);

    // One at the corner is cut away on two sides and still draws what is on the canvas.
    _reset();
    pifGraphics_DrawFillCircle(&s_gfx, 0, 0, 5, PIF_COLOR_WHITE);
    CHECK(s_canvas[0][0] == PIF_COLOR_WHITE);
    CHECK(s_canvas[0][5] == PIF_COLOR_WHITE);
    CHECK(s_canvas[5][0] == PIF_COLOR_WHITE);
    CHECK(COUNT_SET() > 20 && COUNT_SET() < 40);
}

static void _testBitmap(void)
{
    // 9 pixels wide, so a row is two bytes and the second one is only half used.
    static const uint8_t bitmap[] = {
        0xFF, 0x80,
        0x80, 0x80,
        0xFF, 0x80
    };

    CHECK(_begin());

    pifGraphics_DrawBitmap(&s_gfx, 2, 6, 9, 3, bitmap, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 9 + 2 + 9);
    CHECK(s_canvas[6][2] == PIF_COLOR_WHITE);
    CHECK(s_canvas[6][10] == PIF_COLOR_WHITE);			// The last pixel of the first row
    CHECK(s_canvas[6][11] == PIF_COLOR_BLACK);
    CHECK(s_canvas[7][2] == PIF_COLOR_WHITE);
    CHECK(s_canvas[7][3] == PIF_COLOR_BLACK);

    // What the bitmap does not set stays as it was.
    _reset();
    pifGraphics_DrawFillRect(&s_gfx, 0, 0, 20, 20, PIF_COLOR_WHITE);
    pifGraphics_DrawBitmap(&s_gfx, 2, 6, 9, 3, bitmap, PIF_COLOR_BLACK);
    CHECK_EQ(COUNT_SET(), 21 * 21 - (9 + 2 + 9));

    // A bitmap that hangs over the edge is cut there.
    _reset();
    pifGraphics_DrawBitmap(&s_gfx, CANVAS_WIDTH - 4, CANVAS_HEIGHT - 2, 9, 3, bitmap, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4 + 1);
}

#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT

static void _testText(void)
{
    uint16_t col, x, y;

    CHECK(_begin());

    // The columns of a glyph land on the canvas as the font holds them.
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, 'A', PIF_COLOR_WHITE);
    for (col = 0; col < 5; col++) {
        uint8_t expected = pif_font_5x8.p_data[('A' - 0x20) * 5 + col];

        for (y = 0; y < 8; y++) {
            BOOL on = (expected & (1 << y)) ? TRUE : FALSE;
            CHECK_EQ(s_canvas[y][col] == PIF_COLOR_WHITE, on);
        }
    }
    // The 6th column of the cell is the space to the next glyph.
    for (y = 0; y < 8; y++) CHECK(s_canvas[y][5] == PIF_COLOR_BLACK);
    // The cursor moved on by the advance of the font.
    pifGraphics_DrawChar(&s_gfx, 'A', PIF_COLOR_WHITE);
    CHECK(s_canvas[2][6] == PIF_COLOR_WHITE);

    // The glyphs go out as runs of rows, which is what a display draws fastest, not pixel by pixel.
    _reset();
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, 'A', PIF_COLOR_WHITE);
    CHECK_EQ(s_pixel_calls, 0);
    CHECK(s_ver_calls > 0);

    // A glyph covers what was there before, since the whole cell is drawn.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_WHITE);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, ' ', PIF_COLOR_WHITE);
    CHECK_EQ(_countColor(PIF_COLOR_BLACK), 6 * 8);

    // Transparent text leaves the background alone.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawFillRect(&s_gfx, 0, 0, 5, 7, PIF_COLOR_WHITE);
    pifGraphics_SetTextTransparent(&s_gfx);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, ' ', PIF_COLOR_BLACK);
    CHECK_EQ(COUNT_SET(), 6 * 8);
    pifGraphics_SetTextBackColor(&s_gfx, PIF_COLOR_BLACK);

    // A string of two lines, and the line break draws nothing of its own.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawString(&s_gfx, "AB\nC", PIF_COLOR_WHITE);
    CHECK(s_canvas[2][0] == PIF_COLOR_WHITE);			// 'A'
    CHECK(s_canvas[0][6] == PIF_COLOR_WHITE);			// 'B', which starts at the top row
    CHECK(s_canvas[8][1] == PIF_COLOR_WHITE);			// 'C' on the second line

    // A carriage return goes back to the start of the line it is on.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawString(&s_gfx, "A\rB", PIF_COLOR_WHITE);
    CHECK(s_canvas[0][0] == PIF_COLOR_WHITE);			// 'B' is at the start, over the 'A'
    for (y = 0; y < 8; y++) CHECK(s_canvas[y][6] == PIF_COLOR_BLACK);

    // A line is filled to its end and the rest wraps to the next one.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawString(&s_gfx, "0123456789012", PIF_COLOR_WHITE);	// 13 characters
    // 64 / 6 is 10 glyphs in a line, so three of them wrapped.
    CHECK(s_canvas[8][1] != PIF_COLOR_BLACK || s_canvas[8][2] != PIF_COLOR_BLACK ||
            s_canvas[8][3] != PIF_COLOR_BLACK);
    for (y = 16; y < CANVAS_HEIGHT; y++) {
        for (x = 0; x < CANVAS_WIDTH; x++) CHECK(s_canvas[y][x] == PIF_COLOR_BLACK);
    }

    // Without wrapping the text runs off the display, where it is cut away.
    pifGraphics_SetTextWrap(&s_gfx, FALSE);
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawString(&s_gfx, "0123456789012", PIF_COLOR_WHITE);
    for (y = 8; y < CANVAS_HEIGHT; y++) {
        for (x = 0; x < CANVAS_WIDTH; x++) CHECK(s_canvas[y][x] == PIF_COLOR_BLACK);
    }
    pifGraphics_SetTextWrap(&s_gfx, TRUE);

    // A glyph at a row that is not a multiple of 8 is drawn just as well.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_SetCursor(&s_gfx, 0, 5);
    pifGraphics_DrawChar(&s_gfx, '|', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 7);							// '|' is 7 rows of one column
    for (y = 5; y < 12; y++) CHECK(s_canvas[y][2] == PIF_COLOR_WHITE);

    // Printf formats and draws in one go.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_Printf(&s_gfx, PIF_COLOR_WHITE, "%d", 1);
    CHECK_EQ(COUNT_SET(), 10);							// '1' of the font is 10 pixels

    // A character the font does not hold shows as a filled cell.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawChar(&s_gfx, (char)0x01, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 5 * 8);

    // The width of a string is how far the cursor moves over it, the widest line of several.
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "AB"), 12);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "AB\nC"), 12);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "A\nBCD"), 18);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, ""), 0);
}

#endif	// PIF_GRAPHICS_NO_DEFAULT_FONT

static void _testFont(void)
{
    // Four columns of a glyph 16 rows tall, so two bytes a column.
    static const uint8_t data[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const PifFontRange ranges[] = { { 'a', 'a', 0 } };
    PifFont font = { 16, 15, 4, 16, 4, 1, ranges, data, NULL, NULL };

    CHECK(_begin());

    // No line height, no range, no glyph data, and a cell whose width or advance does not hold up are all
    // turned down.
    font.line_height = 0;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    CHECK_EQ(pif_error, E_INVALID_PARAM);
    font.line_height = 16;
    font.range_count = 0;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    font.range_count = 1;
    font.p_ranges = NULL;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    font.p_ranges = ranges;
    font.p_data = NULL;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    font.p_data = data;
    font.width = 0;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    font.width = 4;
    font.advance = 3;
    CHECK(!pifGraphics_SetFont(&s_gfx, &font));
    font.advance = 4;

    // A font of one cell, two pages tall, drawn at a row that is not a page border.
    CHECK(pifGraphics_SetFont(&s_gfx, &font));
    CHECK(pifGraphics_GetFont(&s_gfx) == &font);
    pifGraphics_SetCursor(&s_gfx, 0, 3);
    pifGraphics_DrawChar(&s_gfx, 'a', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4 * 16);
    CHECK(s_canvas[3][0] == PIF_COLOR_WHITE);
    CHECK(s_canvas[18][3] == PIF_COLOR_WHITE);
    CHECK(s_canvas[2][0] == PIF_COLOR_BLACK);
    CHECK(s_canvas[19][0] == PIF_COLOR_BLACK);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "aa"), 8);

#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT
    // NULL goes back to the font of the library.
    CHECK(pifGraphics_SetFont(&s_gfx, NULL));
    CHECK(pifGraphics_GetFont(&s_gfx) == &pif_font_5x8);
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawChar(&s_gfx, '1', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 10);
#else
    // There is no font of the library to go back to, so NULL leaves the surface without one.
    CHECK(pifGraphics_SetFont(&s_gfx, NULL));
    CHECK(pifGraphics_GetFont(&s_gfx) == NULL);
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_DrawChar(&s_gfx, 'a', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 0);
#endif
}

/*
 * A font whose glyphs have widths and offsets of their own, as a converted BMFont has. The codes of 'i' and 'j'
 * follow one another, so they share a range, while 'o' and 'W' each get one of their own.
 */
static const uint8_t prop_data[] = {
    0x7E,							// 'i': one column, rows 1 to 6
    0x0F,							// 'j': one column of four rows, set off from the cursor
    0x3C, 0x24, 0x3C,				// 'o': three columns
    0x7F, 0x40, 0x7F, 0x40, 0x7F	// 'W': five columns
};

static const PifFontGlyphInfo prop_glyphs[] = {
    //  offset, width, height, x_offset, y_offset, advance
    {        0,     1,      8,        0,        0,       2 },	// 'i'
    {        1,     1,      4,        1,        2,       3 },	// 'j'
    {        2,     3,      8,        0,        0,       4 },	// 'o'
    {        5,     5,      8,        0,        0,       6 }	// 'W'
};

static const PifFontRange prop_ranges[] = {
    { 'W', 'W', 3 },
    { 'i', 'j', 0 },
    { 'o', 'o', 2 }
};

static const PifFont prop_font = { 8, 7, 0, 0, 0, 3, prop_ranges, prop_data, prop_glyphs, NULL };

static void _testProportionalFont(void)
{
    uint16_t y;

    CHECK(_begin());
    CHECK(pifGraphics_SetFont(&s_gfx, &prop_font));

    // Each glyph is as wide as it is, and the cursor moves on by its own advance.
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "i"), 2);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "W"), 6);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "io"), 6);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "ioW"), 12);

    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawString(&s_gfx, "io", PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 6 + 10);				// 'i' is 6 pixels, 'o' is 10
    for (y = 1; y <= 6; y++) CHECK(s_canvas[y][0] == PIF_COLOR_WHITE);
    CHECK(s_canvas[0][0] == PIF_COLOR_BLACK);
    CHECK(s_canvas[2][1] == PIF_COLOR_BLACK);	// The advance of 'i' leaves one column empty
    CHECK(s_canvas[2][2] == PIF_COLOR_WHITE);	// 'o' starts here

    // The offsets move a glyph away from where the cursor is, without moving the cursor.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, 'j', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4);
    for (y = 2; y <= 5; y++) CHECK(s_canvas[y][1] == PIF_COLOR_WHITE);
    CHECK(s_canvas[1][1] == PIF_COLOR_BLACK);
    CHECK(s_canvas[6][1] == PIF_COLOR_BLACK);
    CHECK(s_canvas[2][0] == PIF_COLOR_BLACK);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "j"), 3);

    // The cell of a character is filled over its whole advance, so text covers what was there before.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_WHITE);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, 'i', PIF_COLOR_WHITE);
    CHECK_EQ(_countColor(PIF_COLOR_BLACK), 2 * 8 - 6);

    // A code no range holds shows as a filled block instead of nothing.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawChar(&s_gfx, 'z', PIF_COLOR_WHITE);
    CHECK(COUNT_SET() > 0);
}

/*
 * Two Hangul syllables, far apart in their codes, so each takes a range of its own. Both glyphs take the same
 * cell, so the font needs no table of its own widths.
 */
static const uint8_t utf8_data[] = {
    0xFF, 0xFF, 0xFF, 0xFF,		// U+AC00
    0x0F, 0x0F, 0x0F, 0x0F		// U+B098
};

static const PifFontRange utf8_ranges[] = { { 0xAC00, 0xAC00, 0 }, { 0xB098, 0xB098, 1 } };

static const PifFont utf8_font = { 8, 7, 4, 8, 5, 2, utf8_ranges, utf8_data, NULL, NULL };

static void _testUtf8(void)
{
    // U+AC00 and U+B098 as UTF-8, written out so that the file stays plain ASCII.
    static const char hangul[] = "\xEA\xB0\x80\xEB\x82\x98";

    CHECK(_begin());
    CHECK(pifGraphics_SetFont(&s_gfx, &utf8_font));

    // Three bytes make one character, so the string is two glyphs wide, not six.
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, hangul), 10);

    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawString(&s_gfx, hangul, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4 * 8 + 4 * 4);
    CHECK(s_canvas[0][0] == PIF_COLOR_WHITE);	// U+AC00 fills its cell
    CHECK(s_canvas[7][3] == PIF_COLOR_WHITE);
    CHECK(s_canvas[0][4] == PIF_COLOR_BLACK);	// The advance leaves one column empty
    CHECK(s_canvas[0][5] == PIF_COLOR_WHITE);	// U+B098 begins here, four rows tall
    CHECK(s_canvas[3][8] == PIF_COLOR_WHITE);
    CHECK(s_canvas[4][5] == PIF_COLOR_BLACK);

    // A byte that is no UTF-8 sequence counts as the code of that byte alone, so the string still ends.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    pifGraphics_SetCursor(&s_gfx, 0, 0);
    pifGraphics_DrawString(&s_gfx, "\xFF\xEA", PIF_COLOR_WHITE);
    CHECK(COUNT_SET() > 0);						// Two blocks for two codes the font does not hold
}

#ifdef PIF_GRAPHICS_NO_DEFAULT_FONT

static void _testNoDefaultFont(void)
{
    static const uint8_t data[] = { 0x7F, 0x7F };
    static const PifFontRange ranges[] = { { 'a', 'a', 0 } };
    static const PifFont font = { 8, 7, 2, 8, 3, 1, ranges, data, NULL, NULL };

    CHECK(_begin());

    // The build has no font of the library, so until a font of the application arrives the text functions draw
    // nothing and leave the cursor where it was.
    CHECK(pifGraphics_GetFont(&s_gfx) == NULL);
    pifGraphics_DrawString(&s_gfx, "aaa", PIF_COLOR_WHITE);
    pifGraphics_Printf(&s_gfx, PIF_COLOR_WHITE, "%d", 1);
    pifGraphics_DrawChar(&s_gfx, 'a', PIF_COLOR_WHITE);
    pifGraphics_DrawChar(&s_gfx, '\n', PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 0);
    CHECK_EQ(pifGraphics_GetTextWidth(&s_gfx, "aaa"), 0);

    // The drawing functions work all the same.
    pifGraphics_DrawPixel(&s_gfx, 0, 0, PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 1);

    // With a font of the application the text comes out: two glyphs of two columns of 7 rows each.
    pifGraphics_DrawClear(&s_gfx, PIF_COLOR_BLACK);
    CHECK(pifGraphics_SetFont(&s_gfx, &font));
    pifGraphics_DrawString(&s_gfx, "aa", PIF_COLOR_WHITE);
    CHECK_EQ(COUNT_SET(), 4 * 7);
}

#endif	// PIF_GRAPHICS_NO_DEFAULT_FONT

static const struct
{
    void (*run)(void);
    const char *p_name;
} s_tests[] = {
    { _testInitParams, "init parameters" },
    { _testClipArea, "clip area" },
    { _testPixelAndLines, "pixel and straight lines" },
    { _testLine, "line" },
    { _testRect, "rectangle" },
    { _testCircle, "circle" },
    { _testBitmap, "bitmap" },
#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT
    { _testText, "text" },
#else
    { _testNoDefaultFont, "no font of the library" },
#endif
    { _testFont, "font" },
    { _testProportionalFont, "font of its own widths" },
    { _testUtf8, "utf-8 text" },
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
        printf("pif_graphics: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_graphics\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
