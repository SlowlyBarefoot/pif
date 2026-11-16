// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_ssd1306.h"


// Control byte in front of every transfer: Co = 0, so the whole transfer is of one kind.
#define CTRL_CMD			0x00	// D/C# = 0, the bytes are commands
#define CTRL_DATA			0x40	// D/C# = 1, the bytes are pixels

#define DEFAULT_CONTRAST	0xCF	// What a GM009605 is bright and even at, with the internal charge pump

#define COM_PINS_SEQUENTIAL	0x02	// Panels of 16 or 32 rows
#define COM_PINS_ALTERNATIVE 0x12	// Panels of 64 rows, as the GM009605 has


/**
 * @fn _write
 * @brief Sends a block of commands or of pixels. The controller moves its own pointers on by itself, so a block
 *        longer than the port takes goes in several transfers, each with the control byte in front of it again.
 * @return TRUE on success, otherwise FALSE with pif_error set.
 */
static BOOL _write(PifSsd1306 *p_owner, uint8_t ctrl, const uint8_t *p_data, size_t size)
{
	uint16_t max = p_owner->_p_device->max_transfer_size;
	size_t len, pos;

	for (pos = 0; pos < size; pos += len) {
		len = size - pos;
		if (max && len > max) len = max;
		if (!pifDeviceReg_Write(p_owner->_p_device, ctrl, 1, (uint8_t *)p_data + pos, len)) return FALSE;
	}
	return TRUE;
}

static BOOL _writeCmd(PifSsd1306 *p_owner, const uint8_t *p_cmd, size_t size)
{
	return _write(p_owner, CTRL_CMD, p_cmd, size);
}

static BOOL _checkBegin(PifSsd1306 *p_owner)
{
	if (!p_owner || !p_owner->_p_device) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->__bt.begin) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	return TRUE;
}

// A page is clean while its first column lies past its last one.
static void _markClean(PifSsd1306 *p_owner)
{
	uint8_t page;

	for (page = 0; page < p_owner->_pages; page++) {
		p_owner->__dirty_first[page] = (uint8_t)p_owner->parent._width;
		p_owner->__dirty_last[page] = 0;
	}
}

static void _markAll(PifSsd1306 *p_owner)
{
	uint8_t page;

	for (page = 0; page < p_owner->_pages; page++) {
		p_owner->__dirty_first[page] = 0;
		p_owner->__dirty_last[page] = (uint8_t)p_owner->parent._width - 1;
	}
}

/**
 * @fn _applyMask
 * @brief Changes the pixels of one column of one page that mask selects, and marks the column for the next
 *        update when something did change. The caller makes sure x and page are on the panel.
 *
 * The panel has one bit per pixel, so a color is `SSD1306_COLOR_BLACK` to turn the pixels off,
 * `SSD1306_COLOR_INVERSE` to turn them round, and anything else to turn them on.
 */
static void _applyMask(PifSsd1306 *p_owner, uint8_t x, uint8_t page, uint8_t mask, PifColor color)
{
	uint16_t index = (uint16_t)page * p_owner->parent._width + x;
	uint8_t old = p_owner->__p_buffer[index];
	uint8_t now;

	if (color == SSD1306_COLOR_BLACK) now = old & ~mask;
	else if (color == SSD1306_COLOR_INVERSE) now = old ^ mask;
	else now = old | mask;
	if (now == old) return;

	p_owner->__p_buffer[index] = now;
	if (x < p_owner->__dirty_first[page]) p_owner->__dirty_first[page] = x;
	if (x > p_owner->__dirty_last[page]) p_owner->__dirty_last[page] = x;
}

// Mask of the rows from top to bottom of a page, both of them 0 to 7.
static uint8_t _pageMask(uint8_t top, uint8_t bottom)
{
	return (uint8_t)((0xFF << top) & (0xFF >> (7 - bottom)));
}

/*
 * The four functions the drawing surface puts its pixels through. They only touch the frame buffer, which
 * pifSsd1306_Update() sends on. The surface has clipped what it hands over to the panel already.
 */

static void _drawPixel(PifGraphics *p_parent, uint16_t x, uint16_t y, PifColor color)
{
	PifSsd1306 *p_owner = (PifSsd1306 *)p_parent;

	_applyMask(p_owner, (uint8_t)x, (uint8_t)(y >> 3), (uint8_t)(1 << (y & 7)), color);
}

static void _drawHorLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	PifSsd1306 *p_owner = (PifSsd1306 *)p_parent;
	uint8_t page = (uint8_t)(y >> 3);
	uint8_t mask = (uint8_t)(1 << (y & 7));
	uint16_t last = x + len - 1;

	for (; x <= last; x++) _applyMask(p_owner, (uint8_t)x, page, mask, color);
}

static void _drawVerLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	PifSsd1306 *p_owner = (PifSsd1306 *)p_parent;
	uint16_t last = y + len - 1;
	uint8_t first_page = (uint8_t)(y >> 3);
	uint8_t last_page = (uint8_t)(last >> 3);
	uint8_t page, top, bottom;

	for (page = first_page; page <= last_page; page++) {
		top = (page == first_page) ? (uint8_t)(y & 7) : 0;
		bottom = (page == last_page) ? (uint8_t)(last & 7) : 7;
		_applyMask(p_owner, (uint8_t)x, page, _pageMask(top, bottom), color);
	}
}

static void _drawFillRect(PifGraphics *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
	PifSsd1306 *p_owner = (PifSsd1306 *)p_parent;
	uint8_t first_page = (uint8_t)(y1 >> 3);
	uint8_t last_page = (uint8_t)(y2 >> 3);
	uint8_t page, top, bottom, mask;
	uint16_t x;

	for (page = first_page; page <= last_page; page++) {
		top = (page == first_page) ? (uint8_t)(y1 & 7) : 0;
		bottom = (page == last_page) ? (uint8_t)(y2 & 7) : 7;
		mask = _pageMask(top, bottom);
		for (x = x1; x <= x2; x++) _applyMask(p_owner, (uint8_t)x, page, mask, color);
	}
}

BOOL pifSsd1306_Init(PifSsd1306 *p_owner, PifId id, PifI2cPort *p_port, uint8_t addr, void *p_client,
		uint8_t width, uint8_t height)
{
	PifI2cDevice *p_device;

	if (!p_owner || !p_port || !width || width > SSD1306_WIDTH ||
			(height != 16 && height != 32 && height != SSD1306_HEIGHT)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSsd1306));

	if (!pifGraphics_Init(&p_owner->parent, id, width, height)) return FALSE;
	p_owner->parent._fn_draw_pixel = _drawPixel;
	p_owner->parent._fn_draw_hor_line = _drawHorLine;
	p_owner->parent._fn_draw_ver_line = _drawVerLine;
	p_owner->parent._fn_draw_fill_rect = _drawFillRect;
	p_owner->_pages = height / 8;

	p_owner->__p_buffer = calloc((size_t)width * p_owner->_pages, sizeof(uint8_t));
	if (!p_owner->__p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	p_device = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr, p_client);
	if (!p_device) goto fail;
	p_owner->_p_device = &p_device->parent;

	_markClean(p_owner);
	return TRUE;

fail:
	pifSsd1306_Clear(p_owner);
	return FALSE;
}

void pifSsd1306_Clear(PifSsd1306 *p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
		p_owner->_p_device = NULL;
	}
	if (p_owner->__p_buffer) {
		free(p_owner->__p_buffer);
		p_owner->__p_buffer = NULL;
	}
	// The surface has nowhere to put its pixels any more.
	p_owner->parent._fn_draw_pixel = NULL;
	p_owner->parent._fn_draw_hor_line = NULL;
	p_owner->parent._fn_draw_ver_line = NULL;
	p_owner->parent._fn_draw_fill_rect = NULL;
	p_owner->__bt.begin = FALSE;
	p_owner->__bt.display = FALSE;
}

