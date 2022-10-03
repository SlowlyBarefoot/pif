#ifdef __PIF_COLLECT_SIGNAL__
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_list.h"
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "core/pif_pulse.h"


#ifdef __PIF_COLLECT_SIGNAL__
	static PifDList s_cs_list;
#endif


static BOOL _calcuratePositionModulation(PifPulse* p_owner, uint16_t diff)
{
	BOOL rtn = FALSE;

	if (diff < p_owner->__threshold_1us) {
		p_owner->_channel++;
		if (p_owner->_channel < p_owner->__channel_count) {
			if (p_owner->__valid_range[3].check) {
				if (diff >= p_owner->__valid_range[3].min && diff <= p_owner->__valid_range[3].max) {
					p_owner->__p_position[p_owner->_channel] = diff;
					rtn = TRUE;
				}
			}
			else {
				p_owner->__p_position[p_owner->_channel] = diff;
				rtn = TRUE;
			}
		}
	}
	else {
		p_owner->_channel = -1;
	}
	return rtn;
}

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

#endif	// __PIF_COLLECT_SIGNAL__

BOOL pifPulse_Init(PifPulse* p_owner, PifId id)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

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

#ifdef __PIF_COLLECT_SIGNAL__
fail:
	pifPulse_Clear(p_owner);
    return FALSE;
#endif
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
#else
	(void)p_owner;
#endif
}

BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode)
{
	uint8_t mask = 0;

	if (measure_mode & PIF_PMM_EDGE_MASK) mask |= 1;
	if (measure_mode & PIF_PMM_TICK_MASK) mask |= 2;
	if (mask == 3) {
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

BOOL pifPulse_SetPositionMode(PifPulse* p_owner, uint8_t channel_count, uint16_t threshold_1us, uint16_t* p_value)
{
    if (p_owner->_measure_mode & PIF_PMM_EDGE_MASK) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
    }

	p_owner->__channel_count = channel_count;
    p_owner->__threshold_1us = threshold_1us;
    p_owner->__p_position = p_value;
    p_owner->_measure_mode |= PIF_PMM_TICK_POSITION;
    return TRUE;
}

BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint32_t min, uint32_t max)
{
	int index = -1;

	switch (measure_mode) {
	case PIF_PMM_COMMON_PERIOD:
		index = 0;
		break;

	case PIF_PMM_EDGE_LOW_WIDTH:
		index = 1;
		break;

	case PIF_PMM_EDGE_HIGH_WIDTH:
		index = 2;
		break;

	case PIF_PMM_TICK_POSITION:
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
	p_owner->falling_count = 0UL;

	memset(p_owner->__data, 0, sizeof(p_owner->__data));
	p_owner->__ptr = 0;
	p_owner->__last_ptr = 0;
	p_owner->__count = 0;
}

uint16_t pifPulse_GetPeriod(PifPulse* p_owner)
{
	uint8_t prev;
	uint16_t value = 0;

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

uint16_t pifPulse_GetLowWidth(PifPulse* p_owner)
{
	uint8_t prev;
	uint16_t value = 0;

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

uint16_t pifPulse_GetHighWidth(PifPulse* p_owner)
{
	uint16_t value = 0;

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

uint8_t pifPulse_sigEdge(PifPulse* p_owner, PifPulseEdge edge, uint32_t time_us)
{
	uint8_t rtn = 0;

	if (edge == PE_RISING) {
		p_owner->__data[p_owner->__ptr].rising = time_us;
	}
	else {
		p_owner->__data[p_owner->__ptr].falling = time_us;
		if (p_owner->_measure_mode & PIF_PMM_COMMON_COUNT) {
			p_owner->falling_count++;
		}
		p_owner->__last_ptr = p_owner->__ptr;
		p_owner->__ptr = (p_owner->__ptr + 1) & PIF_PULSE_DATA_MASK;
	}
	if (p_owner->__count < PIF_PULSE_DATA_SIZE) p_owner->__count++;

	if (p_owner->__evt.edge) {
		(*p_owner->__evt.edge)(edge, p_owner->__p_issuer);
	}

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & PL_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[PL_CSF_STATE_IDX], edge);
	}
#endif

	return rtn;
}

uint8_t pifPulse_sigTick(PifPulse* p_owner, uint32_t time_us)
{
	uint8_t rtn = 0;

	p_owner->__data[p_owner->__ptr].falling = time_us;
	if (p_owner->_measure_mode & PIF_PMM_COMMON_COUNT) {
		p_owner->falling_count++;
	}
	if (p_owner->_measure_mode & PIF_PMM_TICK_POSITION) {
		rtn |= _calcuratePositionModulation(p_owner, p_owner->__data[p_owner->__ptr].falling - p_owner->__data[p_owner->__last_ptr].falling) << 4;
	}
	p_owner->__last_ptr = p_owner->__ptr;
	p_owner->__ptr = (p_owner->__ptr + 1) & PIF_PULSE_DATA_MASK;

	if (p_owner->__count < PIF_PULSE_DATA_SIZE) p_owner->__count++;

	if (p_owner->__evt.tick) {
		(*p_owner->__evt.tick)(p_owner->__p_issuer);
	}

	return rtn;
}

void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, PifIssueP p_issuer)
{
	p_owner->__evt.edge = evt_edge;
	p_owner->__p_issuer = p_issuer;
}

void pifPulse_AttachEvtTick(PifPulse* p_owner, PifEvtPulseTick evt_tick, PifIssueP p_issuer)
{
	p_owner->__evt.tick = evt_tick;
	p_owner->__p_issuer = p_issuer;
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

#endif	// __PIF_COLLECT_SIGNAL__
