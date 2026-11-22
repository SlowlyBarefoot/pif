// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_max7456.c
 * @brief The MAX7456 driver, written from the register map and the sequences of its datasheet.
 *
 * Nothing here reads a register back to change a field of it: the driver is the only writer of VM0, of DMM and of
 * the row brightness registers, so it keeps what it put there and writes the whole register. A chip the driver did
 * not put in that state, after a power-up or a reset from elsewhere, is brought back to it by pifMax7456_Reset()
 * and pifMax7456_Start().
 */

#include "display/osd/pif_max7456.h"

#include <string.h>


// What OSDM holds until something writes it, which is how pifMax7456_Detect() knows the chip.
#define MAX7456_OSDM_POWER_ON		0x1B

// The software reset is through in about 100 us and the display memory is blanked in about 20 us, so even one
// millisecond is already far more than either needs. The character NVM takes about 12 ms.
#define MAX7456_RESET_TIMEOUT		20
#define MAX7456_CLEAR_TIMEOUT		20
#define MAX7456_NVM_TIMEOUT			50

// RB0 to RB15, an address byte and a data byte each.
#define MAX7456_BRIGHTNESS_SIZE		((MAX7456_REG_W_RB15 - MAX7456_REG_W_RB0 + 1) * 2)

// __brightness: no levels are known to be in the row registers. No level pair comes out as this value.
#define MAX7456_BRIGHTNESS_NONE		0xFF


/**
 * @brief Polls a register until the bits under mask are clear. The chip reports a software reset, a display
 *        memory clear and a character NVM write through by clearing a bit of its own, so one wait does for all
 *        three.
 * @return TRUE when the bits are clear, otherwise FALSE with pif_error set to E_TIMEOUT, or to the error of the
 *         read that failed.
 */
static BOOL _waitClear(PifMax7456* p_owner, uint8_t reg, uint8_t mask, uint16_t timeout1ms)
{
	uint32_t start_time = pif_cumulative_timer1ms;
	uint8_t data;

	while (TRUE) {
		if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, reg, &data)) return FALSE;
		if (!(data & mask)) return TRUE;
		if (PIF_CHECK_ELAPSE_TIME_1MS(start_time, timeout1ms)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}

/**
 * @brief What DMM is to hold: 8 bit operation, the inversion asked for, and the bits of one operation (a clear or
 *        an auto-increment run) on top of them.
 */
static uint8_t _displayMode(PifMax7456* p_owner, uint8_t operation)
{
	return (uint8_t)(MAX7456_OPER_MODE_SELECT_8BIT | p_owner->__invert | operation);
}

/**
 * @brief Puts the 54 bytes of a pattern in the shadow RAM of a character: the character address once, then each
 *        byte at its own address in the pattern.
 */
static BOOL _writeShadowRam(PifMax7456* p_owner, uint8_t char_address, const uint8_t* font_data)
{
	uint8_t i;

	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMAH, char_address)) return FALSE;
	for (i = 0; i < MAX7456_CHAR_DATA_SIZE; i++) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMAL, i)) return FALSE;
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMDI, font_data[i])) return FALSE;
	}
	return TRUE;
}


BOOL pifMax7456_Init(PifMax7456* p_owner, PifId id, PifSpiPort* p_port, void* p_client)
{
	PifSpiDevice* p_device;

	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax7456));
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
		PifSpiDevice* p_device = (PifSpiDevice*)p_owner->_p_device;

		if (!pifSpiPort_RemoveDevice(p_device->_p_port, p_device)) return FALSE;
		p_owner->_p_device = NULL;
	}
	return TRUE;
}

BOOL pifMax7456_Detect(PifMax7456* p_owner)
{
	uint8_t data;

	p_owner->detected = FALSE;
	if (!pifDeviceReg_ReadRegByte(p_owner->_p_device, MAX7456_REG_R_OSDM, &data)) return FALSE;
	// Nothing on the bus reads as 0xFF or 0x00, so a chip that answers with the power-on value is one.
	p_owner->detected = data == MAX7456_OSDM_POWER_ON;
	return p_owner->detected;
}

BOOL pifMax7456_Reset(PifMax7456* p_owner)
{
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0,
			MAX7456_SOFTWARE_RESET_BIT(1))) return FALSE;

	// The bit clears itself once the chip is through. Reads while it is resetting answer with it still set.
	if (!_waitClear(p_owner, MAX7456_REG_R_VM0, MAX7456_SOFTWARE_RESET_BIT_MASK,
			MAX7456_RESET_TIMEOUT)) return FALSE;

	// Every register is at its power-on value now: VM0 and DMM are 0, and what is in the row registers is the
	// factory brightness, which is none of the four levels of pifMax7456_Brightness().
	p_owner->__vm0 = 0;
	p_owner->__invert = MAX7456_INVERT_BIT_NORMAL;
	pifMax7456_InitBrightness(p_owner);
	return TRUE;
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
	// The picture on, the characters put up at the vertical sync so that none of them is torn, the sync taken
	// from the signal while there is one and made by the chip itself while there is none, so that the characters
	// show without a camera, and the video of the input through to the output.
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
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_DMM,
			_displayMode(p_owner, MAX7456_CLEAR_DISP_MEM_CLEAR))) return FALSE;

	// The bit clears itself once every character of the display memory is blank.
	return _waitClear(p_owner, MAX7456_REG_R_DMM, MAX7456_CLEAR_DISP_MEM_MASK, MAX7456_CLEAR_TIMEOUT);
}

