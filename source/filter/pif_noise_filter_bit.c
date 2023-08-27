#include "filter/pif_noise_filter_bit.h"


static PifNfBit* _addMethod(PifNoiseFilter* p_parent, uint8_t size)
{
	PifNfBit* p_method;

	if (!p_parent || size < 3 || size >= 32) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	if (p_parent->_last >= p_parent->_count) {
		pif_error = E_OVERFLOW_BUFFER;
		return NULL;
	}

	p_parent->__p_method[p_parent->_last] = calloc(1, sizeof(PifNfBit));
	if (!p_parent->__p_method[p_parent->_last]) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	p_method = (PifNfBit*)p_parent->__p_method[p_parent->_last];

	p_method->_size = size;
	p_method->__half = size / 2;
	p_method->__msb = 1L << (size - 1);
	p_method->__count = 0;
	p_method->__list = 0L;

	p_parent->_last++;
    return p_method;
}

static void _resetMethod(PifNoiseFilterMethod* p_method)
{
	PifNfBit* p_owner = (PifNfBit*)p_method;

	p_owner->__count = 0;
	p_owner->__list = 0L;
}

static PifNoiseFilterValueP _processCount(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value)
{
	PifNfBit* p_owner = (PifNfBit*)p_method;

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

static PifNoiseFilterValueP _processContinue(PifNoiseFilterMethod* p_method, PifNoiseFilterValueP p_value)
{
	PifNfBit* p_owner = (PifNfBit*)p_method;
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

BOOL pifNoiseFilterBit_AddCount(PifNoiseFilter* p_parent, uint8_t size)
{
	PifNfBit* p_owner = _addMethod(p_parent, size);

	if (!p_owner) return FALSE;

	p_owner->parent.type = NFT_BIT_COUNT;
	p_owner->parent.fn_reset = _resetMethod;
	p_owner->parent.fn_process = _processCount;
    return TRUE;
}

BOOL pifNoiseFilterBit_AddContinue(PifNoiseFilter* p_parent, uint8_t size)
{
	PifNfBit* p_owner = _addMethod(p_parent, size);

	if (!p_owner) return FALSE;

	p_owner->parent.type = NFT_BIT_CONTINUE;
	p_owner->parent.fn_reset = _resetMethod;
	p_owner->parent.fn_process = _processContinue;
	return TRUE;
}
