// SPDX-License-Identifier: BSD-3-Clause
#include "storage/pif_nor_flash.h"


#define CMD_WRITE_ENABLE		0x06
#define CMD_READ_STATUS			0x05
#define CMD_READ_DATA			0x03
#define CMD_PAGE_PROGRAM		0x02
#define CMD_SECTOR_ERASE		0x20
#define CMD_CHIP_ERASE			0xC7
#define CMD_JEDEC_ID			0x9F

#define SR_BUSY					0x01
#define SR_WEL					0x02


/**
 * @fn _command
 * @brief Sends a command and its address, if it has one, under one chip select.
 * @param p_owner Pointer to the flash instance.
 * @param p_command Command byte, followed by the address bytes.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE.
 */
static BOOL _command(PifNorFlash* p_owner, uint8_t* p_command, size_t size)
{
	return pifSpiDevice_Transfer((PifSpiDevice*)p_owner->_p_device, p_command, NULL, size);
}

/**
 * @fn _readStatus
 * @brief Reads status register 1.
 * @param p_owner Pointer to the flash instance.
 * @param p_status Where it goes.
 * @return TRUE on success, otherwise FALSE.
 */
static BOOL _readStatus(PifNorFlash* p_owner, uint8_t* p_status)
{
	return pifDeviceReg_Read(p_owner->_p_device, CMD_READ_STATUS, 1, p_status, 1);
}

/**
 * @fn _waitReady
 * @brief Waits with the CPU held until the device is no longer busy.
 * @param p_owner Pointer to the flash instance.
 * @param timeout Longest it may take, in ms.
 * @return TRUE once it is ready, otherwise FALSE with pif_error set.
 */
static BOOL _waitReady(PifNorFlash* p_owner, uint16_t timeout)
{
	uint32_t start = pif_cumulative_timer1ms;
	uint8_t status;

	for (;;) {
		if (!_readStatus(p_owner, &status)) return FALSE;
		if (!(status & SR_BUSY)) return TRUE;
		if (PIF_CHECK_ELAPSE_TIME_1MS(start, timeout)) {
			pif_error = E_TIMEOUT;
			return FALSE;
		}
	}
}

/**
 * @fn _isFree
 * @brief Checks that no erase that was started and left alone is still going.
 * @param p_owner Pointer to the flash instance.
 * @return TRUE when the device may be accessed, otherwise FALSE with pif_error set.
 */
