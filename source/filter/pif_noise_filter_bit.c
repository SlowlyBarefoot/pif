// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_noise_filter_bit.h"


/**
 * @fn _reset
 * @brief Clears the runtime history state of a bit filter.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _reset(PifNoiseFilter* p_parent)
{
	PifNfBit* p_owner = (PifNfBit*)p_parent;

	p_parent->_ready = FALSE;
	p_owner->_result = OFF;
	p_owner->__len = 0;
	p_owner->__count = 0;
	p_owner->__list = 0UL;
}

/**
 * @fn _shiftIn
 * @brief Drops the oldest bit of the window and adds a new one.
 * @param p_owner Pointer to the bit filter.
 * @param state New bit.
 */
static void _shiftIn(PifNfBit* p_owner, SWITCH state)
{
	if (p_owner->__list & p_owner->__msb) {
		p_owner->__list &= ~p_owner->__msb;
		p_owner->__count--;
	}
	p_owner->__list <<= 1;
	if (state) {
		p_owner->__list |= 1;
		p_owner->__count++;
	}
	if (p_owner->__len < p_owner->_size && ++p_owner->__len == p_owner->_size) p_owner->parent._ready = TRUE;
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

	_shiftIn(p_owner, *(SWITCH*)p_value ? ON : OFF);
	p_owner->_result = p_owner->__count > p_owner->__half;
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

	state = *(SWITCH*)p_value ? ON : OFF;
	sw = p_owner->__list & 1;
	if (sw != state) {
		count = 1;
		mask = 1UL;
		for (i = 1; i < p_owner->_size; i++) {
			if (((p_owner->__list >> i) & 1) != sw) break;
			count++;
			mask |= 1UL << i;
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
	_shiftIn(p_owner, state);
	p_owner->_result = (p_owner->__list >> p_owner->__half) & 1;
	return &p_owner->_result;
}

/**
 * @fn _init
 * @brief Initializes a bit filter of either mode.
 * @param p_owner Pointer to the bit filter.
 * @param size Bit history length used by the filter.
 * @param type Filter type to set.
 * @param fn_process Process function of the filter type.
 * @return TRUE on success, FALSE otherwise.
 */
static BOOL _init(PifNfBit* p_owner, uint8_t size, PifNoiseFilterType type, PifNoiseFilter_Process fn_process)
{
	if (!p_owner || size < 3 || size >= 32) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifNfBit));
	p_owner->parent._type = type;
	p_owner->parent._value = NFV_SWITCH;
	p_owner->parent.__fn_reset = _reset;
	p_owner->parent.__fn_process = fn_process;
	p_owner->_size = size;
	p_owner->__half = size / 2;
	p_owner->__msb = 1UL << (size - 1);
	_reset(&p_owner->parent);
	return TRUE;
}

/**
 * @fn _resetIntegrator
 * @brief Returns an integrating debounce filter to OFF.
 * @param p_parent Pointer to the base noise filter object.
 */
static void _resetIntegrator(PifNoiseFilter* p_parent)
{
	PifNfBitIntegrator* p_owner = (PifNfBitIntegrator*)p_parent;

	p_parent->_ready = FALSE;
	p_owner->_result = OFF;
	p_owner->__count = 0;
}

/**
 * @fn _processIntegrator
 * @brief Counts a sample and changes the output when the count reaches its end.
 * @param p_parent Pointer to the base noise filter object.
 * @param p_value Pointer to the latest SWITCH input sample.
 * @return Pointer to the debounced SWITCH result.
 */
static PifNoiseFilterValueP _processIntegrator(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNfBitIntegrator* p_owner = (PifNfBitIntegrator*)p_parent;
	SWITCH state = *(SWITCH*)p_value ? ON : OFF;

	p_parent->_ready = TRUE;
	if (!p_owner->_result) {
		if (state) {
			if (++p_owner->__count >= p_owner->_on_count) {
				p_owner->_result = ON;
				p_owner->__count = p_owner->_off_count;
			}
		}
		else if (p_owner->__count) {
			p_owner->__count--;
		}
	}
	else {
		if (!state) {
			if (--p_owner->__count == 0) p_owner->_result = OFF;
		}
		else if (p_owner->__count < p_owner->_off_count) {
			p_owner->__count++;
		}
	}
	return &p_owner->_result;
}

BOOL pifNoiseFilterBit_InitIntegrator(PifNfBitIntegrator* p_owner, uint16_t on_count, uint16_t off_count)
{
	if (!p_owner || !on_count || !off_count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifNfBitIntegrator));
	p_owner->parent._type = NFT_BIT_INTEGRATOR;
	p_owner->parent._value = NFV_SWITCH;
	p_owner->parent.__fn_reset = _resetIntegrator;
	p_owner->parent.__fn_process = _processIntegrator;
	p_owner->_on_count = on_count;
	p_owner->_off_count = off_count;
	return TRUE;
}

BOOL pifNoiseFilterBit_InitCount(PifNfBit* p_owner, uint8_t size)
{
	return _init(p_owner, size, NFT_BIT_COUNT, _processCount);
}

BOOL pifNoiseFilterBit_InitContinue(PifNfBit* p_owner, uint8_t size)
{
	return _init(p_owner, size, NFT_BIT_CONTINUE, _processContinue);
}
