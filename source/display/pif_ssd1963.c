// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_task_manager.h"
#include "display/pif_ssd1963.h"


#if PIF_COLOR_DEPTH == 16

static uint8_t _convertColor8bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = (color & 0xF800) >> 8;
	p_data[1] = (color & 0x07E0) >> 3;
	p_data[2] = (color & 0x001F) << 3;
	return 3;
}

static uint8_t _convertColor12bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xF800) >> 4) | ((color & 0x0780) >> 7);
	p_data[1] = ((color & 0x0060) << 5) | ((color & 0x001F) << 3);
	return 2;
}

static uint8_t _convertColor16bit565(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = color;
	return 1;
}

static uint8_t _convertColor18bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xF800) << 2) | ((color & 0x07E0) << 1) | ((color & 0x001F) << 1);
	return 1;
}

static uint8_t _convertColor24bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xF800) << 8) | ((color & 0x07E0) << 5) | ((color & 0x001F) << 3);
	return 1;
}

static uint8_t _convertColor9bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xF800) >> 7) | ((color & 0x0700) >> 8);
	p_data[1] = (color & 0x00FF) << 1;
	return 2;
}

#elif PIF_COLOR_DEPTH == 32

static uint8_t _convertColor8bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = (color & 0xFF0000) >> 16;
	p_data[1] = (color & 0x00FF00) >> 8;
	p_data[2] = color & 0x0000FF;
	return 3;
}

static uint8_t _convertColor12bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = (color & 0xFFF000) >> 12;
	p_data[1] = color & 0x000FFF;
	return 2;
}

static uint8_t _convertColor16bit565(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xF80000) >> 8) | ((color & 0x00FC00) >> 5) | ((color & 0x0000F8) >> 3);
	return 1;
}

static uint8_t _convertColor18bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xFC0000) >> 6) | ((color & 0x00FC00) >> 4) | ((color & 0x0000FC) >> 2);
	return 1;
}

static uint8_t _convertColor24bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = color;
	return 1;
}

static uint8_t _convertColor9bit(PifTftLcd *p_owner, PifColor color, uint32_t *p_data)
{
	(void)p_owner;

	p_data[0] = ((color & 0xFC0000) >> 15) | ((color & 0x00E000) >> 13);
	p_data[1] = ((color & 0x001C00) >> 4) | ((color & 0x0000FC) >> 2);
	return 2;
}

#endif

BOOL pifSsd1963_Init(PifSsd1963* p_owner, PifId id)
{
	PifTftLcd* p_parent;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSsd1963));

	p_parent = (PifTftLcd*)p_owner;
	if (!pifTftLcd_Init(&p_owner->parent, id, SSD1963_WIDTH, SSD1963_HEIGHT, TLR_0_DEGREE)) return FALSE;
	p_parent->_fn_set_rotation = pifSsd1963_SetRotation;
	p_parent->_fn_convert_color = _convertColor8bit;
	p_parent->_fn_set_address = pifTftLcd_SetAddressDcs;
    return TRUE;
}

BOOL pifSsd1963_AttachActParallel(PifSsd1963* p_owner, PifActLcdReset act_reset, PifActLcdChipSelect act_chip_select, PifActLcdReadCmd act_read_cmd,
		PifActLcdWriteCmd act_write_cmd, PifActLcdWriteData act_write_data, PifActLcdWriteRepeat act_write_repeat)
{
	PifTftLcd* p_parent;

	if (!p_owner || !act_reset || !act_chip_select || !act_write_cmd || !act_write_data || !act_write_repeat) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	p_parent = (PifTftLcd*)p_owner;
    p_parent->__act_reset = act_reset;
    p_parent->__act_chip_select = act_chip_select;
    p_parent->__act_read_cmd = act_read_cmd;
    p_parent->__act_write_cmd = act_write_cmd;
    p_parent->__act_write_data = act_write_data;
    p_parent->__act_write_repeat = act_write_repeat;
    return TRUE;
}

