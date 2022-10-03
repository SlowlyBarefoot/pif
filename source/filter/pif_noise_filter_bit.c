#include "filter/pif_noise_filter_bit.h"


static PifNoiseFilterValueP _processCount(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNoiseFilterBit* p_owner = (PifNoiseFilterBit*)p_parent;

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

static PifNoiseFilterValueP _processContinue(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	PifNoiseFilterBit* p_owner = (PifNoiseFilterBit*)p_parent;
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

BOOL pifNoiseFilterBit_Init(PifNoiseFilterBit* p_owner, uint8_t size)
{
	if (!p_owner || size < 3 || size >= 32) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilterBit));

	pifNoiseFilter_Init(&p_owner->parent, NFT_BIT_COUNT);
    p_owner->_size = size;
    p_owner->__half = size / 2;
    p_owner->__msb = 1L << (size - 1);
    p_owner->__count = 0;
    p_owner->__list = 0L;

	p_owner->parent.__fn_process = _processCount;
    return TRUE;
}

BOOL pifNoiseFilterBit_SetContinue(PifNoiseFilterBit* p_owner, uint8_t size)
{
	if (size != p_owner->_size) {
		if (size < 3 || size >= 32) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}

		p_owner->_size = size;
		p_owner->__half = size / 2;
		p_owner->__msb = 1L << (size - 1);
		p_owner->__count = 0;
		p_owner->__list = 0L;
	}

	p_owner->parent._type = NFT_BIT_CONTINUE;
	p_owner->parent.__fn_process = _processContinue;
	return TRUE;
}