BOOL pifSsd1306_Begin(PifSsd1306 *p_owner)
{
	uint8_t setup[25];
	uint8_t i = 0;

	if (!p_owner || !p_owner->_p_device || !p_owner->__p_buffer) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Begin may run again with the panel in any state, so nothing of the last call is kept.
	p_owner->__bt.begin = FALSE;
	p_owner->__bt.display = FALSE;
	p_owner->__bt.inverse = FALSE;
	p_owner->__bt.flip_hor = FALSE;
	p_owner->__bt.flip_ver = FALSE;

	// The display stays off until the frame memory has been cleared, so none of what it held at power up shows.
	setup[i++] = SSD1306_CMD_SET_DISP_OFF;
	setup[i++] = SSD1306_CMD_SET_DISP_CLOCK;
	setup[i++] = 0x80;											// Ratio 1, the frequency in the middle of the range
	setup[i++] = SSD1306_CMD_SET_MULTIPLEX;
	setup[i++] = (uint8_t)p_owner->parent._height - 1;
	setup[i++] = SSD1306_CMD_SET_DISP_OFFSET;
	setup[i++] = 0x00;
	setup[i++] = SSD1306_CMD_SET_START_LINE | 0x00;
	setup[i++] = SSD1306_CMD_SET_CHARGE_PUMP;
	setup[i++] = SSD1306_CHARGE_PUMP_ON;						// The module has no external VCC pin
	setup[i++] = SSD1306_CMD_SET_MEMORY_MODE;
	setup[i++] = SSD1306_MM_HORIZONTAL;							// What pifSsd1306_Update() sends its pages in
	setup[i++] = SSD1306_CMD_SET_SEG_REMAP_ON;					// Column 0 at the left and row 0 at the top of
	setup[i++] = SSD1306_CMD_SET_COM_SCAN_DEC;					// the module as it is built
	setup[i++] = SSD1306_CMD_SET_COM_PINS;
	setup[i++] = (p_owner->parent._height == SSD1306_HEIGHT) ? COM_PINS_ALTERNATIVE : COM_PINS_SEQUENTIAL;
	setup[i++] = SSD1306_CMD_SET_CONTRAST;
	setup[i++] = DEFAULT_CONTRAST;
	setup[i++] = SSD1306_CMD_SET_PRECHARGE;
	setup[i++] = 0xF1;											// Phase 1 of 1 clock, phase 2 of 15
	setup[i++] = SSD1306_CMD_SET_VCOMH_DESELECT;
	setup[i++] = 0x40;
	setup[i++] = SSD1306_CMD_DEACTIVATE_SCROLL;
	setup[i++] = SSD1306_CMD_ENTIRE_DISP_OFF;					// Shows the frame memory, not every pixel
	setup[i++] = SSD1306_CMD_SET_NORMAL_DISP;
	if (!_writeCmd(p_owner, setup, i)) return FALSE;

	// The updates below need the panel set up, so this comes before them and goes again if one fails.
	p_owner->__bt.begin = TRUE;

	memset(p_owner->__p_buffer, 0, (size_t)p_owner->parent._width * p_owner->_pages);
	pifGraphics_SetCursor(&p_owner->parent, 0, 0);
	_markAll(p_owner);
	if (!pifSsd1306_Update(p_owner)) goto fail;

	if (!pifSsd1306_Display(p_owner)) goto fail;
	pif_Delay1ms(PIF_SSD1306_BEGIN_DELAY_MS);
	return TRUE;

fail:
	p_owner->__bt.begin = FALSE;
	p_owner->__bt.display = FALSE;
	return FALSE;
}

