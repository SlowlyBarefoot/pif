#ifdef __PIF_COLLECT_SIGNAL__
	#include "core/pif_collect_signal.h"
#endif
#ifndef __PIF_NO_LOG__
	#include "core/pif_log.h"
#endif
#include "sensor/pif_max31855.h"


#ifdef __PIF_COLLECT_SIGNAL__
	static PifDList s_cs_list;
#endif


static uint16_t _doTask(PifTask* p_task)
{
	PifMax31855* p_owner = (PifMax31855*)p_task->_p_client;
	PifSensor* p_parent = &p_owner->parent;
	double temperature;

	if (!pifMax31855_Measure(p_owner, &temperature, NULL)) return 0;

	if (p_parent->evt_change) {
		if (p_parent->_curr_state) {
			if (temperature < p_owner->__low_threshold) {
				p_parent->_curr_state = OFF;
				(*p_parent->evt_change)(p_parent, p_parent->_curr_state, (PifSensorValueP)&temperature, p_parent->p_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
				if (p_owner->__p_colsig->flag & M3_CSF_STATE_BIT) {
					pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[M3_CSF_STATE_IDX], p_parent->_curr_state);
				}
#endif
			}
		}
		else {
			if (temperature >= p_owner->__high_threshold) {
				p_parent->_curr_state = ON;
				(*p_parent->evt_change)(p_parent, p_parent->_curr_state, (PifSensorValueP)&temperature, p_parent->p_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
				if (p_owner->__p_colsig->flag & M3_CSF_STATE_BIT) {
					pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[M3_CSF_STATE_IDX], p_parent->_curr_state);
				}
#endif
			}
		}
	}

	if (p_owner->__evt_measure) {
		(*p_owner->__evt_measure)(p_owner, temperature, p_parent->p_issuer);
	}
	return 0;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char* prefix[M3_CSF_COUNT] = { "M3L", "M3H" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifMax31855ColSig* p_colsig = (PifMax31855ColSig*)it->data;
		PifMax31855* p_owner = p_colsig->p_owner;
		for (int f = 0; f < M3_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->parent._id, CSVT_WIRE, 1,
						prefix[f], p_owner->parent._curr_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "M3_CS:Add(DC:%u F:%u)", p_owner->parent._id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

#endif	// __PIF_COLLECT_SIGNAL__

BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port)
{
	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMax31855));

	p_owner->_p_spi = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO);
    if (!p_owner->_p_spi) return FALSE;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->parent._id = id;

#ifdef __PIF_COLLECT_SIGNAL__
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_MAX31855, _addDeviceInCollectSignal);
	}
	PifMax31855ColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifMax31855ColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return TRUE;

#ifdef __PIF_COLLECT_SIGNAL__
fail:
	pifMax31855_Destroy((PifSensor**)&p_owner);
	return FALSE;
#endif
}

void pifMax31855_Clear(PifMax31855* p_owner)
{
#ifdef __PIF_COLLECT_SIGNAL__
	pifDList_Remove(&s_cs_list, p_owner->__p_colsig);
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_MAX31855);
	}
	p_owner->__p_colsig = NULL;
#endif

	if (p_owner->_p_spi) {
		pifSpiPort_RemoveDevice(p_owner->_p_spi->__p_port, p_owner->_p_spi);
    	p_owner->_p_spi = NULL;
	}

	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal)
{
	uint8_t raw[4];
	int16_t data;

	pifSpiDevice_Transfer(p_owner->_p_spi, NULL, raw, 4);
	if ((raw[1] & 1) || (raw[3] & 7)) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (p_internal) {
		if (raw[2] & 0x80) {
			data = (int16_t)(0xF000 | (raw[2] << 4) | (raw[3] >> 4));
		}
		else {
			data = (raw[2] << 4) | (raw[3] >> 4);
		}
		*p_internal = data / 16.0;
	}

	if (p_temperature) {
		if (raw[0] & 0x80) {
			data = (int16_t)(0xC000 | (raw[0] << 6) | (raw[1] >> 2));
		}
		else {
			data = (raw[0] << 6) | (raw[1] >> 2);
		}
		if (p_owner->p_filter) {
			*p_temperature = *(int16_t*)pifNoiseFilter_Process(p_owner->p_filter, &data) / 4.0;
		}
		else {
			*p_temperature = data / 4.0;
		}
	}
	return TRUE;
}

BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period, PifEvtMax31855Measure evt_measure)
{
	if (!p_owner || !period) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(TM_PERIOD_MS, period, _doTask, p_owner, TRUE);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "MAX31855";

	p_owner->__evt_measure = evt_measure;
	return TRUE;
}

void pifMax31855_StopMeasurement(PifMax31855* p_owner)
{
	pifTaskManager_Remove(p_owner->_p_task);
	p_owner->_p_task = NULL;
}

void pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold)
{
	p_owner->__low_threshold = low_threshold;
	p_owner->__high_threshold = high_threshold;
}

#ifdef __PIF_COLLECT_SIGNAL__

void pifMax31855_SetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifMax31855_ResetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifMax31855ColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifMax31855ColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifMax31855ColSig_SetFlag(PifMax31855CsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifMax31855ColSig* p_colsig = (PifMax31855ColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifMax31855ColSig_ResetFlag(PifMax31855CsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifMax31855ColSig* p_colsig = (PifMax31855ColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif	// __PIF_COLLECT_SIGNAL__
