// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_graphics.h"


#ifndef PIF_GRAPHICS_NO_DEFAULT_FONT

/*
 * Glyphs of pif_font_5x8, drawn for PIF. Each one is 5 bytes, one per column from the left, and bit 0 of a byte
 * is the top row. Row 7 holds the descenders alone.
 */
static const uint8_t font_5x8_data[] = {
	0x00, 0x00, 0x00, 0x00, 0x00,	// 0x20 ' '
	0x00, 0x00, 0x5F, 0x00, 0x00,	// 0x21 '!'
	0x00, 0x03, 0x00, 0x03, 0x00,	// 0x22 '"'
	0x14, 0x7F, 0x14, 0x7F, 0x14,	// 0x23 '#'
	0x24, 0x2A, 0x7F, 0x2A, 0x12,	// 0x24 '$'
	0x23, 0x13, 0x08, 0x64, 0x62,	// 0x25 '%'
	0x36, 0x49, 0x55, 0x22, 0x50,	// 0x26 '&'
	0x00, 0x00, 0x03, 0x00, 0x00,	// 0x27 '''
	0x00, 0x1C, 0x22, 0x41, 0x00,	// 0x28 '('
	0x00, 0x41, 0x22, 0x1C, 0x00,	// 0x29 ')'
	0x14, 0x08, 0x3E, 0x08, 0x14,	// 0x2A '*'
	0x08, 0x08, 0x3E, 0x08, 0x08,	// 0x2B '+'
	0x00, 0x80, 0x60, 0x20, 0x00,	// 0x2C ','
	0x08, 0x08, 0x08, 0x08, 0x08,	// 0x2D '-'
	0x00, 0x60, 0x60, 0x00, 0x00,	// 0x2E '.'
	0x60, 0x10, 0x08, 0x04, 0x03,	// 0x2F '/'
	0x3E, 0x51, 0x49, 0x45, 0x3E,	// 0x30 '0'
	0x00, 0x42, 0x7F, 0x40, 0x00,	// 0x31 '1'
	0x42, 0x61, 0x51, 0x49, 0x46,	// 0x32 '2'
	0x21, 0x41, 0x45, 0x4B, 0x31,	// 0x33 '3'
	0x18, 0x14, 0x12, 0x7F, 0x10,	// 0x34 '4'
	0x27, 0x45, 0x45, 0x45, 0x39,	// 0x35 '5'
	0x3C, 0x4A, 0x49, 0x49, 0x30,	// 0x36 '6'
	0x01, 0x01, 0x79, 0x05, 0x03,	// 0x37 '7'
	0x36, 0x49, 0x49, 0x49, 0x36,	// 0x38 '8'
	0x06, 0x49, 0x49, 0x29, 0x1E,	// 0x39 '9'
	0x00, 0x36, 0x36, 0x00, 0x00,	// 0x3A ':'
	0x00, 0x40, 0x36, 0x36, 0x00,	// 0x3B ';'
	0x08, 0x14, 0x22, 0x41, 0x00,	// 0x3C '<'
	0x14, 0x14, 0x14, 0x14, 0x14,	// 0x3D '='
	0x00, 0x41, 0x22, 0x14, 0x08,	// 0x3E '>'
	0x02, 0x01, 0x51, 0x09, 0x06,	// 0x3F '?'
	0x3E, 0x41, 0x5D, 0x55, 0x1E,	// 0x40 '@'
	0x7C, 0x12, 0x11, 0x12, 0x7C,	// 0x41 'A'
	0x7F, 0x49, 0x49, 0x49, 0x36,	// 0x42 'B'
	0x3E, 0x41, 0x41, 0x41, 0x22,	// 0x43 'C'
	0x7F, 0x41, 0x41, 0x41, 0x3E,	// 0x44 'D'
	0x7F, 0x49, 0x49, 0x49, 0x41,	// 0x45 'E'
	0x7F, 0x09, 0x09, 0x09, 0x01,	// 0x46 'F'
	0x3E, 0x41, 0x49, 0x49, 0x3A,	// 0x47 'G'
	0x7F, 0x08, 0x08, 0x08, 0x7F,	// 0x48 'H'
	0x00, 0x41, 0x7F, 0x41, 0x00,	// 0x49 'I'
	0x20, 0x40, 0x41, 0x3F, 0x01,	// 0x4A 'J'
	0x7F, 0x08, 0x14, 0x22, 0x41,	// 0x4B 'K'
	0x7F, 0x40, 0x40, 0x40, 0x40,	// 0x4C 'L'
	0x7F, 0x02, 0x0C, 0x02, 0x7F,	// 0x4D 'M'
	0x7F, 0x04, 0x08, 0x10, 0x7F,	// 0x4E 'N'
	0x3E, 0x41, 0x41, 0x41, 0x3E,	// 0x4F 'O'
	0x7F, 0x09, 0x09, 0x09, 0x06,	// 0x50 'P'
	0x3E, 0x41, 0x51, 0x21, 0x5E,	// 0x51 'Q'
	0x7F, 0x09, 0x19, 0x29, 0x46,	// 0x52 'R'
	0x46, 0x49, 0x49, 0x49, 0x31,	// 0x53 'S'
	0x01, 0x01, 0x7F, 0x01, 0x01,	// 0x54 'T'
	0x3F, 0x40, 0x40, 0x40, 0x3F,	// 0x55 'U'
	0x1F, 0x20, 0x40, 0x20, 0x1F,	// 0x56 'V'
	0x7F, 0x20, 0x18, 0x20, 0x7F,	// 0x57 'W'
	0x63, 0x14, 0x08, 0x14, 0x63,	// 0x58 'X'
	0x03, 0x04, 0x78, 0x04, 0x03,	// 0x59 'Y'
	0x61, 0x51, 0x49, 0x45, 0x43,	// 0x5A 'Z'
	0x00, 0x7F, 0x41, 0x41, 0x00,	// 0x5B '['
	0x03, 0x04, 0x08, 0x10, 0x60,	// 0x5C '\'
	0x00, 0x41, 0x41, 0x7F, 0x00,	// 0x5D ']'
	0x04, 0x02, 0x01, 0x02, 0x04,	// 0x5E '^'
	0x80, 0x80, 0x80, 0x80, 0x80,	// 0x5F '_'
	0x00, 0x01, 0x02, 0x00, 0x00,	// 0x60 '`'
	0x20, 0x54, 0x54, 0x54, 0x78,	// 0x61 'a'
	0x7F, 0x44, 0x44, 0x44, 0x38,	// 0x62 'b'
	0x38, 0x44, 0x44, 0x44, 0x28,	// 0x63 'c'
	0x38, 0x44, 0x44, 0x44, 0x7F,	// 0x64 'd'
	0x38, 0x54, 0x54, 0x54, 0x18,	// 0x65 'e'
	0x08, 0x7E, 0x09, 0x01, 0x02,	// 0x66 'f'
	0x18, 0xA4, 0xA4, 0xA4, 0x7C,	// 0x67 'g'
	0x7F, 0x04, 0x04, 0x04, 0x78,	// 0x68 'h'
	0x00, 0x44, 0x7D, 0x44, 0x00,	// 0x69 'i'
	0x40, 0x80, 0x80, 0x7D, 0x00,	// 0x6A 'j'
	0x7F, 0x10, 0x28, 0x44, 0x00,	// 0x6B 'k'
	0x00, 0x41, 0x7F, 0x40, 0x00,	// 0x6C 'l'
	0x7C, 0x04, 0x18, 0x04, 0x78,	// 0x6D 'm'
	0x7C, 0x04, 0x04, 0x04, 0x78,	// 0x6E 'n'
	0x38, 0x44, 0x44, 0x44, 0x38,	// 0x6F 'o'
	0xFC, 0x24, 0x24, 0x24, 0x18,	// 0x70 'p'
	0x18, 0x24, 0x24, 0x24, 0xFC,	// 0x71 'q'
	0x7C, 0x08, 0x04, 0x04, 0x08,	// 0x72 'r'
	0x48, 0x54, 0x54, 0x54, 0x24,	// 0x73 's'
	0x04, 0x3F, 0x44, 0x44, 0x20,	// 0x74 't'
	0x3C, 0x40, 0x40, 0x40, 0x7C,	// 0x75 'u'
	0x1C, 0x20, 0x40, 0x20, 0x1C,	// 0x76 'v'
	0x3C, 0x40, 0x38, 0x40, 0x3C,	// 0x77 'w'
	0x44, 0x28, 0x10, 0x28, 0x44,	// 0x78 'x'
	0x1C, 0xA0, 0xA0, 0xA0, 0x7C,	// 0x79 'y'
	0x44, 0x64, 0x54, 0x4C, 0x44,	// 0x7A 'z'
	0x00, 0x08, 0x36, 0x41, 0x41,	// 0x7B '{'
	0x00, 0x00, 0x7F, 0x00, 0x00,	// 0x7C '|'
	0x41, 0x41, 0x36, 0x08, 0x00,	// 0x7D '}'
	0x0C, 0x02, 0x04, 0x08, 0x06	// 0x7E '~'
};

