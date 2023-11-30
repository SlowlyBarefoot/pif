#include "core/pif_log.h"
#include "display/pif_ili9341.h"


static void _convertCmd(PifIli9341Interface interface, uint8_t* p_src, uint32_t* p_dst, uint8_t len)
{
	uint8_t i;

	switch (interface) {
	case ILI9341_IF_MCU_8BIT_II:
	case ILI9341_IF_MCU_9BIT_II:
		for (i = 0; i < len; i++) p_dst[i] = (uint32_t)p_src[i] << 10;
		break;

	case ILI9341_IF_MCU_16BIT_II:
	case ILI9341_IF_MCU_18BIT_II:
		for (i = 0; i < len; i++) p_dst[i] = (uint32_t)p_src[i] << 1;
		break;

	default:
		for (i = 0; i < len; i++) p_dst[i] = (uint32_t)p_src[i];
		break;
	}
}

static void _setAddress(PifIli9341* p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;
	uint8_t src[4];
	uint32_t dst[4];

	src[0] = x1 >> 8;
	src[1] = x1 & 0xFF;
	src[2] = x2 >> 8;
	src[3] = x2 & 0xFF;
	_convertCmd(p_owner->__interface, src, dst, 4);
	(*p_parent->__act_write_cmd)(ILI9341_CMD_COL_ADDR_SET, dst, 4);

	src[0] = y1 >> 8;
	src[1] = y1 & 0xFF;
	src[2] = y2 >> 8;
	src[3] = y2 & 0xFF;
	_convertCmd(p_owner->__interface, src, dst, 4);
	(*p_parent->__act_write_cmd)(ILI9341_CMD_PAGE_ADDR_SET, dst, 4);

	(*p_parent->__act_write_cmd)(ILI9341_CMD_MEM_WRITE, NULL, 0);
}

static uint8_t _convertColor(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	(void)p_owner;
	(void)color;
	(void)p_data;
	return 0;
}

static uint8_t _convertColorMcuI_8bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 16) | ((color & 0x00E000) >> 13);
		p_data[1] = ((color & 0x001C00) >> 5) | ((color & 0x0000F8) >> 3);
		return 2;
	}
	else {
		p_data[0] = (color & 0xFC0000) >> 16;
		p_data[1] = (color & 0x00FC00) >> 8;
		p_data[2] = color & 0x0000FC;
		return 3;
	}
}

static uint8_t _convertColorMcuI_16bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
		return 1;
	}
	else {
		switch (p_owner->__mdt) {
		case 0:
			p_data[0] = (color & 0xFCFC00) >> 8;
			p_data[1] = ((color & 0x0000FC) << 8) | ((color & 0xFC0000) >> 16);
			p_data[2] = color & 0x00FCFC;
			return 3;

		case 1:
			p_data[0] = (color & 0xFCFC00) >> 8;
			p_data[1] = (color & 0x0000FC) << 8;
			return 2;

		case 2:
			p_data[0] = ((color & 0xFC0000) >> 8) | ((color & 0x00FC00) >> 6) | ((color & 0x0000F0) >> 4);
			p_data[1] = (color & 0x000003) << 14;
			return 2;

		case 3:
			p_data[0] = (color & 0xC00000) >> 22;
			p_data[1] = ((color & 0x3C0000) >> 6) | ((color & 0x00FC00) >> 4) | ((color & 0x0000FC) >> 2);
			return 2;
		}
	}
	return 0;
}

static uint8_t _convertColorMcuI_9bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 16) | ((color & 0x00E000) >> 13);
		p_data[1] = ((color & 0x001C00) >> 5) | ((color & 0x0000F8) >> 3);
		return 2;
	}
	else {
		switch (p_owner->__mdt) {
		case 0:
			p_data[0] = ((color & 0xFC0000) >> 15) | ((color & 0x00E000) >> 13);
			p_data[1] = ((color & 0x001C00) >> 4) | ((color & 0x0000FC) >> 2);
			return 2;

		case 1:
			p_data[0] = (color & 0xFC0000) >> 16;
			p_data[1] = (color & 0x00FC00) >> 8;
			p_data[2] = color & 0x0000FC;
			return 3;
		}
	}
	return 0;
}

static uint8_t _convertColorMcuI_18bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
		return 1;
	}
	else {
		p_data[0] = ((color & 0xFC0000) >> 6) | ((color & 0x00FC00) >> 5) | ((color & 0x0000FC) >> 3);
		return 1;
	}
}

static uint8_t _convertColorMcuII_8bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 6) | ((color & 0x00E000) >> 3);
		p_data[1] = ((color & 0x001C00) << 5) | ((color & 0x0000F8) << 7);
		return 2;
	}
	else {
		p_data[0] = (color & 0xFC0000) >> 6;
		p_data[1] = (color & 0x00FC00) << 2;
		p_data[2] = (color & 0x0000FC) << 10;
		return 3;
	}
}

