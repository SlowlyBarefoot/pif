#include "storage/pif_flash.h"


/**
 * @fn _fail
 * @brief Keeps the first failure of a record.
 * @param p_owner Pointer to the instance.
 * @param error What failed.
 * @return FALSE, for the caller to pass on.
 */
static BOOL _fail(PifFlash* p_owner, PifError error)
{
	if (p_owner->__error == E_SUCCESS) p_owner->__error = error;
	pif_error = p_owner->__error;
	return FALSE;
}

/**
 * @fn _programUnit
 * @brief Programs the gathered unit, erasing the erase unit it falls in first if that has not been
 *        done yet.
 * @param p_owner Pointer to the instance.
 * @param offset Offset of the unit in the region.
 * @return TRUE on success, otherwise FALSE.
 */
static BOOL _programUnit(PifFlash* p_owner, uint32_t offset)
{
	if (offset + p_owner->_program_size > p_owner->_size) return _fail(p_owner, E_OVERFLOW_BUFFER);

	// The program size divides the erase size, so a unit never straddles two erase units and the
	// one it starts in is the only one that has to be erased.
	if (p_owner->_erase_size && offset >= p_owner->__next_erase) {
		if (!(*p_owner->__act_erase)(p_owner, p_owner->_base + p_owner->__next_erase)) return _fail(p_owner, E_ACCESS_FAILED);
		p_owner->__next_erase += p_owner->_erase_size;
	}

	if (!(*p_owner->__act_program)(p_owner, p_owner->_base + offset, p_owner->__unit.b, p_owner->_program_size)) {
		return _fail(p_owner, E_ACCESS_FAILED);
	}
	p_owner->__fill = 0;
	return TRUE;
}

BOOL pifFlash_Init(PifFlash* p_owner, PifId id, uint32_t base, uint32_t size, uint32_t erase_size, uint16_t program_size,
		PifActFlashErase act_erase, PifActFlashProgram act_program, PifActFlashRead act_read)
{
	if (!p_owner || !size || !program_size || program_size > PIF_FLASH_MAX_PROGRAM_SIZE || !act_program) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (erase_size) {
		if (!act_erase || erase_size % program_size || base % erase_size || size % erase_size) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}
	}
	else if (base % program_size || size % program_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifFlash));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_base = base;
	p_owner->_size = size;
	p_owner->_erase_size = erase_size;
	p_owner->_program_size = program_size;
	p_owner->_erased_value = 0xFF;
	p_owner->__act_erase = act_erase;
	p_owner->__act_program = act_program;
	p_owner->__act_read = act_read;
	return TRUE;
}

void pifFlash_AttachActLock(PifFlash* p_owner, PifActFlashLock act_lock)
{
	p_owner->__act_lock = act_lock;
}

void pifFlash_SetErasedValue(PifFlash* p_owner, uint8_t value)
{
	p_owner->_erased_value = value;
}

BOOL pifFlash_Read(PifFlash* p_owner, uint32_t offset, uint8_t* p_data, size_t size)
{
	if (!p_data || offset > p_owner->_size || size > p_owner->_size - offset) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->__act_read) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}
	if (!(*p_owner->__act_read)(p_owner, p_owner->_base + offset, p_data, size)) {
		pif_error = E_ACCESS_FAILED;
		return FALSE;
	}
	return TRUE;
}

BOOL pifFlash_Begin(PifFlash* p_owner)
{
	if (p_owner->_writing) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->_writing = TRUE;
	p_owner->_position = 0;
	p_owner->__fill = 0;
	p_owner->__next_erase = 0;
	p_owner->__error = E_SUCCESS;
	if (p_owner->__act_lock) (*p_owner->__act_lock)(p_owner, FALSE);
	return TRUE;
}

BOOL pifFlash_Write(PifFlash* p_owner, const uint8_t* p_data, size_t size)
{
	size_t len;

	if (!p_owner->_writing) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (p_owner->__error != E_SUCCESS) {
		pif_error = p_owner->__error;
		return FALSE;
	}
	if (!size) return TRUE;
	if (!p_data) return _fail(p_owner, E_INVALID_PARAM);

	while (size) {
		len = p_owner->_program_size - p_owner->__fill;
		if (len > size) len = size;

		memcpy(p_owner->__unit.b + p_owner->__fill, p_data, len);
		p_owner->__fill += len;
		p_owner->_position += len;
		p_data += len;
		size -= len;

		if (p_owner->__fill == p_owner->_program_size) {
			if (!_programUnit(p_owner, p_owner->_position - p_owner->_program_size)) return FALSE;
		}
	}
	return TRUE;
}

BOOL pifFlash_End(PifFlash* p_owner)
{
	if (!p_owner->_writing) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (p_owner->__error == E_SUCCESS && p_owner->__fill) {
		// Padded with what an erased byte reads as, and the padding is not counted as written.
		memset(p_owner->__unit.b + p_owner->__fill, p_owner->_erased_value, p_owner->_program_size - p_owner->__fill);
		_programUnit(p_owner, p_owner->_position - p_owner->__fill);
	}

	if (p_owner->__act_lock) (*p_owner->__act_lock)(p_owner, TRUE);
	p_owner->_writing = FALSE;

	if (p_owner->__error != E_SUCCESS) {
		pif_error = p_owner->__error;
		return FALSE;
	}
	return TRUE;
}
