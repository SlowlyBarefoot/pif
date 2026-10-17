// SPDX-License-Identifier: BSD-3-Clause
#include "storage/pif_littlefs.h"

#ifdef PIF_USE_LITTLEFS


/**
 * @fn _address
 * @brief Where a byte of a block is in the flash.
 * @param c Configuration of the file system.
 * @param block Block number.
 * @param off Offset in the block.
 * @return Address in the flash.
 */
static uint32_t _address(const struct lfs_config *c, lfs_block_t block, lfs_off_t off)
{
	PifLittlefs* p_owner = (PifLittlefs*)c->context;

	return p_owner->_base + block * c->block_size + off;
}

static int _read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
	PifLittlefs* p_owner = (PifLittlefs*)c->context;

	return pifNorFlash_Read(p_owner->_p_flash, _address(c, block, off), (uint8_t*)buffer, size) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	PifLittlefs* p_owner = (PifLittlefs*)c->context;

	return pifNorFlash_Program(p_owner->_p_flash, _address(c, block, off), (const uint8_t*)buffer, size) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _erase(const struct lfs_config *c, lfs_block_t block)
{
	PifLittlefs* p_owner = (PifLittlefs*)c->context;

	return pifNorFlash_EraseSector(p_owner->_p_flash, _address(c, block, 0)) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int _sync(const struct lfs_config *c)
{
	(void)c;

	// Every program is waited for before it returns, so nothing is left to flush.
	return LFS_ERR_OK;
}

BOOL pifLittlefs_Init(PifLittlefs* p_owner, PifNorFlash* p_flash, uint32_t base, uint32_t size)
{
	if (!p_owner || !p_flash || base % PIF_NOR_FLASH_SECTOR_SIZE || base >= p_flash->_capacity) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!size) size = p_flash->_capacity - base;
	// littlefs needs two blocks for its root directory and more for anything else.
	if (size % PIF_NOR_FLASH_SECTOR_SIZE || size > p_flash->_capacity - base || size < 4 * PIF_NOR_FLASH_SECTOR_SIZE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifLittlefs));
	p_owner->_p_flash = p_flash;
	p_owner->_base = base;

	p_owner->cfg.context = p_owner;
	p_owner->cfg.read = _read;
	p_owner->cfg.prog = _prog;
	p_owner->cfg.erase = _erase;
	p_owner->cfg.sync = _sync;

	p_owner->cfg.read_size = 16;
	p_owner->cfg.prog_size = 16;
	p_owner->cfg.block_size = PIF_NOR_FLASH_SECTOR_SIZE;
	p_owner->cfg.block_count = size / PIF_NOR_FLASH_SECTOR_SIZE;
	p_owner->cfg.block_cycles = 500;
	p_owner->cfg.cache_size = PIF_NOR_FLASH_PAGE_SIZE;
	p_owner->cfg.lookahead_size = 32;
	return TRUE;
}

#endif  // PIF_USE_LITTLEFS
