#include "filter/pif_noise_filter.h"


BOOL pifNoiseFilter_Init(PifNoiseFilter* p_owner, PifNoiseFilterType type)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifNoiseFilter));

    p_owner->_type = type;
    return TRUE;
}

#ifdef __PIF_NO_USE_INLINE__

PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_owner, PifNoiseFilterValueP p_value)
{
	return (*p_owner->__fn_process)(p_owner, p_value);
}

#endif
