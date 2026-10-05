// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_slew_filter.h"

// Slew-rate limiter.

void pifSlewFilter_Init(PifSlewFilter* p_owner, float max_step, float initial)
{
	p_owner->max_step = max_step;
	p_owner->_output = initial;
}

void pifSlewFilter_Reset(PifSlewFilter* p_owner, float value)
{
	p_owner->_output = value;
}

float pifSlewFilter_Apply(PifSlewFilter* p_owner, float input)
{
	float delta = input - p_owner->_output;

	if (delta > p_owner->max_step) delta = p_owner->max_step;
	else if (delta < -p_owner->max_step) delta = -p_owner->max_step;
	p_owner->_output += delta;
	return p_owner->_output;
}
