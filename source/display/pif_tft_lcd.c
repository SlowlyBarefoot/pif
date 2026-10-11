// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_tft_lcd.h"


BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation)
{
	if (!p_owner || rotation > TLR_270_DEGREE) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTftLcd));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
	if (rotation & 1) {
		p_owner->_width = height;
		p_owner->_height = width;
	}
	else {
		p_owner->_width = width;
		p_owner->_height = height;
	}
	p_owner->_rotation = rotation;
    return TRUE;
}

BOOL pifTftLcd_ClipArea(PifTftLcd* p_owner, uint16_t* p_x1, uint16_t* p_y1, uint16_t* p_x2, uint16_t* p_y2)
{
	if (*p_x1 > *p_x2 || *p_y1 > *p_y2) return FALSE;
	if (*p_x1 >= p_owner->_width || *p_y1 >= p_owner->_height) return FALSE;

	if (*p_x2 >= p_owner->_width) *p_x2 = p_owner->_width - 1;
	if (*p_y2 >= p_owner->_height) *p_y2 = p_owner->_height - 1;
	return TRUE;
}
