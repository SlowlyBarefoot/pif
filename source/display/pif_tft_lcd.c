// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_tft_lcd.h"


// Parameters one entry of a setup stream, or one command, may carry.
#define MAX_PARAMS			16
// Pixels converted and sent at once by pifTftLcd_DrawArea.
#define DRAW_AREA_CHUNK		16


BOOL pifTftLcd_Init(PifTftLcd *p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation)
{
	if (!p_owner || rotation > TLR_270_DEGREE) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTftLcd));

	if (rotation & 1) {
		if (!pifGraphics_Init(&p_owner->parent, id, height, width)) return FALSE;
	}
	else {
		if (!pifGraphics_Init(&p_owner->parent, id, width, height)) return FALSE;
	}
	p_owner->_rotation = rotation;

	// The drawing is the same for every controller on an MCU bus, so the surface gets it from here.
	p_owner->parent._fn_draw_pixel = pifTftLcd_DrawPixel;
	p_owner->parent._fn_draw_hor_line = pifTftLcd_DrawHorLine;
	p_owner->parent._fn_draw_ver_line = pifTftLcd_DrawVerLine;
	p_owner->parent._fn_draw_fill_rect = pifTftLcd_DrawFillRect;
    return TRUE;
}

void pifTftLcd_SetCommandShift(PifTftLcd *p_owner, uint8_t shift)
{
	if (!p_owner) return;

	p_owner->__command_shift = shift;
}

BOOL pifTftLcd_SetRotation(PifTftLcd *p_owner, PifTftLcdRotation rotation)
{
	if (!p_owner || rotation > TLR_270_DEGREE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->_fn_set_rotation) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}
	return (*p_owner->_fn_set_rotation)(p_owner, rotation);
}

BOOL pifTftLcd_SetBackLight(PifTftLcd *p_owner, uint8_t level)
{
	if (!p_owner || !p_owner->act_backlight) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	(*p_owner->act_backlight)(level);
	return TRUE;
}

void pifTftLcd_WriteCmd(PifTftLcd *p_owner, PifTftLcdCmd cmd, const uint8_t *p_param, uint8_t len)
{
	uint32_t data[MAX_PARAMS];
	uint8_t i;

	if (len > MAX_PARAMS) return;

	// An interface that carries the bus in its high bits wants the parameters up there as well.
	for (i = 0; i < len; i++) data[i] = (uint32_t)p_param[i] << p_owner->__command_shift;
	(*p_owner->__act_write_cmd)(cmd << p_owner->__command_shift, len ? data : NULL, len);
}

BOOL pifTftLcd_ReadCmd(PifTftLcd *p_owner, PifTftLcdCmd cmd, uint32_t *p_data, uint8_t len)
{
	uint8_t i;

	if (!p_owner->__act_read_cmd) return FALSE;

	(*p_owner->__act_read_cmd)(cmd << p_owner->__command_shift, p_data, len);
	// The answer comes back on the lines the parameters go out on, so it moves back down with them.
	for (i = 0; i < len; i++) p_data[i] = (p_data[i] >> p_owner->__command_shift) & 0xFF;
	return TRUE;
}

BOOL pifTftLcd_ScanSetup(PifTftLcd *p_owner, const uint8_t *p_setup, PifTftLcdSetupPeek peek)
{
	const uint8_t *p = p_setup;
	uint8_t cmd, len;

	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd == TFT_SETUP_DELAY_MS) continue;

		if (len > MAX_PARAMS) {
			pif_error = E_WRONG_DATA;
			return FALSE;
		}
		if (peek) (*peek)(p_owner, cmd, p, len);
		p += len;
	}
	return TRUE;
}

void pifTftLcd_RunSetup(PifTftLcd *p_owner, const uint8_t *p_setup)
{
	const uint8_t *p = p_setup;
	uint8_t cmd, len;

	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd == TFT_SETUP_DELAY_MS) {
			pif_Delay1ms(len);
		}
		else {
			pifTftLcd_WriteCmd(p_owner, cmd, p, len);
			p += len;
		}
	}
}