static const PifFontRange font_5x8_ranges[] = { { 0x20, 0x7E, 0 } };

const PifFont pif_font_5x8 = {
	8,		// line_height
	7,		// base
	5,		// width
	8,		// height
	6,		// advance
	1,		// range_count
	font_5x8_ranges,
	font_5x8_data,
	NULL,	// p_glyphs: every glyph takes the same cell
	NULL	// fn_get_glyph
};

#define DEFAULT_FONT	&pif_font_5x8

#else

// The text functions draw nothing until pifGraphics_SetFont() has been given a font of the application.
#define DEFAULT_FONT	NULL

#endif	// PIF_GRAPHICS_NO_DEFAULT_FONT


// Ready to draw on: pifGraphics_Init() has run and a driver has filled in its functions.
static BOOL _checkDraw(PifGraphics *p_owner)
{
	return p_owner && p_owner->_fn_draw_pixel && p_owner->_fn_draw_hor_line && p_owner->_fn_draw_ver_line &&
			p_owner->_fn_draw_fill_rect;
}

// The pixel and the line functions again, on coordinates that the drawing of a shape may have taken off the
// display on any side. They are only called with a surface that _checkDraw has passed.
static void _pixel(PifGraphics *p_owner, int32_t x, int32_t y, PifColor color)
{
	if (x < 0 || y < 0 || x >= (int32_t)p_owner->_width || y >= (int32_t)p_owner->_height) return;

	(*p_owner->_fn_draw_pixel)(p_owner, (uint16_t)x, (uint16_t)y, color);
}

