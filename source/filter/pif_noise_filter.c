#include "filter/pif_noise_filter.h"


BOOL pifNoiseFilter_Init(PifNoiseFilter* p_owner, uint8_t count)
{
	if (!p_owner || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilter));

	p_owner->__p_method = calloc(count, sizeof(PifNoiseFilterMethod*));
	if (!p_owner->__p_method) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

    p_owner->_count = count;
    return TRUE;
}

void pifNoiseFilter_Clear(PifNoiseFilter* p_owner)
{
	int i;
	PifNoiseFilterMethod* p_method;

	if (!p_owner->__p_method) return;

	for (i = 0; i < p_owner->_count; i++) {
		p_method = p_owner->__p_method[i];
		if (p_method) {
			if (p_method->fn_clear) (*p_method->fn_clear)(p_method);
		}
	}
}

#ifdef __PIF_NO_USE_INLINE__

void pifNoiseFilter_Reset(PifNoiseFilter* p_owner, uint8_t index)
{
	(*p_owner->__p_method[index]->fn_reset)(p_owner->__p_method[index]);
}

PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, uint8_t index, PifNoiseFilterValueP p_value)
{
	return (*p_owner->__p_method[index]->fn_process)(p_owner->__p_method[index], p_value);
}

#endif
