#ifdef __PIF_COLLECT_SIGNAL__
	#include "pif_collect_signal.h"
#endif
#include "pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_pulse.h"


#ifdef __PIF_COLLECT_SIGNAL__
	static PifDList s_cs_list;
#endif


#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char* prefix[PL_CSF_COUNT] = { "PL" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifPulseColSig* p_colsig = (PifPulseColSig*)it->data;
		PifPulse* p_owner = p_colsig->p_owner;
		for (int f = 0; f < PL_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_WIRE, 1,
						prefix[f], PE_FALLING);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "PL_CS:Add(DC:%u)", p_owner->_id);
#endif

		it = pifDList_Next(it);
	}
}

#endif

BOOL pifPulse_Init(PifPulse* p_owner, PifId id)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

	p_owner->__p_data = calloc(sizeof(PifPulseData), PIF_PULSE_DATA_SIZE);
    if (!p_owner->__p_data) {
        pif_error = E_OUT_OF_HEAP;
		goto fail;
    }
    p_owner->__data_size = PIF_PULSE_DATA_SIZE;
    p_owner->__data_mask = PIF_PULSE_DATA_SIZE - 1;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

#ifdef __PIF_COLLECT_SIGNAL__
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_PULSE, _addDeviceInCollectSignal);
	}
	PifPulseColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifPulseColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return TRUE;

fail:
	pifPulse_Clear(p_owner);
    return FALSE;
}

void pifPulse_Clear(PifPulse* p_owner)
{
#ifdef __PIF_COLLECT_SIGNAL__
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifPulseColSig* p_colsig = (PifPulseColSig*)it->data;
		if (p_colsig == p_owner->__p_colsig) {
			pifDList_Remove(&s_cs_list, it);
			break;
		}
		it = pifDList_Next(it);
	}
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_PULSE);
	}
	p_owner->__p_colsig = NULL;
#endif
	if (p_owner->__p_data) {
		free(p_owner->__p_data);
		p_owner->__p_data = NULL;
	}
}

void pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	p_owner->_measure_mode |= measure_mode;
}

void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	p_owner->_measure_mode &= ~measure_mode;
}

BOOL pifPulse_SetPositionModulation(PifPulse* p_owner, uint8_t channels, uint16_t threshold_1us)
{
	uint8_t size;
	PifPulseData* p_tmp;

	size = PIF_PULSE_DATA_SIZE;
	while (size < PIF_PULSE_DATA_SIZE + channels) {
		size <<= 1;
	}
	p_tmp = calloc(sizeof(PifPulseData), size);
    if (!p_tmp) {
        pif_error = E_OUT_OF_HEAP;
        return FALSE;
    }

    free(p_owner->__p_data);
    p_owner->__p_data = p_tmp;
    p_owner->__data_size = size;
    p_owner->__data_mask = size - 1;
    p_owner->__channels = channels;
    p_owner->__threshold_1us = threshold_1us;
    p_owner->_measure_mode |= PIF_PMM_POSITION;
    return TRUE;
}

BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint32_t min, uint32_t max)
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

	case PIF_PMM_POSITION:
		index = 3;
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
	p_owner->rising_count = 0UL;
	p_owner->falling_count = 0UL;

	memset(p_owner->__p_data, 0, sizeof(PifPulseData) * p_owner->__data_size);
	p_owner->__ptr = 0;
	p_owner->__count = 0;
}

uint16_t pifPulse_GetPeriod(PifPulse* p_owner)
{
	uint8_t curr, prev;
	uint16_t value = 0;

	if (p_owner->__count < p_owner->__data_size) return 0;

	curr = (p_owner->__ptr + p_owner->__data_mask) & p_owner->__data_mask;
	prev = (curr + p_owner->__data_mask) & p_owner->__data_mask;
	value = p_owner->__p_data[curr].falling - p_owner->__p_data[prev].falling;
	if (p_owner->__valid_range[0].check) {
		if (value >= p_owner->__valid_range[0].min && value <= p_owner->__valid_range[0].max) {
			return value;
		}
	}
	return value;
}