static void _horLine(PifGraphics *p_owner, int32_t x, int32_t y, int32_t len, PifColor color)
{
	if (len <= 0 || y < 0 || y >= (int32_t)p_owner->_height) return;

	if (x < 0) {
		len += x;
		x = 0;
	}
	if (len <= 0 || x >= (int32_t)p_owner->_width) return;
	if (x + len > (int32_t)p_owner->_width) len = (int32_t)p_owner->_width - x;

	(*p_owner->_fn_draw_hor_line)(p_owner, (uint16_t)x, (uint16_t)y, (uint16_t)len, color);
}

static void _verLine(PifGraphics *p_owner, int32_t x, int32_t y, int32_t len, PifColor color)
{
	if (len <= 0 || x < 0 || x >= (int32_t)p_owner->_width) return;

	if (y < 0) {
		len += y;
		y = 0;
	}
	if (len <= 0 || y >= (int32_t)p_owner->_height) return;
	if (y + len > (int32_t)p_owner->_height) len = (int32_t)p_owner->_height - y;

	(*p_owner->_fn_draw_ver_line)(p_owner, (uint16_t)x, (uint16_t)y, (uint16_t)len, color);
}

static void _fillRect(PifGraphics *p_owner, int32_t x1, int32_t y1, int32_t x2, int32_t y2, PifColor color)
{
	if (x2 < x1 || y2 < y1) return;
	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x1 >= (int32_t)p_owner->_width || y1 >= (int32_t)p_owner->_height) return;
	if (x2 >= (int32_t)p_owner->_width) x2 = (int32_t)p_owner->_width - 1;
	if (y2 >= (int32_t)p_owner->_height) y2 = (int32_t)p_owner->_height - 1;

	(*p_owner->_fn_draw_fill_rect)(p_owner, (uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2, color);
}

