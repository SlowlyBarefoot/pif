#ifndef PIF_FLASH_H
#define PIF_FLASH_H


#include "core/pif.h"


/*
 * A region of flash that a record, such as a settings image, is written into as a stream and read
 * back from.
 *
 * Flash differs from the media pif_storage is built for in two ways. It is written only after it
 * has been erased, a whole erase unit (a sector or a page) at a time, and it is programmed only in
 * whole program units (a word, a double word, a 256-bit flash word) at aligned addresses. So a
 * record cannot be patched in place; it is written again from the start, and the bytes the client
 * writes have to be gathered into program units on their way.
 *
 * That is what this does. pifFlash_Begin() starts at the beginning of the region, and
 * pifFlash_Write() takes bytes in any amounts, erases each erase unit as the stream reaches it and
 * programs each program unit as it fills. pifFlash_End() pads and programs what is left over.
 * Every failure is kept, so a client may write the whole record and look at the result of
 * pifFlash_End() alone.
 *
 * What is hardware is left to the client: erasing the unit at an address, programming a unit,
 * reading, and unlocking the controller around a write if it needs that. An erase of a large
 * sector takes long, a second or two for 128 KB on an STM32F4, and the erase and program actions
 * are expected to wait for it, so a record is written when nothing time-critical runs.
 */


// Largest program unit a PifFlash accepts, and the size of the buffer it gathers one in.
#ifndef PIF_FLASH_MAX_PROGRAM_SIZE
#define PIF_FLASH_MAX_PROGRAM_SIZE		32
#endif


struct StPifFlash;
typedef struct StPifFlash PifFlash;

/**
 * @fn PifActFlashErase
 * @brief Erases the erase unit that begins at an address, and waits for it.
 * @param p_owner Pointer to the flash instance.
 * @param addr Address of the unit, base + a multiple of the erase size.
 * @return TRUE on success, otherwise FALSE.
 */
typedef BOOL (*PifActFlashErase)(PifFlash* p_owner, uint32_t addr);

/**
 * @fn PifActFlashProgram
 * @brief Programs one program unit, and waits for it.
 * @param p_owner Pointer to the flash instance.
 * @param addr Address of the unit, base + a multiple of the program size.
 * @param p_data Data of the unit, program size bytes, aligned as the largest native type is.
 * @param size Program size.
 * @return TRUE on success, otherwise FALSE.
 */
typedef BOOL (*PifActFlashProgram)(PifFlash* p_owner, uint32_t addr, const uint8_t* p_data, uint16_t size);

/**
 * @fn PifActFlashRead
 * @brief Reads from the region. On memory-mapped flash it is a memcpy().
 * @param p_owner Pointer to the flash instance.
 * @param addr Address to read from.
 * @param p_data Where the bytes go.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE.
 */
typedef BOOL (*PifActFlashRead)(PifFlash* p_owner, uint32_t addr, uint8_t* p_data, size_t size);

/**
 * @fn PifActFlashLock
 * @brief Unlocks the flash controller before a write and locks it again after.
 * @param p_owner Pointer to the flash instance.
 * @param lock FALSE to unlock, TRUE to lock.
 */
typedef void (*PifActFlashLock)(PifFlash* p_owner, BOOL lock);


/**
 * @class StPifFlash
 * @brief A flash region written as a stream, with erase and program alignment handled.
 */
struct StPifFlash
{
	// Read-only Member Variable
	PifId _id;
	uint32_t _base;				// Address of the region
	uint32_t _size;				// Size of the region, a multiple of the erase size
	uint32_t _erase_size;		// Erase unit, 0 for media that need no erase
	uint16_t _program_size;		// Program unit
	uint8_t _erased_value;		// What an erased byte reads as, and what a last unit is padded with
	BOOL _writing;				// Between pifFlash_Begin() and pifFlash_End()
	uint32_t _position;			// Bytes written since pifFlash_Begin()

