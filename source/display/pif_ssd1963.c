#include "core/pif_log.h"
#include "display/pif_ssd1963.h"


static void _setAddress(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint32_t data[4];

	data[0] = x1 >> 8;
	data[1] = x1 & 0xFF;
	data[2] = x2 >> 8;
	data[3] = x2 & 0xFF;
	(*p_parent->__act_write_cmd)(SSD1963_CMD_SET_COL_ADDR, data, 4);

	data[0] = y1 >> 8;
	data[1] = y1 & 0xFF;
	data[2] = y2 >> 8;
	data[3] = y2 & 0xFF;
	(*p_parent->__act_write_cmd)(SSD1963_CMD_SET_PAGE_ADDR, data, 4);

	(*p_parent->__act_write_cmd)(SSD1963_CMD_WRITE_MEM_START, NULL, 0);
}

static uint8_t _convertColor(uint32_t color, uint32_t* p_data)
{
	(void)color;
	(void)p_data;
	return 0;
}

static uint8_t _convertColor8bit(uint32_t color, uint32_t* p_data)
{
	p_data[0] = (color & 0xFF0000) >> 16;
	p_data[1] = (color & 0x00FF00) >> 8;
	p_data[2] = color & 0x0000FF;
	return 3;
}

static uint8_t _convertColor12bit(uint32_t color, uint32_t* p_data)
{
	p_data[0] = (color & 0xFFF000) >> 12;
	p_data[1] = color & 0x000FFF;
	return 2;
}

static uint8_t _convertColor16bitPacked(uint32_t color, uint32_t* p_data)
{
	p_data[0] = (color & 0xFFFF00) >> 8;
	p_data[1] = ((color & 0x0000FF) << 8) | ((color & 0xFF0000) >> 16);
	p_data[2] = color & 0x00FFFF;
	return 3;
}

static uint8_t _convertColor16bit565(uint32_t color, uint32_t* p_data)
{
	p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
	return 1;
}

static uint8_t _convertColor18bit(uint32_t color, uint32_t* p_data)
{
	p_data[0] = ((color & 0xFC0000) >> 6) | ((color & 0x00FC00) >> 4) | ((color & 0x0000FC) >> 2);
	return 1;
}

static uint8_t _convertColor24bit(uint32_t color, uint32_t* p_data)
{
	p_data[0] = color;
	return 1;
}

static uint8_t _convertColor9bit(uint32_t color, uint32_t* p_data)
{
	p_data[0] = ((color & 0xFC0000) >> 15) | ((color & 0x00E000) >> 13);
	p_data[1] = ((color & 0x001C00) >> 4) | ((color & 0x0000FC) >> 2);
	return 2;
}

static void _setFunction(PifSsd1963* p_owner)
{
	switch (p_owner->__pixel_format) {
	case SSD1963_PF_8BIT:
		p_owner->__fn_convert_color = _convertColor8bit;
		break;

	case SSD1963_PF_12BIT:
		p_owner->__fn_convert_color = _convertColor12bit;
		break;

	case SSD1963_PF_16BIT_PACKED:
		p_owner->__fn_convert_color = _convertColor16bitPacked;
		break;

	case SSD1963_PF_16BIT_565:
		p_owner->__fn_convert_color = _convertColor16bit565;
		break;

	case SSD1963_PF_18BIT:
		p_owner->__fn_convert_color = _convertColor18bit;
		break;

	case SSD1963_PF_24BIT:
		p_owner->__fn_convert_color = _convertColor24bit;
		break;

	case SSD1963_PF_9BIT:
		p_owner->__fn_convert_color = _convertColor9bit;
		break;

	default:
		p_owner->__fn_convert_color = _convertColor;
		break;
	}
}

BOOL pifSsd1963_Init(PifSsd1963* p_owner, PifId id)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSsd1963));

	pifTftLcd_Init(&p_owner->parent, id, SSD1963_WIDTH, SSD1963_HEIGHT, TLR_0_DEGREE);
	p_parent->_fn_set_rotation = pifSsd1963_SetRotation;
	p_parent->_fn_draw_pixel = pifSsd1963_DrawPixel;
	p_parent->_fn_draw_hor_line = pifSsd1963_DrawHorLine;
	p_parent->_fn_draw_ver_line = pifSsd1963_DrawVerLine;
	p_parent->_fn_draw_fill_rect = pifSsd1963_DrawFillRect;

	p_owner->_view_size = SSD1963_WIDTH > SSD1963_HEIGHT ? SSD1963_WIDTH : SSD1963_HEIGHT;

	_setFunction(p_owner);
    return TRUE;
}

