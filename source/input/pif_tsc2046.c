#include "input/pif_tsc2046.h"


static void _actTouchPosition(PifTouchScreen* p_parent, int16_t* x, int16_t* y)
{
	PifTsc2046* p_owner = (PifTsc2046*)p_parent;
	uint8_t write, read[2];

	write = 0xD0;
	pifSpiDevice_Transfer(p_owner->_p_spi, &write, read, 2);
	*x = ((read[0] << 8) | read[1]) >> 3;

	write = 0x90;
	pifSpiDevice_Transfer(p_owner->_p_spi, &write, read, 2);
	*y = ((read[0] << 8) | read[1]) >> 3;
}

static BOOL _actTouchPressure(PifTouchScreen* p_parent)
{
	PifTsc2046* p_owner = (PifTsc2046*)p_parent;

	return (*p_owner->__act_pen)();
}

BOOL pifTsc2046_Init(PifTsc2046* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y, PifSpiPort* p_port, PifActTsc2046Pen act_pen)
{
	if (!p_owner || !p_lcd || !p_port || !act_pen) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTsc2046));

	if (!pifTouchScreen_Init(&p_owner->parent, id, p_lcd, left_x, right_x, top_y, bottom_y)) return FALSE;

	p_owner->_p_spi = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO);
    if (!p_owner->_p_spi) return FALSE;

    if (!pifTouchScreen_AttachAction(&p_owner->parent, _actTouchPosition, _actTouchPressure)) return FALSE;

    p_owner->__act_pen = act_pen;
    return TRUE;
}

void pifTsc2046_Clear(PifTsc2046* p_owner)
{
	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
	}
}