	// Private Member Variable
	union {
		uint8_t b[PIF_FLASH_MAX_PROGRAM_SIZE];
		uint64_t align;			// So that act_program may read the unit as words
	} __unit;
	uint16_t __fill;			// Bytes in __unit
	uint32_t __next_erase;		// Offset of the first unit not erased yet
	PifError __error;			// First failure since pifFlash_Begin(), E_SUCCESS if none

	// Private Action Function
	PifActFlashErase __act_erase;
	PifActFlashProgram __act_program;
	PifActFlashRead __act_read;
	PifActFlashLock __act_lock;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifFlash_Init
 * @brief Initializes a flash instance over a region.
 * @param p_owner Pointer to the instance.
 * @param id Identifier, or PIF_ID_AUTO.
 * @param base Address of the region, aligned to the erase size.
 * @param size Size of the region, a multiple of the erase size.
 * @param erase_size Erase unit: the sector or page size. 0 for media that program without an erase,
 *        or erase as part of programming, in which case act_erase may be NULL.
 * @param program_size Program unit, 1 to PIF_FLASH_MAX_PROGRAM_SIZE, dividing the erase size.
 * @param act_erase Erases a unit. May be NULL only with an erase size of 0.
 * @param act_program Programs a unit.
 * @param act_read Reads from the region. May be NULL for a region that is only written.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifFlash_Init(PifFlash* p_owner, PifId id, uint32_t base, uint32_t size, uint32_t erase_size, uint16_t program_size,
		PifActFlashErase act_erase, PifActFlashProgram act_program, PifActFlashRead act_read);

/**
 * @fn pifFlash_AttachActLock
 * @brief Gives the instance a way to unlock the flash controller for a write.
 * @param p_owner Pointer to the instance.
 * @param act_lock Unlocks and locks the controller.
 */
void pifFlash_AttachActLock(PifFlash* p_owner, PifActFlashLock act_lock);

/**
 * @fn pifFlash_SetErasedValue
 * @brief Sets what an erased byte reads as, 0xFF unless the flash says otherwise. The last unit of
 *        a record is padded with it, which leaves the padding as good as unwritten.
 * @param p_owner Pointer to the instance.
 * @param value Erased byte.
 */
void pifFlash_SetErasedValue(PifFlash* p_owner, uint8_t value);

/**
 * @fn pifFlash_Read
 * @brief Reads from the region.
 * @param p_owner Pointer to the instance.
 * @param offset Offset in the region.
 * @param p_data Where the bytes go.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE (E_INVALID_PARAM past the end of the region,
 *         E_CANNOT_USE without act_read, E_ACCESS_FAILED if act_read failed).
 */
BOOL pifFlash_Read(PifFlash* p_owner, uint32_t offset, uint8_t* p_data, size_t size);

/**
 * @fn pifFlash_Begin
 * @brief Starts writing a record at the beginning of the region, unlocking the controller. Nothing
 *        is erased yet: each erase unit is erased when the stream reaches it, so a record that
 *        fills part of the region erases only that part.
 * @param p_owner Pointer to the instance.
 * @return TRUE on success, otherwise FALSE (E_INVALID_STATE while a record is being written).
 */
BOOL pifFlash_Begin(PifFlash* p_owner);

/**
 * @fn pifFlash_Write
 * @brief Adds bytes to the record. Once something has failed nothing more is written, and every
 *        call returns FALSE until pifFlash_End().
 * @param p_owner Pointer to the instance.
 * @param p_data Bytes to add.
 * @param size Number of bytes.
 * @return TRUE if everything so far went well, otherwise FALSE.
 */
BOOL pifFlash_Write(PifFlash* p_owner, const uint8_t* p_data, size_t size);

/**
 * @fn pifFlash_End
 * @brief Programs whatever is left of the record, padded to a whole unit, and locks the
 *        controller again.
 * @param p_owner Pointer to the instance.
 * @return TRUE if the whole record was written, otherwise FALSE with pif_error set to the first
 *         failure (E_OVERFLOW_BUFFER past the end of the region, E_ACCESS_FAILED for an erase or
 *         program that failed, E_INVALID_STATE without pifFlash_Begin()).
 */
BOOL pifFlash_End(PifFlash* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_FLASH_H
