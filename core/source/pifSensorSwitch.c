#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSensorSwitch.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


static SWITCH _evtFilterCount(SWITCH state, PifSensorSwitchFilter* p_filter)
{
    if (p_filter->list & p_filter->msb) {
    	p_filter->list &= ~p_filter->msb;
    	p_filter->count--;
    }
    p_filter->list <<= 1;
    if (state) {
    	p_filter->list |= 1;
    	p_filter->count++;
    }
    return p_filter->count >= p_filter->half;
}

static SWITCH _evtFilterContinue(SWITCH state, PifSensorSwitchFilter* p_filter)
{
	int i, count;
	SWITCH sw;
	uint32_t mask;

	sw = p_filter->list & 1;
	if (sw != state) {
		count = 1;
		mask = 1L;
		for (i = 1; i < p_filter->size; i++) {
			if (((p_filter->list >> i) & 1) != sw) break;
			count++;
			mask |= 1L << i;
	    }
		if (count <= p_filter->half) {
			if (sw) {
				p_filter->list &= ~mask;
				p_filter->count -= count;
			}
			else {
				p_filter->list |= mask;
				p_filter->count += count;
			}
		}
	}
    if (p_filter->list & p_filter->msb) {
    	p_filter->list &= ~p_filter->msb;
    	p_filter->count--;
    }
    p_filter->list <<= 1;
	if (state) {
		p_filter->list |= 1;
		p_filter->count++;
	}
    return (p_filter->list >> p_filter->half) & 1;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char *prefix[SS_CSF_COUNT] = { "SSR", "SSF" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		PifSensorSwitch* p_owner = p_colsig->p_owner;
		for (int f = 0; f < SS_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->parent._id, CSVT_enWire, 1,
						prefix[f], p_owner->parent._curr_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SS_CS:Add(DC:%u F:%u)", p_owner->parent._id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

void pifSensorSwitch_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifSensorSwitch_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifSensorSwitch_Create
 * @brief 
 * @param id
 * @param init_state
 * @return 
 */
PifSensor* pifSensorSwitch_Create(PifId id, SWITCH init_state)
{
    PifSensorSwitch *p_owner = NULL;

    p_owner = calloc(sizeof(PifSensorSwitch), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    PifSensor *p_parent = &p_owner->parent;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_parent->_id = id;
    p_parent->_init_state = init_state;
    p_parent->_curr_state = init_state;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enSensorSwitch, _addDeviceInCollectSignal);
	PifSensorSwitchColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSensorSwitchColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
	p_colsig->state = init_state;
#endif
    return p_parent;
}

/**
 * @fn pifSensorSwitch_Destroy
 * @brief
 * @param pp_sensor
 */
void pifSensorSwitch_Destroy(PifSensor** pp_sensor)
{
    if (*pp_sensor) {
        free(*pp_sensor);
        *pp_sensor = NULL;
    }
}

/**
 * @fn pifSensorSwitch_InitialState
 * @brief
 * @param p_parent
 */
void pifSensorSwitch_InitialState(PifSensor* p_parent)
{
	PifSensorSwitch* p_owner = (PifSensorSwitch*)p_parent;

	p_parent->_curr_state = p_parent->_init_state;
#ifdef __PIF_COLLECT_SIGNAL__
	p_owner->__p_colsig->state = p_parent->_init_state;
#endif
	p_owner->__state = p_parent->_init_state;
}

/**
 * @fn pifSensorSwitch_AttachFilterState
 * @brief
 * @param p_parent
 * @param filter_method
 * @param filter_size
 * @param p_filter
 * @return
 */
BOOL pifSensorSwitch_AttachFilter(PifSensor* p_parent, uint8_t filter_method, uint8_t filter_size, PifSensorSwitchFilter* p_filter)
{
	PifSensorSwitch* p_owner = (PifSensorSwitch*)p_parent;

    if (!filter_method || filter_size < 3 || filter_size >= 32 || !p_filter) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    pifSensorSwitch_DetachFilter(&p_owner->parent);

    p_filter->size = filter_size;
    p_filter->half = filter_size / 2;
    p_filter->msb = 1L << (filter_size - 1);
    p_filter->count = 0;
    p_filter->list = 0L;
	switch (filter_method) {
    case PIF_SENSOR_SWITCH_FILTER_COUNT:
    	p_filter->evt_filter = _evtFilterCount;
        break;

    case PIF_SENSOR_SWITCH_FILTER_CONTINUE:
    	p_filter->evt_filter = _evtFilterContinue;
        break;

    default:
        break;
    }

	p_owner->__filter_method = filter_method;
	p_owner->__p_filter = p_filter;
    return TRUE;
}

/**
 * @fn pifSensorSwitch_DetachFilter
 * @brief
 * @param p_parent
 */
void pifSensorSwitch_DetachFilter(PifSensor* p_parent)
{
	PifSensorSwitch* p_owner = (PifSensorSwitch*)p_parent;
	PifSensorSwitchFilter* p_filter;

	if (p_owner->__filter_method) {
		p_filter = p_owner->__p_filter;
		p_filter->size = 0;
		p_filter->evt_filter = NULL;
	}

	p_owner->__filter_method = PIF_SENSOR_SWITCH_FILTER_NONE;
	p_owner->__p_filter = NULL;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSensorSwitch_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorSwitch_SetCsFlagAll(PifSensorSwitchCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorSwitch_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSensorSwitch_ResetCsFlagAll(PifSensorSwitchCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSensorSwitchColSig* p_colsig = (PifSensorSwitchColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSensorSwitch_SetCsFlagEach
 * @brief
 * @param p_parent
 * @param flag
 */
void pifSensorSwitch_SetCsFlagEach(PifSensor* p_parent, PifSensorSwitchCsFlag flag)
{
	((PifSensorSwitch*)p_parent)->__p_colsig->flag |= flag;
}

/**
 * @fn pifSensorSwitch_ResetCsFlagEach
 * @brief
 * @param p_parent
 * @param flag
 */
void pifSensorSwitch_ResetCsFlagEach(PifSensor* p_parent, PifSensorSwitchCsFlag flag)
{
	((PifSensorSwitch*)p_parent)->__p_colsig->flag &= ~flag;
}

#endif

/**
 * @fn pifSensorSwitch_sigData
 * @brief 
 * @param p_parent
 * @param swState
 */
void pifSensorSwitch_sigData(PifSensor* p_parent, SWITCH state)
{
	PifSensorSwitch *p_owner = (PifSensorSwitch *)p_parent;

	if (p_owner->__filter_method) {
    	PifSensorSwitchFilter* p_filter = p_owner->__p_filter;
    	p_owner->__state = (*p_filter->evt_filter)(state, p_filter);
    }
	else {
		p_owner->__state = state;
	}
#ifdef __PIF_COLLECT_SIGNAL__
	PifSensorSwitchColSig* p_colsig = p_owner->__p_colsig;
	if (p_colsig->flag & SS_CSF_RAW_BIT) {
		if (p_colsig->state != state) {
			pifCollectSignal_AddSignal(p_colsig->p_device[SS_CSF_RAW_IDX], state);
			p_colsig->state = state;
		}
	}
#endif
}

static uint16_t _doTask(PifTask* p_task)
{
	PifSensorSwitch* p_owner = p_task->_p_client;
	PifSensor* p_parent = &p_owner->parent;

	if (p_parent->__act_acquire) {
		pifSensorSwitch_sigData(p_parent, (*p_parent->__act_acquire)(p_parent->_id));
	}

	if (p_owner->__state != p_parent->_curr_state) {
		if (p_parent->__evt_change) {
			(*p_parent->__evt_change)(p_parent->_id, p_owner->__state, p_parent->__p_change_issuer);
#ifdef __PIF_COLLECT_SIGNAL__
			if (p_owner->__p_colsig->flag & SS_CSF_FILTER_BIT) {
				pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[SS_CSF_FILTER_IDX], p_owner->__state);
			}
#endif
		}
		p_parent->_curr_state = p_owner->__state;
	}
    return 0;
}

/**
 * @fn pifSensorSwitch_AttachTask
 * @brief Task를 추가한다.
 * @param p_parent
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifSensorSwitch_AttachTask(PifSensor* p_parent, PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, p_parent, start);
}