PIF_STATIC_INLINE BOOL _glyphBit(const uint8_t *p_col, uint8_t row)
{
	return (p_col[row >> 3] >> (row & 7)) & 1;
}

/**
 * @fn _drawGlyphColumn
 * @brief Draws the pixels one column of a glyph sets, height rows from y down.
 *
 * The rows go out as runs rather than pixel by pixel, which is what a driver draws fastest: a run is a vertical
 * line, and a monochrome driver turns one into a single operation on its frame buffer. What the glyph leaves
 * unset is not touched, so whatever fills the cell has to be there already.
 */
static void _drawGlyphColumn(PifGraphics *p_owner, int32_t x, int32_t y, const uint8_t *p_col, uint8_t height,
		PifColor color)
{
	uint8_t row = 0, run;

	while (row < height) {
		if (!_glyphBit(p_col, row)) {
			row++;
			continue;
		}
		run = 1;
		while (row + run < height && _glyphBit(p_col, (uint8_t)(row + run))) run++;
		_verLine(p_owner, x, y + row, run, color);
		row += run;
	}
}

/**
 * @fn _getGlyph
 * @brief Looks a character code up in a font that keeps its glyphs where its fields say.
 * @return TRUE if the font holds the code, otherwise FALSE.
 */
static BOOL _getGlyph(const PifFont *p_font, uint32_t code, PifFontGlyph *p_glyph)
{
	const PifFontRange *p_range;
	uint32_t index;
	uint8_t i;

	for (i = 0; i < p_font->range_count; i++) {
		p_range = &p_font->p_ranges[i];
		if (code < p_range->first_code || code > p_range->last_code) continue;

		index = p_range->first_index + (code - p_range->first_code);
		if (p_font->p_glyphs) {
			const PifFontGlyphInfo *p_info = &p_font->p_glyphs[index];

			p_glyph->p_data = p_font->p_data + p_info->offset;
			p_glyph->width = p_info->width;
			p_glyph->height = p_info->height;
			p_glyph->x_offset = p_info->x_offset;
			p_glyph->y_offset = p_info->y_offset;
			p_glyph->advance = p_info->advance;
		}
		else {
			// Every glyph takes the same cell, so where it sits follows from its index alone.
			uint32_t pages = ((uint32_t)p_font->height + 7) / 8;

			p_glyph->p_data = p_font->p_data + index * p_font->width * pages;
			p_glyph->width = p_font->width;
			p_glyph->height = p_font->height;
			p_glyph->x_offset = 0;
			p_glyph->y_offset = 0;
			p_glyph->advance = p_font->advance;
		}
		return TRUE;
	}
	return FALSE;
}

// How wide the block that stands in for a code the font does not hold is.
static uint8_t _missingWidth(const PifFont *p_font)
{
	return p_font->advance ? p_font->advance : (uint8_t)((p_font->line_height + 1) / 2);
}

/**
 * @fn _decodeUtf8
 * @brief Takes the next character code off a UTF-8 string.
 *
 * A byte that does not start a sequence, or a sequence that breaks off, counts as the code of that byte alone,
 * so a string of any bytes is read to its end instead of running away.
 * @return Where the next character begins.
 */
