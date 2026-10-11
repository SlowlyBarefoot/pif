// SPDX-License-Identifier: BSD-3-Clause
#include "input/pif_tsc2046.h"


#define TSC2046_CMD_X		0xD0	// Start, X position, 12-bit, differential, power down between
#define TSC2046_CMD_Y		0x90	// Start, Y position, 12-bit, differential, power down between


/**
 * @brief Converts one channel. The command goes out in the first byte and the 12-bit result
 *        follows in the next sixteen clocks, after one busy bit and before three trailing zeros.
 * @param p_owner Pointer to the TSC2046 instance.
 * @param command Control byte that selects the channel.
 * @return The 12-bit conversion result.
 */
static int16_t _readChannel(PifTsc2046* p_owner, uint8_t command)
{
	uint8_t write[3] = { command, 0, 0 };
	uint8_t read[3] = { 0, 0, 0 };

	pifSpiDevice_Transfer(p_owner->_p_spi, write, read, 3);
	return ((read[1] << 8) | read[2]) >> 3;
}

/**
 * @brief Reads raw touch coordinates from the TSC2046 over SPI.
 * @param p_parent Pointer to the parent touch-screen instance.
 * @param x Output pointer for raw X coordinate.
 * @param y Output pointer for raw Y coordinate.
 */
static void _actTouchPosition(PifTouchScreen* p_parent, int16_t* x, int16_t* y)
{
	PifTsc2046* p_owner = (PifTsc2046*)p_parent;

	*x = _readChannel(p_owner, TSC2046_CMD_X);
	*y = _readChannel(p_owner, TSC2046_CMD_Y);
}

/**
 * @brief Returns the current pen state from the external pen callback.
 * @param p_parent Pointer to the parent touch-screen instance.
 * @return `TRUE` when pen/touch is detected, otherwise `FALSE`.
 */
static BOOL _actTouchPressure(PifTouchScreen* p_parent)
{
	PifTsc2046* p_owner = (PifTsc2046*)p_parent;

	return (*p_owner->__act_pen)();
}

BOOL pifTsc2046_Init(PifTsc2046* p_owner, PifId id, PifTftLcd* p_lcd, int16_t left_x, int16_t right_x, int16_t top_y, int16_t bottom_y, PifSpiPort* p_port, void *p_client, PifActTsc2046Pen act_pen)
{
	if (!p_owner || !p_lcd || !p_port || !act_pen) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifTsc2046));

	if (!pifTouchScreen_Init(&p_owner->parent, id, p_lcd, left_x, right_x, top_y, bottom_y)) return FALSE;

	p_owner->_p_spi = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
	if (!p_owner->_p_spi) return FALSE;

	if (!pifTouchScreen_AttachAction(&p_owner->parent, _actTouchPosition, _actTouchPressure)) return FALSE;

	p_owner->__act_pen = act_pen;
	return TRUE;
}

void pifTsc2046_Clear(PifTsc2046* p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->_p_port, p_owner->_p_spi);
		p_owner->_p_spi = NULL;
	}
	pifTouchScreen_Clear(&p_owner->parent);
}