static BOOL _isFree(PifNorFlash* p_owner)
{
	BOOL busy;

	if (!p_owner->__erasing) return TRUE;

	if (!pifNorFlash_CheckBusy(p_owner, &busy)) return FALSE;
	if (busy) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _writeEnable
 * @brief Sets the write enable latch, which every program and erase needs and clears again.
 * @param p_owner Pointer to the flash instance.
 * @return TRUE when the device took it, otherwise FALSE with pif_error set.
 */
static BOOL _writeEnable(PifNorFlash* p_owner)
{
	uint8_t command = CMD_WRITE_ENABLE;
	uint8_t status;

	if (!_command(p_owner, &command, 1)) return FALSE;
	// Read back, because a device that is not there, or a bus that does not reach it, gives no other sign before
	// the data is lost.
	if (!_readStatus(p_owner, &status)) return FALSE;
	if ((status & (SR_WEL | SR_BUSY)) != SR_WEL) {
		pif_error = E_ACCESS_FAILED;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _isInRange
 * @brief Checks that an area lies inside the part of the device that is reached.
 * @param p_owner Pointer to the flash instance.
 * @param addr Where the area begins.
 * @param size Its size.
 * @return TRUE when it does, otherwise FALSE with pif_error set.
 */
static BOOL _isInRange(PifNorFlash* p_owner, uint32_t addr, size_t size)
{
	if (size > p_owner->_capacity || addr > p_owner->_capacity - size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _startErase
 * @brief Sends an erase command after the write enable.
 * @param p_owner Pointer to the flash instance.
 * @param p_command Command and address bytes.
 * @param size Number of bytes.
 * @return TRUE when the erase was started, otherwise FALSE with pif_error set.
 */
static BOOL _startErase(PifNorFlash* p_owner, uint8_t* p_command, size_t size)
{
	if (!_isFree(p_owner)) return FALSE;
	if (!_writeEnable(p_owner)) return FALSE;
	if (!_command(p_owner, p_command, size)) return FALSE;
	p_owner->__erasing = TRUE;
	return TRUE;
}

BOOL pifNorFlash_Init(PifNorFlash* p_owner, PifId id, PifSpiPort* p_port, void* p_client, uint32_t capacity)
{
	PifSpiDevice* p_device;
	uint8_t* p_jedec;

	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifNorFlash));

	p_device = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, p_client);
	if (!p_device) return FALSE;
	p_owner->_p_device = &p_device->parent;

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->program_timeout = 5;
	p_owner->erase_timeout = 500;

	p_jedec = p_owner->_jedec_id;
	if (!pifDeviceReg_Read(p_owner->_p_device, CMD_JEDEC_ID, 1, p_jedec, 3)) goto fail;
	// A line that nothing drives reads as all ones or all zeros, and no manufacturer has either code.
	if ((p_jedec[0] == 0x00 || p_jedec[0] == 0xFF) && p_jedec[1] == p_jedec[0]) {
		pif_error = E_CANNOT_FOUND;
		goto fail;
	}

	if (!capacity) {
		if (p_jedec[2] < 0x10 || p_jedec[2] > 0x1F) {
			pif_error = E_WRONG_DATA;
			goto fail;
		}
		capacity = 1UL << p_jedec[2];
	}
	p_owner->_capacity = capacity > PIF_NOR_FLASH_MAX_CAPACITY ? PIF_NOR_FLASH_MAX_CAPACITY : capacity;
	return TRUE;

fail:
	pifNorFlash_Clear(p_owner);
	return FALSE;
}

void pifNorFlash_Clear(PifNorFlash* p_owner)
{
	if (!p_owner) return;

	if (p_owner->_p_device) {
		PifSpiDevice* p_device = (PifSpiDevice*)p_owner->_p_device;

		pifSpiPort_RemoveDevice(p_device->_p_port, p_device);
		p_owner->_p_device = NULL;
	}
}

BOOL pifNorFlash_Read(PifNorFlash* p_owner, uint32_t addr, uint8_t* p_data, size_t size)
{
	if (!p_owner || !p_data) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!size) return TRUE;
	if (!_isInRange(p_owner, addr, size)) return FALSE;
	if (!_isFree(p_owner)) return FALSE;

	// The device steps the address on by itself, across pages and sectors. A port that splits the read sends
	// each piece with the address plus what was read, which stays clear of the command byte below 16MB.
	return pifDeviceReg_Read(p_owner->_p_device, ((uint32_t)CMD_READ_DATA << 24) | addr, 4, p_data, size);
}

BOOL pifNorFlash_Program(PifNorFlash* p_owner, uint32_t addr, const uint8_t* p_data, size_t size)
{
	uint16_t max_size;
	size_t len;

	if (!p_owner || !p_data) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!size) return TRUE;
	if (!_isInRange(p_owner, addr, size)) return FALSE;
	if (!_isFree(p_owner)) return FALSE;

	max_size = p_owner->_p_device->max_transfer_size;
	while (size) {
		// As far as the end of the page: past it the device wraps to the start of the same page.
		len = PIF_NOR_FLASH_PAGE_SIZE - addr % PIF_NOR_FLASH_PAGE_SIZE;
		if (len > size) len = size;
		// Every program needs a write enable of its own, so a port must not split one into pieces.
		if (max_size && len > max_size) len = max_size;

		if (!_writeEnable(p_owner)) return FALSE;
		if (!pifDeviceReg_Write(p_owner->_p_device, ((uint32_t)CMD_PAGE_PROGRAM << 24) | addr, 4, (uint8_t*)p_data, len)) return FALSE;
		if (!_waitReady(p_owner, p_owner->program_timeout)) return FALSE;

		addr += len;
		p_data += len;
		size -= len;
	}
	return TRUE;
}

BOOL pifNorFlash_EraseSector(PifNorFlash* p_owner, uint32_t addr)
{
	if (!pifNorFlash_StartEraseSector(p_owner, addr)) return FALSE;
	if (!_waitReady(p_owner, p_owner->erase_timeout)) return FALSE;
	p_owner->__erasing = FALSE;
	return TRUE;
}

BOOL pifNorFlash_StartEraseSector(PifNorFlash* p_owner, uint32_t addr)
{
	uint8_t command[4];

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_isInRange(p_owner, addr, 1)) return FALSE;

	command[0] = CMD_SECTOR_ERASE;
	command[1] = (addr >> 16) & 0xFF;
	command[2] = (addr >> 8) & 0xFF;
	command[3] = addr & 0xFF;
	return _startErase(p_owner, command, 4);
}

BOOL pifNorFlash_StartEraseChip(PifNorFlash* p_owner)
{
	uint8_t command = CMD_CHIP_ERASE;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return _startErase(p_owner, &command, 1);
}

BOOL pifNorFlash_CheckBusy(PifNorFlash* p_owner, BOOL* p_busy)
{
	uint8_t status;

	if (!p_owner || !p_busy) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_readStatus(p_owner, &status)) return FALSE;

	*p_busy = (status & SR_BUSY) != 0;
	if (!*p_busy) p_owner->__erasing = FALSE;
	return TRUE;
}