BOOL pifSsd1963_AttachActParallel(PifSsd1963* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;

	if (!p_owner || !act_reset || !act_chip_select || !act_read_cmd || !act_write_cmd || !act_write_data || !act_write_repeat) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    p_parent->__act_reset = act_reset;
    p_parent->__act_chip_select = act_chip_select;
    p_parent->__act_read_cmd = act_read_cmd;
    p_parent->__act_write_cmd = act_write_cmd;
    p_parent->__act_write_data = act_write_data;
    p_parent->__act_write_repeat = act_write_repeat;
    return TRUE;
}

void pifSsd1963_Setup(PifSsd1963* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;
	uint8_t* p, cmd, len, i;
	uint32_t data[10];

	p_owner->__p_rotation = p_rotation;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Rot: %X, %X, %X, %X", p_owner->__p_rotation[0], p_owner->__p_rotation[1], p_owner->__p_rotation[2], p_owner->__p_rotation[3]);
#endif

	p = p_setup;
	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd != TFT_SETUP_DELAY_MS) {
			switch (cmd) {
			case SSD1963_CMD_SET_PIX_DATA_IF:
				p_owner->__pixel_format = p[0];
				break;
			}
			p += len;
		}
	}
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Pixel Format: %u", p_owner->__pixel_format);
#endif

	_setFunction(p_owner);

    (*p_parent->__act_reset)();

	(*p_parent->__act_chip_select)(TRUE);

	for (i = 0; i < 3; i++)	(*p_parent->__act_write_cmd)(SSD1963_CMD_NOP, NULL, 0);

	p = p_setup;
	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd == TFT_SETUP_DELAY_MS) {
			pif_Delay1ms(len);
		}
		else {
			for (i = 0; i < len; i++) data[i] = p[i];
			(*p_parent->__act_write_cmd)(cmd, data, len);
			p += len;
		}
	}

    (*p_parent->__act_write_cmd)(SSD1963_CMD_SET_DISP_ON, NULL, 0);

	(*p_parent->__act_write_cmd)(SSD1963_CMD_WRITE_MEM_START, NULL, 0);

	(*p_parent->__act_chip_select)(FALSE);
}

void pifSsd1963_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;

	if (rotation & 1) {
		p_parent->_width = SSD1963_HEIGHT;
		p_parent->_height = SSD1963_WIDTH;
	}
	else {
		p_parent->_width = SSD1963_WIDTH;
		p_parent->_height = SSD1963_HEIGHT;
	}
	p_parent->_rotation = rotation;

	(*p_parent->__act_chip_select)(TRUE);
    (*p_parent->__act_write_cmd)(SSD1963_CMD_SET_ADDR_MODE, &p_owner->__p_rotation[rotation], 1);
	(*p_parent->__act_chip_select)(FALSE);

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Rot: %d, %X, %X, %X, %X", rotation, p_owner->__p_rotation[0], p_owner->__p_rotation[1], p_owner->__p_rotation[2], p_owner->__p_rotation[3]);
#endif
}

void pifSsd1963_DrawPixel(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint32_t color)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;
	uint8_t size;
	uint32_t data[3];

	size = (*p_owner->__fn_convert_color)(color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y);
	(*p_parent->__act_write_data)(data, size);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifSsd1963_DrawHorLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;
	uint8_t size;
	uint32_t data[3];

	size = (*p_owner->__fn_convert_color)(color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x + len, y);
	(*p_parent->__act_write_repeat)(data, size, len);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifSsd1963_DrawVerLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;
	uint8_t size;
	uint32_t data[3];

	size = (*p_owner->__fn_convert_color)(color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y + len);
	(*p_parent->__act_write_repeat)(data, size, len);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifSsd1963_DrawFillRect(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;
	uint8_t size;
	uint16_t i, lx, ly;
	uint32_t area, data[3];

	lx = x2 - x1 + 1;
	ly = y2 - y1 + 1;
	area = lx * ly;

	size = (*p_owner->__fn_convert_color)(color, data);

	if (area < p_owner->_view_size) {
		(*p_parent->__act_chip_select)(TRUE);
		_setAddress(p_owner, x1, y1, x2, y2);
		for (i = 0; i < ly; i++) {
			(*p_parent->__act_write_repeat)(data, size, lx);
		}
		(*p_parent->__act_chip_select)(FALSE);
	}
	else {
		if (lx > ly) {
			for (i = y1; i < y1 + ly; i++) {
				(*p_parent->__act_chip_select)(TRUE);
				_setAddress(p_owner, x1, i, x2, i);
				(*p_parent->__act_write_repeat)(data, size, lx);
				(*p_parent->__act_chip_select)(FALSE);
				pifTaskManager_Yield();
			}
		}
		else {
			for (i = x1; i < x1 + lx; i++) {
				(*p_parent->__act_chip_select)(TRUE);
				_setAddress(p_owner, i, y1, i, y2);
				(*p_parent->__act_write_repeat)(data, size, ly);
				(*p_parent->__act_chip_select)(FALSE);
				pifTaskManager_Yield();
			}
		}
	}
}
