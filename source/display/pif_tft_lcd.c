#include "display/pif_tft_lcd.h"


BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTftLcd));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	p_owner->_width = width;
	p_owner->_height = height;
	p_owner->_rotation = rotation;
    return TRUE;
}
