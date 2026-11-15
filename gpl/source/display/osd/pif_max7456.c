// SPDX-License-Identifier: GPL-3.0-or-later
// Portions derived from Betaflight max7456.c, GPL-3.0-or-later.
// See THIRD_PARTY_NOTICES.md.

#include "display/osd/pif_max7456.h"

#include <string.h>


// OSDM is not written by this driver, so it keeps its power-on value.
#define MAX7456_OSDM_DEFAULT		0x1B

// The software reset is over in about 100 us, the display memory is cleared in about 20 us.
#define MAX7456_RESET_TIMEOUT		20
#define MAX7456_CLEAR_TIMEOUT		20

// Writing the NVM takes about 12 ms.
#define MAX7456_NVM_WRITE_TIMEOUT	50

// RB0 to RB15, an address byte and a data byte each.
#define MAX7456_BRIGHTNESS_SIZE		((MAX7456_REG_W_RB15 - MAX7456_REG_W_RB0 + 1) * 2)


static BOOL _writeShadowRam(PifMax7456* p_owner, uint8_t char_address, const uint8_t *font_data)
{
	int x;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMAH, char_address)) return FALSE;

	for (x = 0; x < MAX7456_CHAR_DATA_SIZE; x++) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMAL, x)) return FALSE;
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMDI, font_data[x])) return FALSE;
	}
	return TRUE;
}

static BOOL _waitNvmWrite(PifMax7456* p_owner)
{
	uint32_t start_time = pif_cumulative_timer1ms;
	uint8_t data;

	// Wait until bit 5 in the status register returns to 0. A missing device can read as 0xFF.
	while (TRUE) {
		if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_STAT, &data)) return FALSE;
		if (!(data & MAX7456_CHAR_MEM_STATUS_MASK)) return TRUE;
		if (PIF_CHECK_ELAPSE_TIME_1MS(start_time, MAX7456_NVM_WRITE_TIMEOUT)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}


// The display memory mode the driver keeps (DMM): 8 bit operation, and the invert bit of pifMax7456_Invert().
static uint8_t _displayMode(PifMax7456* p_owner, uint8_t more)
{
	return MAX7456_OPER_MODE_SELECT_8BIT | p_owner->_display_memory_mode | more;
}


BOOL pifMax7456_Detect(PifMax7456* p_owner)
{
	uint8_t data;

	p_owner->detected = FALSE;
	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_OSDM, &data)) return FALSE;
	p_owner->detected = data == MAX7456_OSDM_DEFAULT;
	return p_owner->detected;
}

BOOL pifMax7456_Init(PifMax7456* p_owner, PifId id, PifSpiPort* p_port, void *p_client)
{
	PifSpiDevice *p_device;

	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax7456));
	p_owner->__previous_invert = 0xFF;
	pifMax7456_InitBrightness(p_owner);

	p_device = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
	if (!p_device) return FALSE;
	p_owner->_p_device = &p_device->parent;

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	return TRUE;
}

BOOL pifMax7456_Clear(PifMax7456* p_owner)
{
	if (p_owner->_p_device) {
		PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;

		if (!pifSpiPort_RemoveDevice(p_device->_p_port, p_device)) return FALSE;
		p_owner->_p_device = NULL;
	}
	return TRUE;
}

BOOL pifMax7456_WriteNvm(PifMax7456* p_owner, uint8_t char_address, const uint8_t *font_data)
{
	uint8_t vm0;
	BOOL ret;

	if (!font_data) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!p_owner->detected) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// A background transfer that holds the port would make every access below fail.
	if (pifDeviceReg_IsBusy(p_owner->_p_device)) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_VM0, &vm0)) return FALSE;

	// disable display
	p_owner->_font_is_loading = TRUE;
	ret = pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, 0);

	// A shadow RAM that was not written in full is not stored, so a broken pattern does not reach the NVM.
	if (ret) ret = _writeShadowRam(p_owner, char_address, font_data);

	// Transfer 54 bytes from shadow ram to NVM
	if (ret) ret = pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMM, 0xA0);
	if (ret) ret = _waitNvmWrite(p_owner);

	// Restore the display, unless the NVM is still being written.
	if (ret || pif_error != E_TIMEOUT) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, vm0 & ~MAX7456_SOFTWARE_RESET_BIT_MASK)) ret = FALSE;
	}
	p_owner->_font_is_loading = FALSE;

	return ret;
}

