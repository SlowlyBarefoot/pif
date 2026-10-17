// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_NOR_FLASH_H
#define PIF_NOR_FLASH_H


/*
 * An SPI NOR flash with the common JEDEC command set, such as the Winbond W25Q, Macronix MX25L,
 * GigaDevice GD25Q or ISSI IS25LP series.
 *
 * A byte of it can be programmed only from 1 to 0, so data goes into an area that has been erased
 * first, a 4KB sector at a time. Programming goes a 256 byte page at a time and never past the end
 * of the page, which this takes care of. That is the media littlefs is built for, which
 * pif_littlefs puts on top of this.
 *
 * Addresses are three bytes, so at most the first 16MB of a larger device are reached.
 *
 * The SPI port needs act_transfer for the commands without data, act_read and act_write for the
 * ones with data. act_read and act_write send the isize bytes of the internal address high byte
 * first, as a register address goes, and here the first of them is the command. Reads use the
 * plain read command (0x03), which most devices take up to 33 to 50MHz.
 *
 * Programming a page takes a few milliseconds at most and is waited for. Erasing a sector takes
 * tens of milliseconds and up to a few hundred, which pifNorFlash_EraseSector() waits for too,
 * holding the CPU. pifNorFlash_StartEraseSector() and pifNorFlash_CheckBusy() let a task leave the
 * device alone meanwhile instead.
 */


#include "communication/pif_spi.h"


#define PIF_NOR_FLASH_PAGE_SIZE		256
#define PIF_NOR_FLASH_SECTOR_SIZE	4096

// Largest capacity three address bytes reach.
#define PIF_NOR_FLASH_MAX_CAPACITY	0x1000000UL


/**
 * @class StPifNorFlash
 * @brief SPI NOR flash device.
 */
typedef struct StPifNorFlash
{
	// Public Member Variable
	uint16_t program_timeout;		// Longest a page program may take, in ms. 5 by default.
	uint16_t erase_timeout;			// Longest a sector erase may take, in ms. 500 by default.

	// Read-only Member Variable
	PifId _id;
	PifDeviceReg *_p_device;
	uint8_t _jedec_id[3];			// Manufacturer, memory type, capacity
	uint32_t _capacity;				// Bytes that are reached

	// Private Member Variable
	BOOL __erasing;					// An erase was started and has not been seen over yet
} PifNorFlash;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifNorFlash_Init
 * @brief Adds the device to the port and reads its JEDEC ID, which is how it is found.
 * @param p_owner Pointer to the flash instance.
 * @param id Object id, or `PIF_ID_AUTO` to allocate automatically.
 * @param p_port SPI port the device is on.
 * @param p_client Client pointer passed to the SPI device, such as its chip select.
 * @param capacity Size of the device in bytes, or 0 to take it from the third byte of the JEDEC
 *        ID, as 2 to the power of it, which is how most devices give it. A capacity above 16MB is
 *        cut to 16MB.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set to E_CANNOT_FOUND when nothing
 *         answers, or E_WRONG_DATA when the capacity is to be taken from an ID that does not give
 *         it in that way.
 */
BOOL pifNorFlash_Init(PifNorFlash* p_owner, PifId id, PifSpiPort* p_port, void* p_client, uint32_t capacity);

/**
 * @fn pifNorFlash_Clear
 * @brief Removes the device from its port.
 * @param p_owner Pointer to the flash instance.
 */
void pifNorFlash_Clear(PifNorFlash* p_owner);

/**
 * @fn pifNorFlash_Read
 * @brief Reads bytes from any address, across pages and sectors.
 * @param p_owner Pointer to the flash instance.
 * @param addr Address to read from.
 * @param p_data Where the bytes go.
 * @param size Number of bytes.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set, to E_INVALID_STATE while an
 *         erase from pifNorFlash_StartEraseSector() is still going.
 */
BOOL pifNorFlash_Read(PifNorFlash* p_owner, uint32_t addr, uint8_t* p_data, size_t size);

/**
 * @fn pifNorFlash_Program
 * @brief Programs bytes into an erased area, a page at a time, and waits for each page. The bytes
 *        may begin and end anywhere: they are cut at the page boundaries here.
 *        Programming only clears bits, so a byte that is not erased ends up as the AND of what it
 *        was and what was programmed.
 * @param p_owner Pointer to the flash instance.
 * @param addr Address to program from.
 * @param p_data The bytes.
 * @param size Number of bytes.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set: E_ACCESS_FAILED when the device
 *         did not take the write enable, E_TIMEOUT when a page did not finish in program_timeout.
 */
BOOL pifNorFlash_Program(PifNorFlash* p_owner, uint32_t addr, const uint8_t* p_data, size_t size);

/**
 * @fn pifNorFlash_EraseSector
 * @brief Erases the 4KB sector the address falls in, and waits for it with the CPU held.
 * @param p_owner Pointer to the flash instance.
 * @param addr Any address in the sector.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set, to E_TIMEOUT when it did not
 *         finish in erase_timeout.
 */
BOOL pifNorFlash_EraseSector(PifNorFlash* p_owner, uint32_t addr);

/**
 * @fn pifNorFlash_StartEraseSector
 * @brief Starts erasing the 4KB sector the address falls in and returns without waiting. Until
 *        pifNorFlash_CheckBusy() says it is over, every other access fails with E_INVALID_STATE.
 * @param p_owner Pointer to the flash instance.
 * @param addr Any address in the sector.
 * @return `TRUE` when the erase was started, otherwise `FALSE` with pif_error set.
 */
BOOL pifNorFlash_StartEraseSector(PifNorFlash* p_owner, uint32_t addr);

/**
 * @fn pifNorFlash_StartEraseChip
 * @brief Starts erasing the whole device and returns without waiting, which takes from seconds to
 *        minutes depending on its size. Until pifNorFlash_CheckBusy() says it is over, every other
 *        access fails with E_INVALID_STATE.
 * @param p_owner Pointer to the flash instance.
 * @return `TRUE` when the erase was started, otherwise `FALSE` with pif_error set.
 */
BOOL pifNorFlash_StartEraseChip(PifNorFlash* p_owner);

/**
 * @fn pifNorFlash_CheckBusy
 * @brief Asks the device whether it is still busy with an erase or a program.
 * @param p_owner Pointer to the flash instance.
 * @param p_busy Where the answer goes: TRUE while it is busy.
 * @return `TRUE` when the device was asked, otherwise `FALSE` with pif_error set.
 */
BOOL pifNorFlash_CheckBusy(PifNorFlash* p_owner, BOOL* p_busy);

#ifdef __cplusplus
}
#endif


#endif  // PIF_NOR_FLASH_H
