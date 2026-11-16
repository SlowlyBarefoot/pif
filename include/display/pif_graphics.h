// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_graphics.h
 * @brief Base of the graphic displays: the colors, the drawing functions and the text, with no hardware in them.
 *
 * A display driver embeds a PifGraphics as its first member, tells it how large the display is and fills in the
 * four functions that put pixels on it: a pixel, a horizontal line, a vertical line and a filled rectangle.
 * Everything else here, from lines and circles to the text of a font, is built on those four and is the same
 * whatever the display is, so an application draws through the pifGraphics functions alone and the same code
 * works on a TFT LCD behind pif_ili9341 or pif_ssd1963 and on an OLED behind pif_ssd1306.
 *
 * What a color means is left to the driver. A color display takes a PifColor of PIF_COLOR_DEPTH bits, which the
 * macros here build, while a monochrome one takes 0 for off and anything else for on.
 *
 * The text functions draw with pif_font_5x8 until pifGraphics_SetFont() is given another font. A font of its
 * own is made with tools/bmfont2pif.py, which turns an export of BMFont into the C source of a PifFont, glyphs
 * of their own widths and Hangul among them.
 */
#ifndef PIF_GRAPHICS_H
#define PIF_GRAPHICS_H


#include "core/pif.h"


#ifndef PIF_COLOR_DEPTH
#define PIF_COLOR_DEPTH				16	///< Bits of a PifColor: 16 (RGB565) or 32 (XRGB8888). Set it in pif_conf.h.
#endif

#define PIF_COLOR_MAKE16(r8, g8, b8) 	((((b8) & 0xF8) >> 3) + (((g8) & 0xFC) << 3) + (((r8) & 0xF8) << 8))	///< 16 bit RGB 565 color from 8 bit red, green and blue
#define PIF_COLOR_MAKE32(r8, g8, b8) 	((b8) + ((g8) << 8) + ((uint32_t)(r8) << 16))	///< 32 bit RGB 888 color from 8 bit red, green and blue

#define PIF_COLOR_MAKE(r8, g8, b8) 		PIF_CONCAT2(PIF_COLOR_MAKE, PIF_COLOR_DEPTH)(r8, g8, b8)	///< Color of PIF_COLOR_DEPTH bits from 8 bit red, green and blue

/** @name Basic colors
 *  @{
 */
#define PIF_COLOR_BLACK   				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0x00))	///< Black, and the pixel off of a monochrome display
#define PIF_COLOR_BLUE    				((PifColor)PIF_COLOR_MAKE(0x00, 0x00, 0xFF))	///< Blue
#define PIF_COLOR_GREEN   				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0x00))	///< Green
#define PIF_COLOR_RED     				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0x00))	///< Red
#define PIF_COLOR_GRAY   				((PifColor)PIF_COLOR_MAKE(0x7F, 0x7F, 0x7F))	///< Gray
#define PIF_COLOR_CYAN    				((PifColor)PIF_COLOR_MAKE(0x00, 0xFF, 0xFF))	///< Cyan
#define PIF_COLOR_MAGENTA 				((PifColor)PIF_COLOR_MAKE(0xFF, 0x00, 0xFF))	///< Magenta
#define PIF_COLOR_YELLOW  				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0x00))	///< Yellow
#define PIF_COLOR_WHITE   				((PifColor)PIF_COLOR_MAKE(0xFF, 0xFF, 0xFF))	///< White, and a pixel on of a monochrome display
/** @} */

#ifndef PIF_GRAPHICS_PRINTF_SIZE
#define PIF_GRAPHICS_PRINTF_SIZE	32	///< Size of the buffer that pifGraphics_Printf() formats into
#endif


/**
 * @def PIF_GRAPHICS_NO_DEFAULT_FONT
 * @brief Defined in pif_conf.h, this leaves pif_font_5x8 out of the build, which saves its 475 bytes of glyphs
 *        for an application that draws with a font of its own.
 *
 * pifGraphics_SetFont() then has to run before the text functions, which draw nothing until it has, and it takes
 * no NULL to go back to.
 */


