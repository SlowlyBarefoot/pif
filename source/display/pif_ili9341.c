#include "core/pif_log.h"
#include "display/pif_ili9341.h"


static void _setAddress(PifIli9341* p_owner, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t data[4];

	data[0] = x1 >> 8;
	data[1] = x1 & 0xFF;
	data[2] = x2 >> 8;
	data[3] = x2 & 0xFF;
	(*p_owner->__act_write_cmd)(ILI9341_CMD_COL_ADDR_SET, data, 4);

	data[0] = y1 >> 8;
	data[1] = y1 & 0xFF;
	data[2] = y2 >> 8;
	data[3] = y2 & 0xFF;
	(*p_owner->__act_write_cmd)(ILI9341_CMD_PAGE_ADDR_SET, data, 4);

	(*p_owner->__act_write_cmd)(ILI9341_CMD_MEM_WRITE, NULL, 0);
}

static uint8_t _convertColor(PifIli9341* p_owner, uint32_t color, uint16_t* p_data)
{
	uint8_t size;

	switch (p_owner->_interface) {
	case ILI9341_IF_PARALLEL_8BIT:
		p_data[0] = ((color & 0xF80000) >> 16) | ((color & 0x00E000) >> 13);
		p_data[1] = ((color & 0x001C00) >> 5) | ((color & 0x0000F8) >> 3);
		size = 2;
		break;

	case ILI9341_IF_PARALLEL_16BIT:
		p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
		size = 1;
		break;
	}
	return size;
}

BOOL pifIli9341_Init(PifIli9341* p_owner, PifId id, PifIli9341Interface interface)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifIli9341));

	pifTftLcd_Init(&p_owner->parent, id, ILI9341_WIDTH, ILI9341_HEIGHT, TLR_0_DEGREE);
	p_owner->parent._fn_set_rotation = pifIli9341_SetRotation;
	p_owner->parent._fn_draw_pixel = pifIli9341_DrawPixel;
	p_owner->parent._fn_draw_hor_line = pifIli9341_DrawHorLine;
	p_owner->parent._fn_draw_ver_line = pifIli9341_DrawVerLine;
	p_owner->parent._fn_draw_fill_rect = pifIli9341_DrawFillRect;

	p_owner->_interface = interface;
	p_owner->_view_size = ILI9341_WIDTH > ILI9341_HEIGHT ? ILI9341_WIDTH : ILI9341_HEIGHT;
    return TRUE;
}

BOOL pifIli9341_AttachActParallel(PifIli9341* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat)
{
	int i;
	uint16_t data[4];

	if (!p_owner || !act_reset || !act_chip_select || !act_read_cmd || !act_write_cmd || !act_write_data || !act_write_repeat) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    p_owner->__act_reset = act_reset;
    p_owner->__act_chip_select = act_chip_select;
    p_owner->__act_read_cmd = act_read_cmd;
    p_owner->__act_write_cmd = act_write_cmd;
    p_owner->__act_write_data = act_write_data;
    p_owner->__act_write_repeat = act_write_repeat;

    (*act_reset)();

	(*p_owner->__act_chip_select)(TRUE);

	for (i = 0; i < 3; i++)	(*act_write_cmd)(ILI9341_CMD_NOP, NULL, 0);

	(*act_read_cmd)(ILI9341_CMD_READ_DISP_ID_INFO, data, 4);
	p_owner->_ic_id_04 = (data[1] << 16) + (data[2] << 8) + data[3];
	pifLog_Printf(LT_INFO, "ID(%Xh): %Xh", ILI9341_CMD_READ_DISP_ID_INFO, p_owner->_ic_id_04);

	(*act_read_cmd)(ILI9341_CMD_READ_ID_4, data, 4);
	p_owner->_ic_id_D3 = (data[1] << 16) + (data[2] << 8) + data[3];
	pifLog_Printf(LT_INFO, "ID(%Xh): %Xh", ILI9341_CMD_READ_ID_4, p_owner->_ic_id_D3);

	(*p_owner->__act_chip_select)(FALSE);
    return TRUE;
}