static uint8_t _convertColorMcuII_16bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 6) | ((color & 0x00FC00) >> 4) | ((color & 0x0000F8) >> 2);
		return 1;
	}
	else {
		switch (p_owner->__mdt) {
		case 0:
			p_data[0] = (color & 0xFCFC00) >> 7;
			p_data[1] = ((color & 0x0000FC) << 7) | ((color & 0xFC0000) >> 15);
			p_data[2] = (color & 0x00FCFC) << 1;
			return 3;

		case 1:
			p_data[0] = (color & 0xFCFC00) >> 7;
			p_data[1] = (color & 0x0000FC) << 9;
			return 2;

		case 2:
			p_data[0] = ((color & 0xFC0000) >> 7) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F0) >> 3);
			p_data[1] = (color & 0x000003) << 15;
			return 2;

		case 3:
			p_data[0] = (color & 0xC00000) >> 21;
			p_data[1] = ((color & 0x3C0000) >> 5) | ((color & 0x00FC00) >> 3) | ((color & 0x0000FC) >> 1);
			return 2;
		}
	}
	return 0;
}

static uint8_t _convertColorMcuII_9bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 16) | ((color & 0x00E000) >> 13);
		p_data[1] = ((color & 0x001C00) >> 5) | ((color & 0x0000F8) >> 3);
		return 2;
	}
	else {
		switch (p_owner->__mdt) {
		case 0:
			p_data[0] = ((color & 0xFC0000) >> 6) | ((color & 0x00E000) >> 4);
			p_data[1] = ((color & 0x001C00) << 5) | ((color & 0x0000FC) << 7);
			return 2;

		case 1:
			p_data[0] = (color & 0xFC0000) >> 7;
			p_data[1] = (color & 0x00FC00) >> 1;
			p_data[2] = (color & 0x0000FC) << 11;
			return 3;
		}
	}
	return 0;
}

static uint8_t _convertColorMcuII_18bit(PifIli9341* p_owner, uint32_t color, uint32_t* p_data)
{
	if (p_owner->__pixel_format == ILI9341_PF_16BIT) {
		p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
		return 1;
	}
	else {
		p_data[0] = ((color & 0xFC0000) >> 6) | ((color & 0x00FC00) >> 5) | ((color & 0x0000FC) >> 3);
		return 1;
	}
}

static void _setFunction(PifIli9341* p_owner)
{
	switch (p_owner->__interface) {
	case ILI9341_IF_MCU_8BIT_I:
		p_owner->__fn_convert_color = _convertColorMcuI_8bit;
		break;

	case ILI9341_IF_MCU_16BIT_I:
		p_owner->__fn_convert_color = _convertColorMcuI_16bit;
		break;

	case ILI9341_IF_MCU_9BIT_I:
		p_owner->__fn_convert_color = _convertColorMcuI_9bit;
		break;

	case ILI9341_IF_MCU_18BIT_I:
		p_owner->__fn_convert_color = _convertColorMcuI_18bit;
		break;

	case ILI9341_IF_MCU_8BIT_II:
		p_owner->__fn_convert_color = _convertColorMcuII_8bit;
		break;

	case ILI9341_IF_MCU_16BIT_II:
		p_owner->__fn_convert_color = _convertColorMcuII_16bit;
		break;

	case ILI9341_IF_MCU_9BIT_II:
		p_owner->__fn_convert_color = _convertColorMcuII_9bit;
		break;

	case ILI9341_IF_MCU_18BIT_II:
		p_owner->__fn_convert_color = _convertColorMcuII_18bit;
		break;

	default:
		p_owner->__fn_convert_color = _convertColor;
		break;
	}
}

BOOL pifIli9341_Init(PifIli9341* p_owner, PifId id, PifIli9341Interface interface)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifIli9341));

	pifTftLcd_Init(&p_owner->parent, id, ILI9341_WIDTH, ILI9341_HEIGHT, TLR_0_DEGREE);
	p_parent->_fn_set_rotation = pifIli9341_SetRotation;
	p_parent->_fn_draw_pixel = pifIli9341_DrawPixel;
	p_parent->_fn_draw_hor_line = pifIli9341_DrawHorLine;
	p_parent->_fn_draw_ver_line = pifIli9341_DrawVerLine;
	p_parent->_fn_draw_fill_rect = pifIli9341_DrawFillRect;

	p_owner->__interface = interface;
	p_owner->_view_size = ILI9341_WIDTH > ILI9341_HEIGHT ? ILI9341_WIDTH : ILI9341_HEIGHT;

	_setFunction(p_owner);
    return TRUE;
}