static const char* _decodeUtf8(const char *p_string, uint32_t *p_code)
{
	uint8_t first = (uint8_t)*p_string;
	uint32_t code;
	uint8_t i, len;

	if (first < 0x80) {
		*p_code = first;
		return p_string + 1;
	}
	if ((first & 0xE0) == 0xC0) {
		len = 2;
		code = first & 0x1F;
	}
	else if ((first & 0xF0) == 0xE0) {
		len = 3;
		code = first & 0x0F;
	}
	else if ((first & 0xF8) == 0xF0) {
		len = 4;
		code = first & 0x07;
	}
	else {
		*p_code = first;
		return p_string + 1;
	}

	for (i = 1; i < len; i++) {
		if ((p_string[i] & 0xC0) != 0x80) {
			*p_code = first;
			return p_string + 1;
		}
		code = (code << 6) | (p_string[i] & 0x3F);
	}
	*p_code = code;
	return p_string + len;
}

BOOL pifGraphics_Init(PifGraphics *p_owner, PifId id, uint16_t width, uint16_t height)
{
	if (!p_owner || !width || !height) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGraphics));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_width = width;
	p_owner->_height = height;

	p_owner->__p_font = DEFAULT_FONT;
	p_owner->__text_back_color = PIF_COLOR_BLACK;
	p_owner->__bt.text_opaque = TRUE;
	p_owner->__bt.text_wrap = TRUE;
	return TRUE;
}

void pifGraphics_SetSize(PifGraphics *p_owner, uint16_t width, uint16_t height)
{
	if (!p_owner || !width || !height) return;

	p_owner->_width = width;
	p_owner->_height = height;
}

BOOL pifGraphics_ClipArea(PifGraphics *p_owner, uint16_t *p_x1, uint16_t *p_y1, uint16_t *p_x2, uint16_t *p_y2)
{
	uint16_t tmp;

	if (!p_owner || !p_x1 || !p_y1 || !p_x2 || !p_y2) return FALSE;

	if (*p_x1 > *p_x2) {
		tmp = *p_x1;
		*p_x1 = *p_x2;
		*p_x2 = tmp;
	}
	if (*p_y1 > *p_y2) {
		tmp = *p_y1;
		*p_y1 = *p_y2;
		*p_y2 = tmp;
	}
	if (*p_x1 >= p_owner->_width || *p_y1 >= p_owner->_height) return FALSE;

	if (*p_x2 >= p_owner->_width) *p_x2 = p_owner->_width - 1;
	if (*p_y2 >= p_owner->_height) *p_y2 = p_owner->_height - 1;
	return TRUE;
}

void pifGraphics_DrawPixel(PifGraphics *p_owner, uint16_t x, uint16_t y, PifColor color)
{
	if (!_checkDraw(p_owner) || x >= p_owner->_width || y >= p_owner->_height) return;

	(*p_owner->_fn_draw_pixel)(p_owner, x, y, color);
}

void pifGraphics_DrawHorLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	if (!_checkDraw(p_owner)) return;

	_horLine(p_owner, x, y, len, color);
}

void pifGraphics_DrawVerLine(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	if (!_checkDraw(p_owner)) return;

	_verLine(p_owner, x, y, len, color);
}

void pifGraphics_DrawLine(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
	int32_t ax1, ay1, ax2, ay2, dx, dy, err, y_step, tmp;
	BOOL steep;

	if (!_checkDraw(p_owner)) return;

	if (y1 == y2) {
		_horLine(p_owner, (x1 < x2) ? x1 : x2, y1, (x1 < x2) ? x2 - x1 + 1 : x1 - x2 + 1, color);
		return;
	}
	if (x1 == x2) {
		_verLine(p_owner, x1, (y1 < y2) ? y1 : y2, (y1 < y2) ? y2 - y1 + 1 : y1 - y2 + 1, color);
		return;
	}

	// Bresenham along the longer axis, and always from the lower end of it, so that a line covers the same
	// pixels whichever way round its ends are given.
	ax1 = x1;
	ay1 = y1;
	ax2 = x2;
	ay2 = y2;
	dx = (ax2 > ax1) ? ax2 - ax1 : ax1 - ax2;
	dy = (ay2 > ay1) ? ay2 - ay1 : ay1 - ay2;
	steep = dy > dx;
	if (steep) {
		tmp = ax1;
		ax1 = ay1;
		ay1 = tmp;
		tmp = ax2;
		ax2 = ay2;
		ay2 = tmp;
	}
	if (ax1 > ax2) {
		tmp = ax1;
		ax1 = ax2;
		ax2 = tmp;
		tmp = ay1;
		ay1 = ay2;
		ay2 = tmp;
	}
	dx = ax2 - ax1;
	dy = (ay2 > ay1) ? ay2 - ay1 : ay1 - ay2;
	y_step = (ay1 < ay2) ? 1 : -1;
	err = dx / 2;
	for (; ax1 <= ax2; ax1++) {
		if (steep) _pixel(p_owner, ay1, ax1, color);
		else _pixel(p_owner, ax1, ay1, color);
		err -= dy;
		if (err < 0) {
			ay1 += y_step;
			err += dx;
		}
	}
}