void pifIli9341_Setup(PifIli9341* p_owner, const uint16_t* p_setup, const uint16_t* p_rotation)
{
	uint16_t* p = p_setup, cmd, len;

	p_owner->__p_rotation = p_rotation;
	pifLog_Printf(LT_INFO, "Rot: %X, %X, %X, %X", p_owner->__p_rotation[0], p_owner->__p_rotation[1], p_owner->__p_rotation[2], p_owner->__p_rotation[3]);

	(*p_owner->__act_chip_select)(TRUE);

	while (*p) {
		cmd = *p++;
		len = *p++;
		(*p_owner->__act_write_cmd)(cmd, p, len);
		p += len;
	}

    (*p_owner->__act_write_cmd)(ILI9341_CMD_SLEEP_OUT, NULL, 0);
    pif_Delay1ms(120);

    (*p_owner->__act_write_cmd)(ILI9341_CMD_DISP_ON, NULL, 0);
    (*p_owner->__act_write_cmd)(ILI9341_CMD_MEM_WRITE, NULL, 0);

	(*p_owner->__act_chip_select)(FALSE);
}

void pifIli9341_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint16_t val;

	if (rotation & 1) {
		p_owner->parent._width = ILI9341_HEIGHT;
		p_owner->parent._height = ILI9341_WIDTH;
	}
	else {
		p_owner->parent._width = ILI9341_WIDTH;
		p_owner->parent._height = ILI9341_HEIGHT;
	}
	p_owner->parent._rotation = rotation;

	(*p_owner->__act_chip_select)(TRUE);
    (*p_owner->__act_write_cmd)(ILI9341_CMD_MEM_ACCESS_CTRL, &p_owner->__p_rotation[rotation], 1);
	(*p_owner->__act_chip_select)(FALSE);

	pifLog_Printf(LT_INFO, "Rot: %d, %X, %X, %X, %X", rotation, p_owner->__p_rotation[0], p_owner->__p_rotation[1], p_owner->__p_rotation[2], p_owner->__p_rotation[3]);
}

void pifIli9341_DrawPixel(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint16_t data[2];

	size = _convertColor(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y);
	(*p_owner->__act_write_data)(data, size);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifIli9341_DrawHorLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint16_t data[2];

	size = _convertColor(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x + len, y);
	(*p_owner->__act_write_repeat)(data, size, len);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifIli9341_DrawVerLine(PifTftLcd* p_parent, uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint16_t data[2];

	size = _convertColor(p_owner, color, data);

	(*p_owner->__act_chip_select)(TRUE);
	_setAddress(p_owner, x, y, x, y + len);
	(*p_owner->__act_write_repeat)(data, size, len);
	(*p_owner->__act_chip_select)(FALSE);
}

void pifIli9341_DrawFillRect(PifTftLcd* p_parent, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
	PifIli9341* p_owner = (PifIli9341*)p_parent;
	uint8_t size;
	uint16_t i, data[2], lx, ly;
	uint32_t area;

	lx = x2 - x1 + 1;
	ly = y2 - y1 + 1;
	area = lx * ly;

	size = _convertColor(p_owner, color, data);

	if (area < p_owner->_view_size) {
		(*p_owner->__act_chip_select)(TRUE);
		_setAddress(p_owner, x1, y1, x2, y2);
		for (i = 0; i < ly; i++) {
			(*p_owner->__act_write_repeat)(data, size, lx);
		}
		(*p_owner->__act_chip_select)(FALSE);
	}
	else {
		if (lx > ly) {
			for (i = y1; i < y1 + ly; i++) {
				(*p_owner->__act_chip_select)(TRUE);
				_setAddress(p_owner, x1, i, x2, i);
				(*p_owner->__act_write_repeat)(data, size, lx);
				(*p_owner->__act_chip_select)(FALSE);
				pifTaskManager_Yield();
			}
		}
		else {
			for (i = x1; i < x1 + lx; i++) {
				(*p_owner->__act_chip_select)(TRUE);
				_setAddress(p_owner, i, y1, i, y2);
				(*p_owner->__act_write_repeat)(data, size, ly);
				(*p_owner->__act_chip_select)(FALSE);
				pifTaskManager_Yield();
			}
		}
	}
}