BOOL pifIli9341_AttachActParallel(PifIli9341* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;

	if (!p_owner || !act_reset || !act_chip_select || !act_write_cmd || !act_write_data || !act_write_repeat) {
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

void pifIli9341_Setup(PifIli9341* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;
	int i;
	uint8_t* p, cmd, len;
	uint32_t data[16];

	p_owner->__p_rotation = p_rotation;
#ifndef PIF_NO_LOG
	if (p_rotation) {
		pifLog_Printf(LT_INFO, "Rot: %X, %X, %X, %X", p_rotation[0], p_rotation[1], p_rotation[2], p_rotation[3]);
	}
#endif

	p = (uint8_t*)p_setup;
	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd != TFT_SETUP_DELAY_MS) {
			switch (cmd) {
			case ILI9341_CMD_PIXEL_FORMAT_SET:
				p_owner->__pixel_format = p[0] >> 4;
				break;

			case ILI9341_CMD_INTERFACE_CTRL:
				p_owner->__mdt = p[1] & 3;
				break;
			}
			p += len;
		}
	}

	_setFunction(p_owner);

    (*p_parent->__act_reset)();

	(*p_parent->__act_chip_select)(TRUE);

	for (i = 0; i < 3; i++)	(*p_parent->__act_write_cmd)(ILI9341_CMD_NOP, NULL, 0);

	if (p_parent->__act_read_cmd) {
		(*p_parent->__act_read_cmd)(ILI9341_CMD_READ_DISP_ID_INFO, data, 4);
		p_owner->_ic_id_04 = (data[1] << 16) + (data[2] << 8) + data[3];
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "ID(%Xh): %Xh", ILI9341_CMD_READ_DISP_ID_INFO, p_owner->_ic_id_04);
#endif

		(*p_parent->__act_read_cmd)(ILI9341_CMD_READ_ID_4, data, 4);
		p_owner->_ic_id_D3 = (data[1] << 16) + (data[2] << 8) + data[3];
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "ID(%Xh): %Xh", ILI9341_CMD_READ_ID_4, p_owner->_ic_id_D3);
#endif
	}

	p = (uint8_t*)p_setup;
	while (*p) {
		cmd = *p++;
		len = *p++;
		if (cmd == TFT_SETUP_DELAY_MS) {
			pif_Delay1ms(len);
		}
		else {
			_convertCmd(p_owner->__interface, p, data, len);
			(*p_parent->__act_write_cmd)(cmd, data, len);
			p += len;
		}
	}

    (*p_parent->__act_write_cmd)(ILI9341_CMD_SLEEP_OUT, NULL, 0);
    pif_Delay1ms(120);

    (*p_parent->__act_write_cmd)(ILI9341_CMD_DISP_ON, NULL, 0);
    (*p_parent->__act_write_cmd)(ILI9341_CMD_MEM_WRITE, NULL, 0);

	(*p_parent->__act_chip_select)(FALSE);
}

BOOL pifIli9341_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint32_t data[1];

	if (!p_owner->__p_rotation) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	if (rotation & 1) {
		p_parent->_width = ILI9341_HEIGHT;
		p_parent->_height = ILI9341_WIDTH;
	}
	else {
		p_parent->_width = ILI9341_WIDTH;
		p_parent->_height = ILI9341_HEIGHT;
	}
	p_parent->_rotation = rotation;

	(*p_parent->__act_chip_select)(TRUE);
	_convertCmd(p_owner->__interface, (uint8_t*)(p_owner->__p_rotation + rotation), data, 1);
    (*p_parent->__act_write_cmd)(ILI9341_CMD_MEM_ACCESS_CTRL, data, 1);
	(*p_parent->__act_chip_select)(FALSE);

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Rot: %d -> %X", rotation, p_owner->__p_rotation[rotation]);
#endif
	return TRUE;
}

void pifIli9341_DrawPixel(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint32_t data[2];

	size = (*p_owner->__fn_convert_color)(p_owner, color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y);
	(*p_parent->__act_write_data)(data, size);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifIli9341_DrawHorLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint32_t data[2];

	size = (*p_owner->__fn_convert_color)(p_owner, color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x + len, y);
	(*p_parent->__act_write_repeat)(data, size, len);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifIli9341_DrawVerLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint32_t data[2];

	size = (*p_owner->__fn_convert_color)(p_owner, color, data);

	(*p_parent->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y + len);
	(*p_parent->__act_write_repeat)(data, size, len);
	(*p_parent->__act_chip_select)(FALSE);
}

void pifIli9341_DrawFillRect(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint16_t i, lx, ly;
	uint32_t area, data[2];

	lx = x2 - x1 + 1;
	ly = y2 - y1 + 1;
	area = lx * ly;

	size = (*p_owner->__fn_convert_color)(p_owner, color, data);

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