void pifTftLcd_SetAddressDcs(PifTftLcd *p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint8_t param[4];

	param[0] = x1 >> 8;
	param[1] = x1 & 0xFF;
	param[2] = x2 >> 8;
	param[3] = x2 & 0xFF;
	pifTftLcd_WriteCmd(p_owner, PIF_TFT_LCD_CMD_COL_ADDR, param, 4);

	param[0] = y1 >> 8;
	param[1] = y1 & 0xFF;
	param[2] = y2 >> 8;
	param[3] = y2 & 0xFF;
	pifTftLcd_WriteCmd(p_owner, PIF_TFT_LCD_CMD_PAGE_ADDR, param, 4);

	pifTftLcd_WriteCmd(p_owner, PIF_TFT_LCD_CMD_MEM_WRITE, NULL, 0);
}

void pifTftLcd_DrawPixel(PifGraphics *p_parent, uint16_t x, uint16_t y, PifColor color)
{
	PifTftLcd *p_owner = (PifTftLcd *)p_parent;
	uint8_t size;
	uint32_t data[PIF_TFT_LCD_MAX_COLOR_WORDS];

	if (x >= p_parent->_width || y >= p_parent->_height) return;

	size = (*p_owner->_fn_convert_color)(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	(*p_owner->_fn_set_address)(p_owner, x, y, x, y);
	(*p_owner->__act_write_data)(data, size);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifTftLcd_DrawHorLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	PifTftLcd *p_owner = (PifTftLcd *)p_parent;
	uint8_t size;
	uint32_t data[PIF_TFT_LCD_MAX_COLOR_WORDS];

	if (!len || x >= p_parent->_width || y >= p_parent->_height) return;

	if (len > p_parent->_width - x) len = p_parent->_width - x;

	size = (*p_owner->_fn_convert_color)(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	(*p_owner->_fn_set_address)(p_owner, x, y, x + len - 1, y);
	(*p_owner->__act_write_repeat)(data, size, len);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifTftLcd_DrawVerLine(PifGraphics *p_parent, uint16_t x, uint16_t y, uint16_t len, PifColor color)
{
	PifTftLcd *p_owner = (PifTftLcd *)p_parent;
	uint8_t size;
	uint32_t data[PIF_TFT_LCD_MAX_COLOR_WORDS];

	if (!len || x >= p_parent->_width || y >= p_parent->_height) return;

	if (len > p_parent->_height - y) len = p_parent->_height - y;

	size = (*p_owner->_fn_convert_color)(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	(*p_owner->_fn_set_address)(p_owner, x, y, x, y + len - 1);
	(*p_owner->__act_write_repeat)(data, size, len);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifTftLcd_DrawFillRect(PifGraphics *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor color)
{
	PifTftLcd *p_owner = (PifTftLcd *)p_parent;
	uint8_t size;
	uint32_t data[PIF_TFT_LCD_MAX_COLOR_WORDS];

	if (!pifGraphics_ClipArea(p_parent, &x1, &y1, &x2, &y2)) return;

	size = (*p_owner->_fn_convert_color)(p_owner, color, data);

	// One address window and one repeat for the whole rectangle; the controller wraps to the next
	// line by itself.
	(*p_owner->__act_chip_select)(TRUE);
	(*p_owner->_fn_set_address)(p_owner, x1, y1, x2, y2);
	(*p_owner->__act_write_repeat)(data, size, (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1));
	(*p_owner->__act_chip_select)(FALSE);
}

void pifTftLcd_DrawArea(PifGraphics *p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, PifColor *p_color_map)
{
	PifTftLcd *p_owner = (PifTftLcd *)p_parent;
	uint32_t i, count, size;
	uint32_t data[DRAW_AREA_CHUNK * PIF_TFT_LCD_MAX_COLOR_WORDS];

	// The color map has no stride of its own, so an area that does not fit is not drawn rather than clipped.
	if (!p_color_map || x1 > x2 || y1 > y2 || x2 >= p_parent->_width || y2 >= p_parent->_height) return;

	count = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

	(*p_owner->__act_chip_select)(TRUE);
	(*p_owner->_fn_set_address)(p_owner, x1, y1, x2, y2);
	while (count) {
		size = 0;
		for (i = 0; i < DRAW_AREA_CHUNK && count; i++, count--) {
			size += (*p_owner->_fn_convert_color)(p_owner, *p_color_map++, data + size);
		}
		(*p_owner->__act_write_data)(data, size);
	}
	(*p_owner->__act_chip_select)(FALSE);
}
