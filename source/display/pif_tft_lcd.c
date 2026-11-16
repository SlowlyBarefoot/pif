// SPDX-License-Identifier: BSD-3-Clause
#include "display/pif_tft_lcd.h"


BOOL pifTftLcd_Init(PifTftLcd* p_owner, PifId id, uint16_t width, uint16_t height, PifTftLcdRotation rotation)
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
    return TRUE;
}