/**
 * @brief 16 bit RGB 565 color.
 */
typedef uint16_t PifColor16;

/**
 * @brief 32 bit RGB 888 color.
 */
typedef uint32_t PifColor32;

/**
 * @brief Color of PIF_COLOR_DEPTH (16 or 32) bits.
 */
typedef PIF_CONCAT2(PifColor, PIF_COLOR_DEPTH) PifColor;


/**
 * @class StPifFontGlyph
 * @brief One glyph of a font, as the text functions draw it.
 *
 * The columns of the glyph follow one another in p_data, (height + 7) / 8 bytes each, and bit 0 of the first
 * byte of a column is its topmost row. A font hands this over for a character code, so where the columns come
 * from is its own business: an array in flash, or a buffer it has just read a file into.
 */
typedef struct StPifFontGlyph
{
	const uint8_t *p_data;	///< Columns of the glyph
	uint8_t width;	///< Columns
	uint8_t height;	///< Rows
	int8_t x_offset;	///< Columns from the cursor to the left edge of the glyph
	int8_t y_offset;	///< Rows from the top of the line to the top edge of the glyph
	uint8_t advance;	///< Columns the cursor moves on by
} PifFontGlyph;

/**
 * @class StPifFontGlyphInfo
 * @brief Where one glyph of a font of its own widths sits, and how large it is.
 *
 * A bitmap font generator such as BMFont gives these for every character, and the converter of PIF writes them
 * out as they are.
 */
typedef struct StPifFontGlyphInfo
{
	uint32_t offset;	///< Where the columns of the glyph begin in the glyph data of the font
	uint8_t width;	///< Columns
	uint8_t height;	///< Rows
	int8_t x_offset;	///< Columns from the cursor to the left edge of the glyph
	int8_t y_offset;	///< Rows from the top of the line to the top edge of the glyph
	uint8_t advance;	///< Columns the cursor moves on by
} PifFontGlyphInfo;

/**
 * @class StPifFontRange
 * @brief A run of character codes a font holds, and where the glyph of its first code sits.
 *
 * A font that holds the ASCII characters and the Hangul syllables has one of these for each, so that the
 * glyphs of the two runs follow one another with nothing in between for the codes they skip.
 */
typedef struct StPifFontRange
{
	uint32_t first_code;	///< First character code of the run
	uint32_t last_code;	///< Last character code of the run
	uint16_t first_index;	///< Glyph index of first_code
} PifFontRange;


struct StPifFont;
typedef struct StPifFont PifFont;

/**
 * @brief Hands over the glyph of a character code.
 * @param p_font Font that was asked.
 * @param code Character code, as pifGraphics_DrawString() decoded it from UTF-8.
 * @param p_glyph Filled in with the glyph.
 * @return `TRUE` if the font holds the code, otherwise `FALSE`.
 */
typedef BOOL (*PifFontGetGlyph)(const PifFont *p_font, uint32_t code, PifFontGlyph *p_glyph);

/**
 * @class StPifFont
 * @brief Bitmap font the text functions draw with.
 *
 * A font of one cell, as pif_font_5x8 is, leaves p_glyphs NULL and gives width, height and advance for every
 * glyph. A font of its own widths, as a converted BMFont is, gives p_glyphs instead. Either way the glyph data
 * is the columns of one glyph after another, as StPifFontGlyph describes them.
 *
 * A font that is not held in memory at all sets fn_get_glyph and may leave the rest of the glyph fields alone.
 */
struct StPifFont
{
	uint8_t line_height;	///< Rows the cursor moves per line
	uint8_t base;	///< Rows from the top of the line to the baseline. Only told, never drawn by.
	uint8_t width;	///< Columns of every glyph of a font of one cell, 0 for a font of its own widths
	uint8_t height;	///< Rows of every glyph of a font of one cell, 0 for a font of its own widths
	uint8_t advance;	///< Columns per character of a font of one cell
	uint8_t range_count;	///< Entries of p_ranges
	const PifFontRange *p_ranges;	///< Character codes the font holds, in the order their glyphs are stored
	const uint8_t *p_data;	///< Columns of the glyphs
	const PifFontGlyphInfo *p_glyphs;	///< One per glyph, or NULL for a font of one cell
	PifFontGetGlyph fn_get_glyph;	///< Looks a glyph up itself. NULL reads the font as the fields above say.
};


