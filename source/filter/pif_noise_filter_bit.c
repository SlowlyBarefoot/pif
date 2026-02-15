#include "filter/pif_noise_filter_bit.h"


/**
 * @fn _addMethod
 * @brief Allocates and registers a bit filter instance in the manager.
 * @param p_manager Pointer to the filter manager.
 * @param size Bit history length used by the filter.
 * @return Pointer to the allocated bit filter object, or NULL on failure.
 */
static PifNfBit* _addMethod(PifNoiseFilterManager* p_manager, uint8_t size)
{
	PifNfBit* p_owner;

	if (!p_manager || size < 3 || size >= 32) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	p_owner = calloc(1, sizeof(PifNfBit));
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	PifPtrArrayIterator it = pifPtrArray_Add(&p_manager->__filters, p_owner);
	if (!it) goto fail;

	p_owner->_size = size;
	p_owner->__half = size / 2;
	p_owner->__msb = 1L << (size - 1);
	p_owner->__count = 0;
	p_owner->__list = 0L;
    return p_owner;

fail:
	if (p_owner) free(p_owner);
	return NULL;
}

/**
 * @fn _resetMethod
 * @brief Clears the runtime history state of a bit filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetMethod(PifNoiseFilter* p_parent)
{
	PifNfBit* p_owner = (PifNfBit*)p_parent;

	p_owner->__count = 0;
	p_owner->__list = 0L;
}

/**
 * @fn _processCount
 * @brief Updates the output using majority counting over the bit window.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest SWITCH input sample.
 * @return Pointer to the debounced SWITCH result.
 */
static PifNoiseFilterValueP _processCount(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfBit* p_owner = (PifNfBit*)p_parent;

    if (p_owner->__list & p_owner->__msb) {
    	p_owner->__list &= ~p_owner->__msb;
    	p_owner->__count--;
    }
    p_owner->__list <<= 1;
    if (*(SWITCH*)p_value) {
    	p_owner->__list |= 1;
    	p_owner->__count++;
    }
	p_owner->_result = p_owner->__count >= p_owner->__half;
    return &p_owner->_result;
}

/**
 * @fn _processContinue
 * @brief Suppresses short state glitches before updating the bit output.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest SWITCH input sample.
 * @return Pointer to the debounced SWITCH result.
 */
static PifNoiseFilterValueP _processContinue(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfBit* p_owner = (PifNfBit*)p_parent;
	int i, count;
	SWITCH state, sw;
	uint32_t mask;

	state = *(SWITCH*)p_value;
	sw = p_owner->__list & 1;
	if (sw != state) {
		count = 1;
		mask = 1L;
		for (i = 1; i < p_owner->_size; i++) {
			if (((p_owner->__list >> i) & 1) != sw) break;
			count++;
			mask |= 1L << i;
	    }
		if (count <= p_owner->__half) {
			if (sw) {
				p_owner->__list &= ~mask;
				p_owner->__count -= count;
			}
			else {
				p_owner->__list |= mask;
				p_owner->__count += count;
			}
		}
	}
    if (p_owner->__list & p_owner->__msb) {
    	p_owner->__list &= ~p_owner->__msb;
    	p_owner->__count--;
    }
    p_owner->__list <<= 1;
	if (state) {
		p_owner->__list |= 1;
		p_owner->__count++;
	}
	p_owner->_result = (p_owner->__list >> p_owner->__half) & 1;
    return &p_owner->_result;
}

PifNoiseFilter* pifNoiseFilterBit_AddCount(PifNoiseFilterManager* p_manager, uint8_t size)
{
	PifNfBit* p_owner = _addMethod(p_manager, size);

	if (!p_owner) return NULL;

	p_owner->parent._type = NFT_BIT_COUNT;
	p_owner->parent.__fn_reset = _resetMethod;
	p_owner->parent.__fn_process = _processCount;
    return (PifNoiseFilter*)p_owner;
}

PifNoiseFilter* pifNoiseFilterBit_AddContinue(PifNoiseFilterManager* p_manager, uint8_t size)
{
	PifNfBit* p_owner = _addMethod(p_manager, size);

	if (!p_owner) return NULL;

	p_owner->parent._type = NFT_BIT_CONTINUE;
	p_owner->parent.__fn_reset = _resetMethod;
	p_owner->parent.__fn_process = _processContinue;
	return (PifNoiseFilter*)p_owner;
}