BOOL pifSsd1306_Update(PifSsd1306 *p_owner)
{
	uint8_t cmd[3];
	uint8_t page, first, last;

	if (!_checkBegin(p_owner)) return FALSE;

	for (page = 0; page < p_owner->_pages; page++) {
		first = p_owner->__dirty_first[page];
		last = p_owner->__dirty_last[page];
		if (first > last) continue;

		cmd[0] = SSD1306_CMD_SET_PAGE_ADDR;
		cmd[1] = page;
		cmd[2] = page;
		if (!_writeCmd(p_owner, cmd, 3)) return FALSE;

		cmd[0] = SSD1306_CMD_SET_COL_ADDR;
		cmd[1] = first;
		cmd[2] = last;
		if (!_writeCmd(p_owner, cmd, 3)) return FALSE;

		if (!_write(p_owner, CTRL_DATA, p_owner->__p_buffer + (uint16_t)page * p_owner->parent._width + first,
				(size_t)(last - first) + 1)) return FALSE;

		// Only a page the panel has got is taken off the list, so another call sends the rest again.
		p_owner->__dirty_first[page] = (uint8_t)p_owner->parent._width;
		p_owner->__dirty_last[page] = 0;
	}
	return TRUE;
}

BOOL pifSsd1306_UpdateAll(PifSsd1306 *p_owner)
{
	if (!_checkBegin(p_owner)) return FALSE;

	_markAll(p_owner);
	return pifSsd1306_Update(p_owner);
}

BOOL pifSsd1306_GetPixel(PifSsd1306 *p_owner, uint16_t x, uint16_t y)
{
	if (!p_owner || !p_owner->__p_buffer || x >= p_owner->parent._width || y >= p_owner->parent._height) {
		return FALSE;
	}

	return (p_owner->__p_buffer[(uint16_t)(y >> 3) * p_owner->parent._width + x] & (1 << (y & 7))) ? TRUE : FALSE;
}

BOOL pifSsd1306_Display(PifSsd1306 *p_owner)
{
	uint8_t cmd = SSD1306_CMD_SET_DISP_ON;

	if (!_checkBegin(p_owner)) return FALSE;

	if (!_writeCmd(p_owner, &cmd, 1)) return FALSE;
	p_owner->__bt.display = TRUE;
	return TRUE;
}

BOOL pifSsd1306_NoDisplay(PifSsd1306 *p_owner)
{
	uint8_t cmd = SSD1306_CMD_SET_DISP_OFF;

	if (!_checkBegin(p_owner)) return FALSE;

	if (!_writeCmd(p_owner, &cmd, 1)) return FALSE;
	p_owner->__bt.display = FALSE;
	return TRUE;
}

BOOL pifSsd1306_SetContrast(PifSsd1306 *p_owner, uint8_t contrast)
{
	uint8_t cmd[2];

	if (!_checkBegin(p_owner)) return FALSE;

	cmd[0] = SSD1306_CMD_SET_CONTRAST;
	cmd[1] = contrast;
	return _writeCmd(p_owner, cmd, 2);
}

BOOL pifSsd1306_Inverse(PifSsd1306 *p_owner)
{
	uint8_t cmd = SSD1306_CMD_SET_INVERSE_DISP;

	if (!_checkBegin(p_owner)) return FALSE;

	if (!_writeCmd(p_owner, &cmd, 1)) return FALSE;
	p_owner->__bt.inverse = TRUE;
	return TRUE;
}

BOOL pifSsd1306_NoInverse(PifSsd1306 *p_owner)
{
	uint8_t cmd = SSD1306_CMD_SET_NORMAL_DISP;

	if (!_checkBegin(p_owner)) return FALSE;

	if (!_writeCmd(p_owner, &cmd, 1)) return FALSE;
	p_owner->__bt.inverse = FALSE;
	return TRUE;
}

BOOL pifSsd1306_SetFlip(PifSsd1306 *p_owner, BOOL flip_hor, BOOL flip_ver)
{
	uint8_t cmd[2];

	if (!_checkBegin(p_owner)) return FALSE;

	cmd[0] = flip_hor ? SSD1306_CMD_SET_SEG_REMAP_OFF : SSD1306_CMD_SET_SEG_REMAP_ON;
	cmd[1] = flip_ver ? SSD1306_CMD_SET_COM_SCAN_INC : SSD1306_CMD_SET_COM_SCAN_DEC;
	if (!_writeCmd(p_owner, cmd, 2)) return FALSE;
	p_owner->__bt.flip_hor = flip_hor ? 1 : 0;
	p_owner->__bt.flip_ver = flip_ver ? 1 : 0;
	return TRUE;
}