void pifGraphics_DrawRect(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
	uint16_t tmp;

	if (!_checkDraw(p_owner)) return;

	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}
	// An edge that lies off the display is left out, instead of being drawn along its border.
	_horLine(p_owner, x1, y1, (int32_t)x2 - x1 + 1, color);
	if (y2 < p_owner->_height) _horLine(p_owner, x1, y2, (int32_t)x2 - x1 + 1, color);
	_verLine(p_owner, x1, y1, (int32_t)y2 - y1 + 1, color);
	if (x2 < p_owner->_width) _verLine(p_owner, x2, y1, (int32_t)y2 - y1 + 1, color);
}

void pifGraphics_DrawFillRect(PifGraphics *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
	if (!_checkDraw(p_owner)) return;
	if (!pifGraphics_ClipArea(p_owner, &x1, &y1, &x2, &y2)) return;

	(*p_owner->_fn_draw_fill_rect)(p_owner, x1, y1, x2, y2, color);
}

void pifGraphics_DrawCircle(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t radius, PifColor color)
{
	int32_t cx = x, cy = y, r = radius;
	int32_t f, ddf_x, ddf_y, px, py;

	if (!_checkDraw(p_owner)) return;

	if (!radius) {
		_pixel(p_owner, cx, cy, color);
		return;
	}

	f = 1 - r;
	ddf_x = 1;
	ddf_y = -2 * r;
	px = 0;
	py = r;

	_pixel(p_owner, cx, cy + r, color);
	_pixel(p_owner, cx, cy - r, color);
	_pixel(p_owner, cx + r, cy, color);
	_pixel(p_owner, cx - r, cy, color);

	while (px < py) {
		if (f >= 0) {
			py--;
			ddf_y += 2;
			f += ddf_y;
		}
		px++;
		ddf_x += 2;
		f += ddf_x;

		_pixel(p_owner, cx + px, cy + py, color);
		_pixel(p_owner, cx - px, cy + py, color);
		_pixel(p_owner, cx + px, cy - py, color);
		_pixel(p_owner, cx - px, cy - py, color);
		_pixel(p_owner, cx + py, cy + px, color);
		_pixel(p_owner, cx - py, cy + px, color);
		_pixel(p_owner, cx + py, cy - px, color);
		_pixel(p_owner, cx - py, cy - px, color);
	}
}

void pifGraphics_DrawFillCircle(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t radius, PifColor color)
{
	int32_t cx = x, cy = y, r = radius;
	int32_t f, ddf_x, ddf_y, px, py;

	if (!_checkDraw(p_owner)) return;

	if (!radius) {
		_pixel(p_owner, cx, cy, color);
		return;
	}

	f = 1 - r;
	ddf_x = 1;
	ddf_y = -2 * r;
	px = 0;
	py = r;

	_verLine(p_owner, cx, cy - r, 2 * r + 1, color);

	while (px < py) {
		if (f >= 0) {
			py--;
			ddf_y += 2;
			f += ddf_y;
		}
		px++;
		ddf_x += 2;
		f += ddf_x;

		_verLine(p_owner, cx + px, cy - py, 2 * py + 1, color);
		_verLine(p_owner, cx - px, cy - py, 2 * py + 1, color);
		_verLine(p_owner, cx + py, cy - px, 2 * px + 1, color);
		_verLine(p_owner, cx - py, cy - px, 2 * px + 1, color);
	}
}

