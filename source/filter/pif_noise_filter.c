#include "filter/pif_noise_filter.h"


PIF_INLINE void pifNoiseFilter_Reset(PifNoiseFilter* p_parent)
{
	(*p_parent->__fn_reset)(p_parent);
}

PIF_INLINE PifNoiseFilterValueP pifNoiseFilter_Process(PifNoiseFilter* p_parent, PifNoiseFilterValueP p_value)
{
	return (*p_parent->__fn_process)(p_parent, p_value);
}