struct StPifGraphics;
typedef struct StPifGraphics PifGraphics;

/**
 * @brief Puts one pixel on the display. Filled in by the driver.
 */
typedef void (*PifGraphicsDrawPixel)(PifGraphics *p_owner, uint16_t x, uint16_t y, PifColor color);

/**
 * @brief Puts a horizontal line of len pixels on the display. Filled in by the driver.
 */

 typedef void (*PifGraphicsDrawHorLine)(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color);
/**
 * @brief Puts a vertical line of len pixels on the display. Filled in by the driver.
 */

 typedef void (*PifGraphicsDrawVerLine)(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color);
/**
 * @brief Fills a rectangle from (x1, y1) to (x2, y2) on the display. Filled in by the driver.
 */
typedef void (*PifGraphicsDrawFillRect)(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @class StPifGraphics
 * @brief Drawing surface of a display, as the application sees it.
 */
struct StPifGraphics
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	uint16_t _width;	///< Width in pixels
	uint16_t _height;	///< Height in pixels

	// Private Member Variable
	const PifFont *__p_font;
	uint16_t __cursor_x;
	uint16_t __cursor_y;
	PifColor __text_back_color;

	struct {
		uint8_t text_opaque	: 1;	///< The cell of a glyph is filled with __text_back_color
		uint8_t text_wrap	: 1;	///< A glyph that does not fit goes to the next line
	} __bt;

	// Read-only Function
	PifGraphicsDrawPixel _fn_draw_pixel;	///< Set by the display driver
	PifGraphicsDrawHorLine _fn_draw_hor_line;	///< Set by the display driver
	PifGraphicsDrawVerLine _fn_draw_ver_line;	///< Set by the display driver
	PifGraphicsDrawFillRect _fn_draw_fill_rect;	///< Set by the display driver
};


#ifdef __cplusplus
extern "C" {
#endif

#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT

/**
 * @brief Font of 5 x 7 glyphs in a 5 x 8 cell, 6 columns apart, for the characters 0x20 to 0x7E. The text
 *        functions use it until pifGraphics_SetFont() is given another one. Left out of the build by
 *        `PIF_GRAPHICS_NO_DEFAULT_FONT`.
 */
extern const PifFont pif_font_5x8;

#endif	// PIF_GRAPHICS_NO_DEFAULT_FONT

/**
 * @brief Sets up the drawing surface of a display. A display driver calls it from its own init, and then fills
 *        in the four _fn_draw_ functions.
 * @param p_owner Pointer to the graphics instance to initialize.
 * @param id Unique object identifier. Use `PIF_ID_AUTO` to assign one automatically.
 * @param width Width of the display in pixels.
 * @param height Height of the display in pixels.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE` with pif_error set.
 */
BOOL pifGraphics_Init(PifGraphics *p_owner, PifId id, uint16_t width, uint16_t height);

/**
 * @brief Tells the surface how large the display is now, which a driver does when it turns the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param width New width in pixels.
 * @param height New height in pixels.
 */
void pifGraphics_SetSize(PifGraphics *p_owner, uint16_t width, uint16_t height);

/**
 * @brief Puts the corners of a rectangle in order and cuts it down to the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param p_x1 Left X coordinate, swapped with p_x2 if it lies to the right of it.
 * @param p_y1 Top Y coordinate, swapped with p_y2 if it lies below it.
 * @param p_x2 Right X coordinate, lowered to the last column if it lies beyond it.
 * @param p_y2 Bottom Y coordinate, lowered to the last row if it lies beyond it.
 * @return `FALSE` if nothing of the rectangle is on the display, otherwise `TRUE`.
 */
BOOL pifGraphics_ClipArea(PifGraphics *p_owner, uint16_t *p_x1, uint16_t *p_y1, uint16_t *p_x2, uint16_t *p_y2);

/**
 * @brief Draws one pixel. A pixel off the display is not drawn.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column, 0 at the left.
 * @param y Row, 0 at the top.
 * @param color Color of the pixel.
 */
void pifGraphics_DrawPixel(PifGraphics *p_owner, uint16_t x, uint16_t y, PifColor color);

/**
 * @brief Draws a horizontal line of len pixels, clipped to the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the left end.
 * @param y Row of the line.
 * @param len Pixels to draw.
 * @param color Color of the line.
 */
void pifGraphics_DrawHorLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @brief Draws a vertical line of len pixels, clipped to the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the line.
 * @param y Row of the top end.
 * @param len Pixels to draw.
 * @param color Color of the line.
 */
void pifGraphics_DrawVerLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color);

