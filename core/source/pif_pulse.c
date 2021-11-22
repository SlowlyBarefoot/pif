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

static uint16_t _doTask(PifTask* p_task)
{
	PifPulse* p_owner = (PifPulse*)p_task->_p_client;
	int i;
	uint8_t curr, next, count;
	uint32_t sum;

	if (p_owner->__trigger_edge == PE_UNKNOWN) return 0;

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & PL_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[PL_CSF_STATE_IDX], p_owner->__trigger_edge - 1);
	}
#endif

	if (p_owner->_measure_mode & PIF_PMM_PERIOD) {
		sum = 0UL;
		count = 0;
		curr = p_owner->__ptr;
		for (i = 0; i < PIF_PULSE_DATA_SIZE - 2;) {
			next = (curr + 2) & PIF_PULSE_DATA_MASK;
			if (p_owner->__data[curr].edge == PE_FALLING && p_owner->__data[next].edge == PE_FALLING) {
				sum += p_owner->__data[next].timer1us - p_owner->__data[curr].timer1us;
				count++;
				curr = (curr + 2) & PIF_PULSE_DATA_MASK;
				i += 2;
			}
			else {
				curr = (curr + 1) & PIF_PULSE_DATA_MASK;
				i++;
			}
		}
		if (count) {
			p_owner->_period_1us = sum / count;
		}
	}

	if (p_owner->_measure_mode & PIF_PMM_LOW_LEVEL_TIME) {
		sum = 0UL;
		count = 0;
		curr = p_owner->__ptr;
		for (i = 0; i < PIF_PULSE_DATA_SIZE - 1;) {
			next = (curr + 1) & PIF_PULSE_DATA_MASK;
			if (p_owner->__data[curr].edge == PE_FALLING && p_owner->__data[next].edge == PE_RISING) {
				sum += p_owner->__data[next].timer1us - p_owner->__data[curr].timer1us;
				count++;
				curr = (curr + 2) & PIF_PULSE_DATA_MASK;
				i += 2;
			}
			else {
				curr = (curr + 1) & PIF_PULSE_DATA_MASK;
				i++;
			}
		}
		if (count) {
			p_owner->_low_level_1us = sum / count;
		}
	}

	if (p_owner->_measure_mode & PIF_PMM_HIGH_LEVEL_TIME) {
		sum = 0UL;
		count = 0;
		curr = p_owner->__ptr;
		for (i = 0; i < PIF_PULSE_DATA_SIZE - 1;) {
			next = (curr + 1) & PIF_PULSE_DATA_MASK;
			if (p_owner->__data[curr].edge == PE_RISING && p_owner->__data[next].edge == PE_FALLING) {
				sum += p_owner->__data[next].timer1us - p_owner->__data[curr].timer1us;
				count++;
				curr = (curr + 2) & PIF_PULSE_DATA_MASK;
				i += 2;
			}
			else {
				curr = (curr + 1) & PIF_PULSE_DATA_MASK;
				i++;
			}
		}
		if (count) {
			p_owner->_high_level_1us = sum / count;
		}
	}

	if (p_owner->__evt_edge) {
		(*p_owner->__evt_edge)(p_owner->__trigger_edge, p_owner->__p_edge_issuer);
	}

	p_owner->__trigger_edge = PE_UNKNOWN;
	return 0;
}

BOOL pifPulse_Init(PifPulse* p_owner, PifId id)
{
    if (!p_owner) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
    }

	memset(p_owner, 0, sizeof(PifPulse));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

    p_owner->__p_task = pifTaskManager_Add(TM_RATIO, 100, _doTask, p_owner, TRUE);
    if (!p_owner->__p_task) goto fail;

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
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
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

void pifPulse_ResetMeasureValue(PifPulse* p_owner)
{
	p_owner->rising_count = 0UL;
	p_owner->falling_count = 0UL;
	p_owner->_period_1us = 0UL;
	p_owner->_low_level_1us = 0UL;
	p_owner->_high_level_1us = 0UL;

	p_owner->__trigger_edge = PE_UNKNOWN;
	memset(p_owner->__data, 0, sizeof(p_owner->__data));
	p_owner->__ptr = 0;
}

double pifPulse_GetFrequency(PifPulse* p_owner)
{
	if ((p_owner->_measure_mode & PIF_PMM_PERIOD) && p_owner->_period_1us) {
		return 1000000.0 / p_owner->_period_1us;
	}
	return 0;
}

double pifPulse_GetLowLevelDuty(PifPulse* p_owner)
{
	if ((p_owner->_measure_mode & PIF_PMM_PERIOD) && (p_owner->_measure_mode & PIF_PMM_LOW_LEVEL_TIME) && p_owner->_period_1us) {
		return 100.0 * p_owner->_low_level_1us / p_owner->_period_1us;
	}
	return 0;
}

double pifPulse_GetHighLevelDuty(PifPulse* p_owner)
{
	if ((p_owner->_measure_mode & PIF_PMM_PERIOD) && (p_owner->_measure_mode & PIF_PMM_HIGH_LEVEL_TIME) && p_owner->_period_1us) {
		return 100.0 * p_owner->_high_level_1us / p_owner->_period_1us;
	}
	return 0;
}

void pifPulse_sigEdge(PifPulse* p_owner, PifPulseEdge edge)
{
	if (edge == PE_RISING) {
		if (p_owner->_measure_mode & PIF_PMM_RISING_COUNT) {
			p_owner->rising_count++;
		}
	}
	else {
		if (p_owner->_measure_mode & PIF_PMM_FALLING_COUNT) {
			p_owner->falling_count++;
		}
	}

	p_owner->__data[p_owner->__ptr].edge = edge;
	if (pif_act_timer1us) {
		p_owner->__data[p_owner->__ptr].timer1us = (*pif_act_timer1us)();
	}
	else {
		p_owner->__data[p_owner->__ptr].timer1us = pif_cumulative_timer1ms * 1000;
	}
	p_owner->__ptr = (p_owner->__ptr + 1) & PIF_PULSE_DATA_MASK;
	p_owner->__trigger_edge = edge;
}

void pifTimer_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, void* p_issuer)
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
	pifDList_Clear(&s_cs_list);
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