uint16_t pifPulse_GetLowWidth(PifPulse* p_owner)
{
	uint8_t curr, prev;
	uint16_t value = 0;

	if (p_owner->__count < p_owner->__data_size) return 0;

	curr = (p_owner->__ptr + p_owner->__data_mask) & p_owner->__data_mask;
	prev = (curr + p_owner->__data_mask) & p_owner->__data_mask;
	value = p_owner->__p_data[curr].rising - p_owner->__p_data[prev].falling;
	if (p_owner->__valid_range[1].check) {
		if (value >= p_owner->__valid_range[1].min && value <= p_owner->__valid_range[1].max) {
			return value;
		}
	}
	return value;
}

uint16_t pifPulse_GetHighWidth(PifPulse* p_owner)
{
	uint8_t curr;
	uint16_t value = 0;

	if (p_owner->__count < p_owner->__data_size) return 0;

	curr = (p_owner->__ptr + p_owner->__data_mask) & p_owner->__data_mask;
	value = p_owner->__p_data[curr].falling - p_owner->__p_data[curr].rising;
	if (p_owner->__valid_range[2].check) {
		if (value >= p_owner->__valid_range[2].min && value <= p_owner->__valid_range[2].max) {
			return value;
		}
	}
	return value;
}

BOOL pifPulse_GetPositionModulation(PifPulse* p_owner, uint16_t* p_value)
{
	uint8_t i, curr, next, count;
	uint16_t value = 0;

	if (p_owner->__count < p_owner->__data_size) return FALSE;

	count = 0;
	curr = (p_owner->__ptr + 1) & p_owner->__data_mask;
	while (1) {
		next = (curr + 1) & p_owner->__data_mask;
		value = p_owner->__p_data[next].falling - p_owner->__p_data[curr].falling;
		curr = next;
		if (value > p_owner->__threshold_1us) break;
		count++;
	}
	if (count >= p_owner->__channels) {
		curr = (p_owner->__ptr + 1) & p_owner->__data_mask;
	}

	for (i = 0; i < p_owner->__channels; i++) {
		next = (curr + 1) & p_owner->__data_mask;
		value = p_owner->__p_data[next].falling - p_owner->__p_data[curr].falling;
		if (p_owner->__valid_range[3].check) {
			if (value >= p_owner->__valid_range[3].min && value <= p_owner->__valid_range[3].max) {
				p_value[i] = value;
			}
		}
		else {
			p_value[i] = value;
		}
		curr = next;
	}
	return value;
}

void pifPulse_sigEdgeTime(PifPulse* p_owner, PifPulseEdge edge, uint32_t time_us)
{
	if (edge == PE_RISING) {
		p_owner->__p_data[p_owner->__ptr].rising = time_us;
		if (p_owner->_measure_mode & PIF_PMM_RISING_COUNT) {
			p_owner->rising_count++;
		}
	}
	else {
		p_owner->__p_data[p_owner->__ptr].falling = time_us;
		p_owner->__ptr = (p_owner->__ptr + 1) & p_owner->__data_mask;
		if (p_owner->_measure_mode & PIF_PMM_FALLING_COUNT) {
			p_owner->falling_count++;
		}
	}
	if (p_owner->__count < p_owner->__data_size) p_owner->__count++;

	if (p_owner->__evt_edge) {
		(*p_owner->__evt_edge)(edge, p_owner->__p_edge_issuer);
	}

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & PL_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[PL_CSF_STATE_IDX], edge);
	}
#endif
}

void pifPulse_sigEdge(PifPulse* p_owner, PifPulseEdge edge)
{
	if (pif_act_timer1us) {
		pifPulse_sigEdgeTime(p_owner, edge, (*pif_act_timer1us)());
	}
	else {
		pifPulse_sigEdgeTime(p_owner, edge, pif_cumulative_timer1ms * 1000);
	}
}

void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, void* p_issuer)
{
	p_owner->__evt_edge = evt_edge;
	p_owner->__p_edge_issuer = p_issuer;
}

#ifdef __PIF_COLLECT_SIGNAL__

void pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifPulseColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifPulseColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifPulseColSig_SetFlag(PifPulseCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifPulseColSig* p_colsig = (PifPulseColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifPulseColSig_ResetFlag(PifPulseCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifPulseColSig* p_colsig = (PifPulseColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif
