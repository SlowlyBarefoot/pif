#include "filter/pif_noise_filter_manager.h"


BOOL pifNoiseFilterManager_Init(PifNoiseFilterManager* p_owner, uint8_t count)
{
	if (!p_owner || !count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilterManager));

	p_owner->__p_list = calloc(count, sizeof(PifNoiseFilter*));
	if (!p_owner->__p_list) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

    p_owner->_count = count;
    return TRUE;
}

void pifNoiseFilterManager_Clear(PifNoiseFilterManager* p_owner)
{
	int i;
	PifNoiseFilter* p_filter;

	if (!p_owner->__p_list) return;

	for (i = 0; i < p_owner->_count; i++) {
		p_filter = p_owner->__p_list[i];
		if (p_filter) {
			if (p_filter->__fn_clear) (*p_filter->__fn_clear)(p_filter);
		}
	}
}