/**
 * @fn _peekSetup
 * @brief Reads the pixel format out of the setup stream, which the conversion of a color follows, before
 *        anything is sent.
 */
static void _peekSetup(PifTftLcd *p_lcd, uint8_t cmd, const uint8_t *p_param, uint8_t len)
{
	PifSsd1963 *p_owner = (PifSsd1963 *)p_lcd;

	switch (cmd) {
	case SSD1963_CMD_SET_PIX_DATA_IF:
		if (len >= 1) p_owner->__pixel_format = p_param[0] & 0x07;
		break;
	}
}

BOOL pifSsd1963_Setup(PifSsd1963* p_owner, const uint8_t* p_setup, const uint8_t* p_rotation)
{
	PifTftLcd* p_parent = (PifTftLcd*)p_owner;
	uint8_t i;

	if (!p_owner || !p_setup) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_parent->__act_reset || !p_parent->__act_chip_select || !p_parent->__act_write_cmd) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!pifTftLcd_ScanSetup(p_parent, p_setup, _peekSetup)) return FALSE;

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Pixel Format: %u", p_owner->__pixel_format);
#endif

	switch (p_owner->__pixel_format) {
	case SSD1963_PF_8BIT:
		p_parent->_fn_convert_color = _convertColor8bit;
		break;

	case SSD1963_PF_12BIT:
		p_parent->_fn_convert_color = _convertColor12bit;
		break;

	case SSD1963_PF_16BIT_565:
		p_parent->_fn_convert_color = _convertColor16bit565;
		break;

	case SSD1963_PF_18BIT:
		p_parent->_fn_convert_color = _convertColor18bit;
		break;

	case SSD1963_PF_24BIT:
		p_parent->_fn_convert_color = _convertColor24bit;
		break;

	case SSD1963_PF_9BIT:
		p_parent->_fn_convert_color = _convertColor9bit;
		break;

	default:
		// 16-bit packed sends two pixels in three words, which a per-pixel conversion cannot produce.
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	p_owner->__p_rotation = p_rotation;
#ifndef PIF_NO_LOG
	if (p_rotation) {
		pifLog_Printf(LT_INFO, "Rot: %X, %X, %X, %X", p_rotation[0], p_rotation[1], p_rotation[2], p_rotation[3]);
	}
#endif

    (*p_parent->__act_reset)();

	(*p_parent->__act_chip_select)(TRUE);

	for (i = 0; i < 3; i++) pifTftLcd_WriteCmd(p_parent, SSD1963_CMD_NOP, NULL, 0);

	pifTftLcd_RunSetup(p_parent, p_setup);

    pifTftLcd_WriteCmd(p_parent, SSD1963_CMD_SET_DISP_ON, NULL, 0);
	pifTftLcd_WriteCmd(p_parent, SSD1963_CMD_WRITE_MEM_START, NULL, 0);

	(*p_parent->__act_chip_select)(FALSE);
	return TRUE;
}

BOOL pifSsd1963_SetRotation(PifTftLcd* p_parent, PifTftLcdRotation rotation)
{
	PifSsd1963* p_owner = (PifSsd1963*)p_parent;

	if (rotation > TLR_270_DEGREE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->__p_rotation) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	if (rotation & 1) {
		pifGraphics_SetSize(&p_parent->parent, SSD1963_HEIGHT, SSD1963_WIDTH);
	}
	else {
		pifGraphics_SetSize(&p_parent->parent, SSD1963_WIDTH, SSD1963_HEIGHT);
	}
	p_parent->_rotation = rotation;

	(*p_parent->__act_chip_select)(TRUE);
	pifTftLcd_WriteCmd(p_parent, SSD1963_CMD_SET_ADDR_MODE, p_owner->__p_rotation + rotation, 1);
	(*p_parent->__act_chip_select)(FALSE);

#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "Rot: %d -> %X", rotation, p_owner->__p_rotation[rotation]);
#endif
	return TRUE;
}