BOOL pifMax7456_Invert(PifMax7456* p_owner, BOOL invert)
{
	if (invert) {
		p_owner->_display_memory_mode |= MAX7456_INVERT_BIT_INVERT;
	} else {
		p_owner->_display_memory_mode &= ~MAX7456_INVERT_BIT_INVERT;
	}

	if (p_owner->_display_memory_mode != p_owner->__previous_invert) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_DMM, p_owner->_display_memory_mode)) return FALSE;
		p_owner->__previous_invert = p_owner->_display_memory_mode;
	}
	return TRUE;
}

BOOL pifMax7456_Reset(PifMax7456* p_owner)
{
	uint32_t start_time;
	uint8_t data;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, MAX7456_SOFTWARE_RESET_BIT(1))) return FALSE;

	// The bit clears itself when the chip is back. Reads while it is resetting answer with the bit still set.
	start_time = pif_cumulative_timer1ms;
	while (TRUE) {
		if (pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_VM0, &data) &&
				!(data & MAX7456_SOFTWARE_RESET_BIT_MASK)) {
			p_owner->__vm0 = data;
			p_owner->_display_memory_mode = 0;
			p_owner->__previous_invert = 0xFF;
			pifMax7456_InitBrightness(p_owner);
			return TRUE;
		}
		if (PIF_CHECK_ELAPSE_TIME_1MS(start_time, MAX7456_RESET_TIMEOUT)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}

PifMax7456Video pifMax7456_VideoSignal(PifMax7456* p_owner)
{
	uint8_t data;

	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_STAT, &data)) return MAX7456_VIDEO_NONE;
	if (data & MAX7456_PAL_SIGNAL_DETECT_MASK) return MAX7456_VIDEO_PAL;
	if (data & MAX7456_NTSC_SIGNAL_DETECT_MASK) return MAX7456_VIDEO_NTSC;
	return MAX7456_VIDEO_NONE;
}

BOOL pifMax7456_Start(PifMax7456* p_owner, PifMax7456VideoStandardSelect standard)
{
	// The picture on, the characters put up at the vertical sync so that none is torn, and the sync taken from
	// the signal when there is one (internal otherwise, so that the characters show without a camera).
	p_owner->__vm0 = (uint8_t)standard | MAX7456_SYNC_SELECT_MODE_AUTO | MAX7456_VERT_SYNC_OSD_VSYNC |
			MAX7456_ENABLE_DISP_OSD_IMAGE(1) | MAX7456_VIDEO_BUFFER_ENABLE;
	return pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, p_owner->__vm0);
}

BOOL pifMax7456_Enable(PifMax7456* p_owner, BOOL on)
{
	if (on) p_owner->__vm0 |= MAX7456_ENABLE_DISP_OSD_IMAGE_MASK;
	else p_owner->__vm0 &= ~MAX7456_ENABLE_DISP_OSD_IMAGE_MASK;
	return pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, p_owner->__vm0);
}

BOOL pifMax7456_ClearDisplay(PifMax7456* p_owner)
{
	uint32_t start_time;
	uint8_t data;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_DMM,
			_displayMode(p_owner, MAX7456_CLEAR_DISP_MEM_CLEAR))) return FALSE;
	p_owner->__previous_invert = 0xFF;		// DMM was written from here, not by pifMax7456_Invert()

	start_time = pif_cumulative_timer1ms;
	while (TRUE) {
		if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_DMM, &data)) return FALSE;
		if (!(data & MAX7456_CLEAR_DISP_MEM_MASK)) return TRUE;
		if (PIF_CHECK_ELAPSE_TIME_1MS(start_time, MAX7456_CLEAR_TIMEOUT)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}