/**
 * @brief Draws a line between two points, clipped to the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x1 Column of one end.
 * @param y1 Row of one end.
 * @param x2 Column of the other end.
 * @param y2 Row of the other end.
 * @param color Color of the line. A line covers the same pixels whichever way round its ends are given.
 */
void pifGraphics_DrawLine(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @brief Draws the outline of a rectangle, clipped to the display. The corners may come in any order, and an
 *        edge that lies off the display is left out instead of being drawn along its border.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x1 Column of one corner.
 * @param y1 Row of one corner.
 * @param x2 Column of the other corner.
 * @param y2 Row of the other corner.
 * @param color Color of the outline.
 */
void pifGraphics_DrawRect(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @brief Fills a rectangle, clipped to the display. The corners may come in any order.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x1 Column of one corner.
 * @param y1 Row of one corner.
 * @param x2 Column of the other corner.
 * @param y2 Row of the other corner.
 * @param color Fill color.
 */
void pifGraphics_DrawFillRect(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color);

/**
 * @brief Draws the outline of a circle, clipped to the display. A radius of 0 draws the center pixel alone.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the center.
 * @param y Row of the center.
 * @param radius Radius in pixels.
 * @param color Color of the outline.
 */
void pifGraphics_DrawCircle(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t radius, PifColor color);

/**
 * @brief Fills a circle, clipped to the display.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the center.
 * @param y Row of the center.
 * @param radius Radius in pixels.
 * @param color Fill color.
 */
void pifGraphics_DrawFillCircle(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t radius, PifColor color);

/**
 * @brief Draws a bitmap, clipped to the display. Only the pixels that are set in it are drawn, so what is
 *        under the others stays.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the left edge.
 * @param y Row of the top edge.
 * @param width Columns of the bitmap.
 * @param height Rows of the bitmap.
 * @param p_bitmap Rows of (width + 7) / 8 bytes, the topmost row first. Bit 7 of the first byte of a row is
 *        its leftmost pixel.
 * @param color Color of the pixels that are set.
 */
void pifGraphics_DrawBitmap(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
		const uint8_t *p_bitmap, PifColor color);

/**
 * @brief Fills the whole display with one color and puts the text cursor back to the top left corner.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param color Color to fill with.
 */
void pifGraphics_DrawClear(PifGraphics *p_owner, PifColor color);

/**
 * @brief Chooses the font the text functions draw with. The surface only keeps the pointer, so the font and its
 *        glyphs have to stay where they are, which lets an application hold them in flash.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param p_font Font to use, or `NULL` to go back to pif_font_5x8. With `PIF_GRAPHICS_NO_DEFAULT_FONT` there is
 *        none to go back to, and `NULL` leaves the surface without a font, as it is before the first call.
 * @return `TRUE` if the font is taken, otherwise `FALSE` with pif_error set: it has no line height, no code
 *         ranges, or, with no fn_get_glyph of its own, no glyph data, or it is a font of one cell whose width,
 *         height or advance does not hold up.
 */
BOOL pifGraphics_SetFont(PifGraphics *p_owner, const PifFont *p_font);

/**
 * @brief Returns the font the text functions draw with.
 * @param p_owner Pointer to an initialized graphics instance.
 * @return The font, or `NULL` if there is none.
 */
const PifFont* pifGraphics_GetFont(PifGraphics *p_owner);

/**
 * @brief Moves the text cursor, which is the top left corner of the next glyph. pifGraphics_Init() and
 *        pifGraphics_DrawClear() put it back to (0, 0).
 * @param p_owner Pointer to an initialized graphics instance.
 * @param x Column of the cursor.
 * @param y Row of the cursor.
 */
void pifGraphics_SetCursor(PifGraphics *p_owner, uint16_t x, uint16_t y);

/**
 * @brief Fills the cell of every glyph with a color, so that text covers what was there before. This is how a
 *        new surface stands, with `PIF_COLOR_BLACK` behind the text.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param color Color behind the glyphs.
 */
void pifGraphics_SetTextBackColor(PifGraphics *p_owner, PifColor color);

/**
 * @brief Draws only the pixels of a glyph, so that text lies over what was there before.
 * @param p_owner Pointer to an initialized graphics instance.
 */
void pifGraphics_SetTextTransparent(PifGraphics *p_owner);

/**
 * @brief Chooses what happens to a glyph that does not fit in what is left of a line.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param wrap `TRUE`, as a new surface stands, moves it to the start of the next line. `FALSE` lets the text
 *        run off the display, where it is clipped away.
 */
void pifGraphics_SetTextWrap(PifGraphics *p_owner, BOOL wrap);

/**
 * @brief Returns how wide a string is in the current font, which lets an application place it itself.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param p_string Text to measure. A line break starts a new line, and the widest line counts.
 * @return Width in pixels, or 0 without a font. The space after the last glyph of a line is counted in, since
 *         that is how far the cursor moves.
 */
uint16_t pifGraphics_GetTextWidth(PifGraphics *p_owner, const char *p_string);

/**
 * @brief Draws the character of one code at the cursor and moves the cursor on by the advance of its glyph.
 *
 * Behind the glyph goes the color of pifGraphics_SetTextBackColor(), over the whole cell of the character,
 * unless pifGraphics_SetTextTransparent() has run. A code the font does not hold is drawn as a filled block,
 * and nothing is drawn at all while the surface has no font.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param code Character code. `'\\n'` moves the cursor to the start of the next line and `'\\r'` to the start
 *        of this one; neither draws anything.
 * @param color Color of the pixels of the glyph.
 */
void pifGraphics_DrawCode(PifGraphics *p_owner, uint32_t code, PifColor color);

/**
 * @brief Draws one character of a single byte, which is pifGraphics_DrawCode() on its code.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param c Character to draw.
 * @param color Color of the pixels of the glyph.
 */
void pifGraphics_DrawChar(PifGraphics *p_owner, char c, PifColor color);

/**
 * @brief Draws a null-terminated string from the cursor, as pifGraphics_DrawCode() draws each character.
 *
 * The string is read as UTF-8, so a font that holds the Hangul syllables draws them from the source as it is
 * written. A byte that is not part of a UTF-8 sequence counts as the code of that byte alone.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param p_string Text to draw.
 * @param color Color of the pixels of the glyphs.
 */
void pifGraphics_DrawString(PifGraphics *p_owner, const char *p_string, PifColor color);

/**
 * @brief Formats text with `printf`-style arguments and draws it with pifGraphics_DrawString(). The text is cut
 *        at `PIF_GRAPHICS_PRINTF_SIZE - 1` characters.
 * @param p_owner Pointer to an initialized graphics instance.
 * @param color Color of the pixels of the glyphs.
 * @param p_format `printf`-style format string.
 */
void pifGraphics_Printf(PifGraphics *p_owner, PifColor color, const char *p_format, ...);

#ifdef __cplusplus
}
#endif


#endif  // PIF_GRAPHICS_H
