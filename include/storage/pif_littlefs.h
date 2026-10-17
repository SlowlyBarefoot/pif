// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_LITTLEFS_H
#define PIF_LITTLEFS_H


/*
 * Puts littlefs (https://github.com/littlefs-project/littlefs, BSD-3-Clause) on an SPI NOR flash
 * of pif_nor_flash. littlefs keeps its files intact across a power loss and spreads the wear
 * over the device by itself.
 *
 * littlefs is not part of pif. Add lfs.c and lfs_util.c of it to the build, put lfs.h on the
 * include path and define PIF_USE_LITTLEFS in pif_conf.h. Without PIF_USE_LITTLEFS this compiles
 * to nothing, so a build that takes every source of pif does not need littlefs.
 *
 * littlefs works with the CPU held: an erase of a sector, up to a few hundred milliseconds, is
 * waited for inside the call that needs it. Call it from a task that may take that long, not
 * from one with a deadline.
 */


#include "core/pif.h"

#ifdef PIF_USE_LITTLEFS

#include "lfs.h"
#include "storage/pif_nor_flash.h"


/**
 * @class StPifLittlefs
 * @brief A littlefs block device on an area of an SPI NOR flash.
 */
typedef struct StPifLittlefs
{
	// Public Member Variable
	// What lfs_format() and lfs_mount() take. pifLittlefs_Init() fills it; its sizes may be changed
	// before the file system is mounted, the callbacks and the geometry may not.
	struct lfs_config cfg;

	// Read-only Member Variable
	PifNorFlash* _p_flash;
	uint32_t _base;					// Where the area begins in the flash
} PifLittlefs;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifLittlefs_Init
 * @brief Describes an area of the flash to littlefs. One block is one 4KB sector. The rest of
 *        the configuration is set to what suits a NOR flash: reads and programs of 16 bytes at
 *        least, caches of one page (256 bytes), a lookahead of 32 bytes (256 blocks) and wear
 *        levelling every 500 erases of a block. The caches are allocated by littlefs when it is
 *        mounted, unless buffers are given in cfg before that.
 * @param p_owner Pointer to the instance.
 * @param p_flash Flash the area is on.
 * @param base Where the area begins, a multiple of the sector size.
 * @param size Size of the area, a multiple of the sector size, or 0 for the rest of the flash.
 * @return `TRUE` on success, otherwise `FALSE` with pif_error set to E_INVALID_PARAM.
 */
BOOL pifLittlefs_Init(PifLittlefs* p_owner, PifNorFlash* p_flash, uint32_t base, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  // PIF_USE_LITTLEFS


#endif  // PIF_LITTLEFS_H