void pifGraphics_DrawBitmap(PifGraphics *p_owner, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
		const uint8_t *p_bitmap, PifColor color)
{
	uint32_t row_bytes;
	uint16_t row, col;

	if (!_checkDraw(p_owner) || !p_bitmap || !width || !height) return;

	row_bytes = ((uint32_t)width + 7) / 8;
	for (row = 0; row < height; row++) {
		if ((uint32_t)y + row >= p_owner->_height) break;
		for (col = 0; col < width; col++) {
			if ((uint32_t)x + col >= p_owner->_width) break;
			if (p_bitmap[(uint32_t)row * row_bytes + (col >> 3)] & (0x80 >> (col & 7))) {
				(*p_owner->_fn_draw_pixel)(p_owner, (uint16_t)(x + col), (uint16_t)(y + row), color);
			}
		}
	}
}

void pifGraphics_DrawClear(PifGraphics *p_owner, PifColor color)
{
	if (!_checkDraw(p_owner)) return;

	(*p_owner->_fn_draw_fill_rect)(p_owner, 0, 0, p_owner->_width - 1, p_owner->_height - 1, color);
	p_owner->__cursor_x = 0;
	p_owner->__cursor_y = 0;
}

BOOL pifGraphics_SetFont(PifGraphics *p_owner, const PifFont *p_font)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_font) {
		p_owner->__p_font = DEFAULT_FONT;
		return TRUE;
	}
	if (!p_font->line_height) goto fail;
	if (!p_font->fn_get_glyph) {
		// A font that does not look its glyphs up itself has to say where they are.
		if (!p_font->range_count || !p_font->p_ranges || !p_font->p_data) goto fail;
		if (!p_font->p_glyphs) {
			// Every glyph takes the same cell, so the cell has to hold up.
			if (!p_font->width || !p_font->height || p_font->advance < p_font->width) goto fail;
		}
	}
	p_owner->__p_font = p_font;
	return TRUE;

fail:
	pif_error = E_INVALID_PARAM;
	return FALSE;
}

const PifFont* pifGraphics_GetFont(PifGraphics *p_owner)
{
	return p_owner ? p_owner->__p_font : NULL;
}

void pifGraphics_SetCursor(PifGraphics *p_owner, uint16_t x, uint16_t y)
{
	if (!p_owner) return;

	p_owner->__cursor_x = x;
	p_owner->__cursor_y = y;
}

void pifGraphics_SetTextBackColor(PifGraphics *p_owner, PifColor color)
{
	if (!p_owner) return;

	p_owner->__text_back_color = color;
	p_owner->__bt.text_opaque = TRUE;
}

void pifGraphics_SetTextTransparent(PifGraphics *p_owner)
{
	if (!p_owner) return;

	p_owner->__bt.text_opaque = FALSE;
}

void pifGraphics_SetTextWrap(PifGraphics *p_owner, BOOL wrap)
{
	if (!p_owner) return;

	p_owner->__bt.text_wrap = wrap ? 1 : 0;
}

uint16_t pifGraphics_GetTextWidth(PifGraphics *p_owner, const char *p_string)
{
	const PifFont *p_font;
	PifFontGlyph glyph;
	uint32_t code, line = 0, widest = 0;

	if (!p_owner || !p_string || !p_owner->__p_font) return 0;

	p_font = p_owner->__p_font;
	while (*p_string) {
		p_string = _decodeUtf8(p_string, &code);
		if (code == '\n') {
			if (line > widest) widest = line;
			line = 0;
		}
		else if (code != '\r') {
			if ((*(p_font->fn_get_glyph ? p_font->fn_get_glyph : _getGlyph))(p_font, code, &glyph)) {
				line += glyph.advance;
			}
			else line += _missingWidth(p_font);
		}
	}
	if (line > widest) widest = line;
	return (widest > 0xFFFF) ? 0xFFFF : (uint16_t)widest;
}