BOOL pifMax7456_WriteChar(PifMax7456* p_owner, uint16_t address, uint8_t character)
{
	uint8_t buf[8];

	// The mode, the two address bytes and the character, under one chip select.
	buf[0] = MAX7456_REG_W_DMM;
	buf[1] = _displayMode(p_owner, 0);
	buf[2] = MAX7456_REG_W_DMAH;
	buf[3] = (uint8_t)(address >> 8) & MAX7456_DISP_MEM_ADDR_BIT_8_MASK;		// the character, not its attribute
	buf[4] = MAX7456_REG_W_DMAL;
	buf[5] = (uint8_t)address;
	buf[6] = MAX7456_REG_W_DMDI;
	buf[7] = character;
	return pifSpiDevice_Transfer((PifSpiDevice*)p_owner->_p_device, buf, NULL, sizeof(buf));
}

BOOL pifMax7456_WriteChars(PifMax7456* p_owner, uint16_t address, const uint8_t* p_characters, uint16_t count)
{
	// The two address bytes, the mode that starts the run, a pair for each character and the escape that ends it.
	uint8_t buf[2 * (MAX7456_COLUMNS + 4)];
	size_t size = 2 * ((size_t)count + 4);
	uint16_t i, n = 0;

	if (!p_characters || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// A run of more than a row does not fit the buffer, the escape would end the run where it stands instead of
	// reaching the picture, and a port of its own transfer size may not take the run whole. Each of the three is
	// written character by character, which needs none of it.
	if (count > MAX7456_COLUMNS || memchr(p_characters, MAX7456_ESCAPE, count) ||
			(p_owner->_p_device->max_transfer_size && p_owner->_p_device->max_transfer_size < size)) {
		for (i = 0; i < count; i++) {
			if (!pifMax7456_WriteChar(p_owner, address + i, p_characters[i])) return FALSE;
		}
		return TRUE;
	}

	buf[n++] = MAX7456_REG_W_DMAH;
	buf[n++] = (uint8_t)(address >> 8) & MAX7456_DISP_MEM_ADDR_BIT_8_MASK;
	buf[n++] = MAX7456_REG_W_DMAL;
	buf[n++] = (uint8_t)address;
	buf[n++] = MAX7456_REG_W_DMM;
	buf[n++] = _displayMode(p_owner, MAX7456_AUTO_INCR_MODE(1));
	// The chip steps the address on by itself, one character a pair.
	for (i = 0; i < count; i++) {
		buf[n++] = MAX7456_REG_W_DMDI;
		buf[n++] = p_characters[i];
	}
	buf[n++] = MAX7456_REG_W_DMDI;
	buf[n++] = MAX7456_ESCAPE;				// ends the run, so the chip leaves auto-increment mode
	return pifSpiDevice_Transfer((PifSpiDevice*)p_owner->_p_device, buf, NULL, n);
}

BOOL pifMax7456_Invert(PifMax7456* p_owner, BOOL invert)
{
	p_owner->__invert = invert ? MAX7456_INVERT_BIT_INVERT : MAX7456_INVERT_BIT_NORMAL;
	return pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_DMM, _displayMode(p_owner, 0));
}

BOOL pifMax7456_Brightness(PifMax7456* p_owner, uint8_t black, uint8_t white)
{
	PifDeviceReg* p_device = p_owner->_p_device;
	uint8_t buf[MAX7456_BRIGHTNESS_SIZE];
	uint8_t level, reg;
	int n = 0;

	if (black > 3 || white > 3) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// The white level of the register counts the other way round: 0 is the brightest of the four.
	level = (uint8_t)((black << 2) | (3 - white));
	if (level == p_owner->__brightness) return TRUE;

	if (!p_device->max_transfer_size || p_device->max_transfer_size >= MAX7456_BRIGHTNESS_SIZE) {
		// Every row under one chip select: the picture never shows two levels at once.
		for (reg = MAX7456_REG_W_RB0; reg <= MAX7456_REG_W_RB15; reg++) {
			buf[n++] = reg;
			buf[n++] = level;
		}
		if (!pifSpiDevice_Transfer((PifSpiDevice*)p_device, buf, NULL, sizeof(buf))) return FALSE;
	}
	else {
		for (reg = MAX7456_REG_W_RB0; reg <= MAX7456_REG_W_RB15; reg++) {
			if (!pifDeviceReg_WriteRegByte(p_device, reg, level)) return FALSE;
		}
	}
	p_owner->__brightness = level;
	return TRUE;
}

void pifMax7456_InitBrightness(PifMax7456* p_owner)
{
	p_owner->__brightness = MAX7456_BRIGHTNESS_NONE;
}

BOOL pifMax7456_WriteNvm(PifMax7456* p_owner, uint8_t char_address, const uint8_t* font_data)
{
	BOOL ret;

	if (!font_data) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	// A background transfer that holds the port would make every access below fail.
	if (!p_owner->detected || pifDeviceReg_IsBusy(p_owner->_p_device)) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	// The picture has to be off while the character memory is written. The rest of VM0 stays as it is, so the
	// chip keeps its standard and its sync.
	if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0,
			p_owner->__vm0 & ~MAX7456_ENABLE_DISP_OSD_IMAGE_MASK)) return FALSE;

	// Only a pattern that reached the shadow RAM whole is stored, so a broken one never gets into the NVM.
	ret = _writeShadowRam(p_owner, char_address, font_data);
	if (ret) ret = pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_CMM, MAX7456_CMM_WRITE_NVM);
	if (ret) ret = _waitClear(p_owner, MAX7456_REG_R_STAT, MAX7456_CHAR_MEM_STATUS_MASK, MAX7456_NVM_TIMEOUT);

	// The NVM may still be being written after a timeout, and it is not to be interrupted, so the picture is
	// left off then.
	if (ret || pif_error != E_TIMEOUT) {
		if (!pifDeviceReg_WriteRegByte(p_owner->_p_device, MAX7456_REG_W_VM0, p_owner->__vm0)) ret = FALSE;
	}
	return ret;
}
