#include "filter/pif_noise_filter.h"


#ifdef PIF_NO_USE_INLINE

void pifNoiseFilter_Reset(PifNoiseFilter* p_parent)
{
	(*p_parent->__fn_reset)(p_parent);
}

PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	return (*p_parent->__fn_process)(p_parent, p_value);
}

#endif
