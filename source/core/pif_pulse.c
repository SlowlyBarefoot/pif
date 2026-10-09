// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_pulse.h"

// Pulse input abstraction with edge tracking and optional signal collection.

BOOL pifPulse_Init(PifPulse* p_owner, PifId id)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    return TRUE;
}

void pifPulse_Clear(PifPulse* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifPulse_ResetCsFlag(p_owner, PL_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	if (measure_mode == 0 || measure_mode > 0x0F) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->_measure_mode |= measure_mode;
	return TRUE;
}

void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	p_owner->_measure_mode &= ~measure_mode;
}

BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint16_t min, uint16_t max)
{
	int index = -1;

	switch (measure_mode) {
	case PIF_PMM_PERIOD:
		index = 0;
		break;

	case PIF_PMM_LOW_WIDTH:
		index = 1;
		break;

	case PIF_PMM_HIGH_WIDTH:
		index = 2;
		break;
	}
	if (index < 0) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	p_owner->__valid_range[index].check = TRUE;
	p_owner->__valid_range[index].min = min;
	p_owner->__valid_range[index].max = max;
	return TRUE;
}

void pifPulse_ResetMeasureValue(PifPulse* p_owner)
{
	p_owner->falling_count = 0UL;

	memset(p_owner->__data, 0, sizeof(p_owner->__data));
	p_owner->__ptr = 0;
	p_owner->__last_ptr = 0;
	p_owner->__count = 0;
}

uint32_t pifPulse_GetPeriod(PifPulse* p_owner)
{
	uint8_t prev;
	uint32_t value = 0;

	if (p_owner->__count < PIF_PULSE_DATA_SIZE) return 0;

	prev = (p_owner->__last_ptr + PIF_PULSE_DATA_MASK) & PIF_PULSE_DATA_MASK;
	value = p_owner->__data[p_owner->__last_ptr].falling - p_owner->__data[prev].falling;
	if (p_owner->__valid_range[0].check) {
		if (value >= p_owner->__valid_range[0].min && value <= p_owner->__valid_range[0].max) {
			return value;
		}
		else value = 0;
	}
	return value;
}

uint32_t pifPulse_GetLowWidth(PifPulse* p_owner)
{
	uint8_t prev;
	uint32_t value = 0;

	if (p_owner->__count < PIF_PULSE_DATA_SIZE) return 0;

	prev = (p_owner->__last_ptr + PIF_PULSE_DATA_MASK) & PIF_PULSE_DATA_MASK;
	value = p_owner->__data[p_owner->__last_ptr].rising - p_owner->__data[prev].falling;
	if (p_owner->__valid_range[1].check) {
		if (value >= p_owner->__valid_range[1].min && value <= p_owner->__valid_range[1].max) {
			return value;
		}
		else value = 0;
	}
	return value;
}

uint32_t pifPulse_GetHighWidth(PifPulse* p_owner)
{
	uint32_t value = 0;

	if (p_owner->__count < PIF_PULSE_DATA_SIZE) return 0;

	value = p_owner->__data[p_owner->__last_ptr].falling - p_owner->__data[p_owner->__last_ptr].rising;
	if (p_owner->__valid_range[2].check) {
		if (value >= p_owner->__valid_range[2].min && value <= p_owner->__valid_range[2].max) {
			return value;
		}
		else value = 0;
	}
	return value;
}

BOOL pifPulse_sigEdge(PifPulse* p_owner, PifPulseState state, uint32_t time_us)
{
	BOOL rtn = FALSE;

	if (state == PS_RISING_EDGE) {
		p_owner->__data[p_owner->__ptr].rising = time_us;
	}
	else {
		p_owner->__data[p_owner->__ptr].falling = time_us;
		if (p_owner->_measure_mode & PIF_PMM_COUNT) {
			p_owner->falling_count++;
		}
		p_owner->__last_ptr = p_owner->__ptr;
		p_owner->__ptr = (p_owner->__ptr + 1) & PIF_PULSE_DATA_MASK;

		if (p_owner->__evt_edge) {
			(*p_owner->__evt_edge)(state, p_owner->__p_issuer);
		}
		rtn = TRUE;
	}
	if (p_owner->__count < PIF_PULSE_DATA_SIZE) p_owner->__count++;

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[PL_CSF_STATE_IDX], state == PS_RISING_EDGE);
#endif

	return rtn;
}

void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, PifIssuerP p_issuer)
{
	p_owner->__evt_edge = evt_edge;
	p_owner->__p_issuer = p_issuer;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	if (flag & PL_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[PL_CSF_STATE_IDX], "PL", p_owner->_id, CSVT_WIRE, 1,
				PS_LOW_LEVEL)) return FALSE;
	}
	return TRUE;
}

void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	if (flag & PL_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[PL_CSF_STATE_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
