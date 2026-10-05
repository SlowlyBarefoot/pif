// SPDX-License-Identifier: BSD-3-Clause
#include "filter/pif_moving_average.h"

// Moving average with a running sum over a ring of samples.

BOOL pifMovingAverage_Init(PifMovingAverage* p_owner, float* p_buffer, uint16_t size)
{
	if (!p_buffer || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_buffer = p_buffer;
	p_owner->_size = size;
	pifMovingAverage_Reset(p_owner);
	return TRUE;
}

void pifMovingAverage_Reset(PifMovingAverage* p_owner)
{
	p_owner->_count = 0;
	p_owner->__index = 0;
	p_owner->__sum = 0.0f;
}

float pifMovingAverage_Apply(PifMovingAverage* p_owner, float input)
{
	uint16_t i;

	if (p_owner->_count < p_owner->_size) {
		p_owner->_count++;
	}
	else {
		p_owner->__sum -= p_owner->__p_buffer[p_owner->__index];
	}
	p_owner->__p_buffer[p_owner->__index] = input;
	p_owner->__sum += input;

	p_owner->__index++;
	if (p_owner->__index >= p_owner->_size) {
		p_owner->__index = 0;

		// Once per window, drop the rounding error the add/subtract pairs left behind.
		p_owner->__sum = 0.0f;
		for (i = 0; i < p_owner->_count; i++) {
			p_owner->__sum += p_owner->__p_buffer[i];
		}
	}
	return p_owner->__sum / p_owner->_count;
}