BOOL pifMax7456_WriteChar(PifMax7456* p_owner, uint16_t address, uint8_t character)
{
	uint8_t buf[8];

	buf[0] = MAX7456_REG_W_DMM;
	buf[1] = _displayMode(p_owner, 0);
	buf[2] = MAX7456_REG_W_DMAH;
	buf[3] = (uint8_t)(address >> 8) & 0x01;		// bit 1 is the attribute byte: the character is bit 1 clear
	buf[4] = MAX7456_REG_W_DMAL;
	buf[5] = (uint8_t)address;
	buf[6] = MAX7456_REG_W_DMDI;
	buf[7] = character;
	p_owner->__previous_invert = 0xFF;
	return pifSpiDevice_Transfer((PifSpiDevice *)p_owner->_p_device, buf, NULL, sizeof(buf));
}

BOOL pifMax7456_WriteChars(PifMax7456* p_owner, uint16_t address, const uint8_t* p_characters, uint16_t count)
{
	PifSpiDevice *p_device = (PifSpiDevice *)p_owner->_p_device;
	uint8_t buf[2 * (MAX7456_COLUMNS + 5)];
	uint16_t i, n = 0;

	if (!p_characters || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	// The escape ends the run, so a character of that value is written on its own.
	if (count > (sizeof(buf) / 2) - 5 || memchr(p_characters, MAX7456_ESCAPE, count) ||
			(p_owner->_p_device->max_transfer_size && p_owner->_p_device->max_transfer_size < 2 * (count + 5))) {
		for (i = 0; i < count; i++) {
			if (!pifMax7456_WriteChar(p_owner, address + i, p_characters[i])) return FALSE;
		}
		return TRUE;
	}

	buf[n++] = MAX7456_REG_W_DMAH;
	buf[n++] = (uint8_t)(address >> 8) & 0x01;
	buf[n++] = MAX7456_REG_W_DMAL;
	buf[n++] = (uint8_t)address;
	buf[n++] = MAX7456_REG_W_DMM;
	buf[n++] = _displayMode(p_owner, MAX7456_AUTO_INCR_MODE(1));
	for (i = 0; i < count; i++) {
		buf[n++] = MAX7456_REG_W_DMDI;
		buf[n++] = p_characters[i];
	}
	buf[n++] = MAX7456_REG_W_DMDI;			// ends the run and leaves auto-increment mode
	buf[n++] = MAX7456_ESCAPE;
	p_owner->__previous_invert = 0xFF;
	return pifSpiDevice_Transfer(p_device, buf, NULL, n);
}

void pifMax7456_InitBrightness(PifMax7456* p_owner)
{
	p_owner->__previous_brightness = 0xFF;
}

BOOL pifMax7456_Brightness(PifMax7456* p_owner, uint8_t black, uint8_t white)
{
	PifDeviceReg *p_device = p_owner->_p_device;
	uint8_t reg;
	uint8_t buf[MAX7456_BRIGHTNESS_SIZE];
	int i, j;

	if (black > 3 || white > 3) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	reg = (black << 2) | (3 - white);
	if (reg == p_owner->__previous_brightness) return TRUE;

	if (!p_device->max_transfer_size || p_device->max_transfer_size >= MAX7456_BRIGHTNESS_SIZE) {
		for (i = MAX7456_REG_W_RB0, j = 0; i <= MAX7456_REG_W_RB15; i++) {
			buf[j++] = i;
			buf[j++] = reg;
		}
		if (!pifSpiDevice_Transfer((PifSpiDevice *)p_device, buf, NULL, sizeof(buf))) return FALSE;
	}
	else {
		// The port cannot take all rows under one chip select, so each row is written on its own.
		for (i = MAX7456_REG_W_RB0; i <= MAX7456_REG_W_RB15; i++) {
			if (!pifDeviceReg_WriteRegByte(p_device, i, reg)) return FALSE;
		}
	}
	p_owner->__previous_brightness = reg;
	return TRUE;
}