void pifGraphics_DrawCode(PifGraphics *p_owner, uint32_t code, PifColor color)
{
	const PifFont *p_font;
	PifFontGlyph glyph;
	uint32_t x, y, next;
	uint8_t advance, col, pages;
	BOOL found;

	if (!_checkDraw(p_owner)) return;

	p_font = p_owner->__p_font;
	if (!p_font) return;

	if (code == '\r') {
		p_owner->__cursor_x = 0;
		return;
	}
	if (code == '\n') {
		p_owner->__cursor_x = 0;
		// A cursor that would run off the end of the range stops at it, where nothing is drawn any more.
		y = (uint32_t)p_owner->__cursor_y + p_font->line_height;
		p_owner->__cursor_y = (y > 0xFFFF) ? 0xFFFF : (uint16_t)y;
		return;
	}

	x = p_owner->__cursor_x;
	y = p_owner->__cursor_y;
	found = (*(p_font->fn_get_glyph ? p_font->fn_get_glyph : _getGlyph))(p_font, code, &glyph);
	advance = found ? glyph.advance : _missingWidth(p_font);

	// The whole cell of the character is filled first, so that a glyph covers what was there before whatever
	// shape it has. Transparent text skips this and lies over what is there.
	if (p_owner->__bt.text_opaque && advance) {
		_fillRect(p_owner, (int32_t)x, (int32_t)y, (int32_t)x + advance - 1, (int32_t)y + p_font->line_height - 1,
				p_owner->__text_back_color);
	}

	if (!found) {
		// A code the font does not hold shows as a filled block, so that it is not missed.
		_fillRect(p_owner, (int32_t)x, (int32_t)y, (int32_t)x + advance - 2,
				(int32_t)y + p_font->line_height - 1, color);
	}
	else if (glyph.p_data && glyph.width && glyph.height) {
		pages = (uint8_t)((glyph.height + 7) / 8);
		for (col = 0; col < glyph.width; col++) {
			_drawGlyphColumn(p_owner, (int32_t)x + glyph.x_offset + col, (int32_t)y + glyph.y_offset,
					glyph.p_data + (uint32_t)col * pages, glyph.height, color);
		}
	}

	next = x + advance;
	p_owner->__cursor_x = (next > 0xFFFF) ? 0xFFFF : (uint16_t)next;
}

void pifGraphics_DrawChar(PifGraphics *p_owner, char c, PifColor color)
{
	pifGraphics_DrawCode(p_owner, (uint8_t)c, color);
}

void pifGraphics_DrawString(PifGraphics *p_owner, const char *p_string, PifColor color)
{
	const PifFont *p_font;
	PifFontGlyph glyph;
	uint32_t code;
	uint8_t advance;

	if (!_checkDraw(p_owner) || !p_string) return;

	p_font = p_owner->__p_font;
	if (!p_font) return;

	while (*p_string) {
		p_string = _decodeUtf8(p_string, &code);
		// A glyph that does not fit in what is left of the line goes to the start of the next one.
		if (p_owner->__bt.text_wrap && code != '\n' && code != '\r') {
			advance = (*(p_font->fn_get_glyph ? p_font->fn_get_glyph : _getGlyph))(p_font, code, &glyph) ?
					glyph.advance : _missingWidth(p_font);
			if ((uint32_t)p_owner->__cursor_x + advance > p_owner->_width) {
				pifGraphics_DrawCode(p_owner, '\n', color);
			}
		}
		pifGraphics_DrawCode(p_owner, code, color);
	}
}

void pifGraphics_Printf(PifGraphics *p_owner, PifColor color, const char *p_format, ...)
{
	va_list data;
	char buffer[PIF_GRAPHICS_PRINTF_SIZE];

	if (!_checkDraw(p_owner) || !p_owner->__p_font) return;

	va_start(data, p_format);
	pif_PrintFormat(buffer, sizeof(buffer), &data, p_format);
	va_end(data);

	pifGraphics_DrawString(p_owner, buffer, color);
}
