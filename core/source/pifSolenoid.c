#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif
#include "pifSolenoid.h"


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


static void _action(PifSolenoid* p_owner, BOOL state, PifSolenoidDir dir)
{
	(*p_owner->__act_control)(state, dir);
#ifdef __PIF_COLLECT_SIGNAL__
	PifSolenoidColSig* p_colsig = p_owner->__p_colsig;
	if (p_colsig->flag & SN_CSF_ACTION_BIT) {
		pifCollectSignal_AddSignal(p_colsig->p_device[SN_CSF_ACTION_IDX], state);
	}
	if (p_colsig->flag & SN_CSF_DIR_BIT) {
		pifCollectSignal_AddSignal(p_colsig->p_device[SN_CSF_DIR_IDX], dir);
	}
#endif
}

static void _actionOn(PifSolenoid *p_owner, uint16_t delay, PifSolenoidDir dir)
{
	if (!delay) {
		if (p_owner->_type != ST_2POINT || dir != p_owner->__current_dir) {
			p_owner->__current_dir = dir;
			_action(p_owner, ON, dir);
			p_owner->__state = TRUE;
			if (p_owner->on_time) {
				if (!pifPulse_StartItem(p_owner->__p_timer_on, p_owner->on_time)) {
					p_owner->__current_dir = SD_INVALID;
					_action(p_owner, OFF, SD_INVALID);
					p_owner->__state = FALSE;
					if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
				}
			}
		}
	}
	else {
		p_owner->__dir = dir;
		if (!pifPulse_StartItem(p_owner->__p_timer_delay, delay)) {
			if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
		}
	}
}

static void _evtTimerDelayFinish(void* p_issuer)
{
    PifSolenoid* p_owner = (PifSolenoid*)p_issuer;

	if (p_owner->_type != ST_2POINT || p_owner->__dir != p_owner->__current_dir) {
		p_owner->__current_dir = p_owner->__dir;
		_action(p_owner, ON, p_owner->__dir);
		p_owner->__state = TRUE;
		if (p_owner->on_time) {
			if (!pifPulse_StartItem(p_owner->__p_timer_on, p_owner->on_time)) {
				p_owner->__current_dir = SD_INVALID;
				_action(p_owner, OFF, SD_INVALID);
				p_owner->__state = FALSE;
				if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
			}
		}
	}

	if (p_owner->__p_buffer) {
		PifSolenoidContent *pstContent = pifRingData_Remove(p_owner->__p_buffer);
		if (pstContent) {
			_actionOn(p_owner, pstContent->delay, pstContent->dir);
		}
	}
}

static void _evtTimerOnFinish(void* p_issuer)
{
    PifSolenoid* p_owner = (PifSolenoid*)p_issuer;

    if (p_owner->__state) {
        _action(p_owner, OFF, SD_INVALID);
        if (p_owner->evt_off) (*p_owner->evt_off)(p_owner);
        p_owner->__state = FALSE;
    }
}

static int32_t _calcurateTime(PifSolenoid* p_owner)
{
	PifSolenoidContent* p_content;
	int32_t time;

	time = pifPulse_RemainItem(p_owner->__p_timer_delay);
	p_content = pifRingData_GetFirstData(p_owner->__p_buffer);
	while (p_content) {
		time += p_content->delay;
		p_content = pifRingData_GetNextData(p_owner->__p_buffer);
	}
	return time;
}

#ifdef __PIF_COLLECT_SIGNAL__

static void _addDeviceInCollectSignal()
{
	const char *prefix[SN_CSF_COUNT] = { "SNA", "SND" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSolenoidColSig* p_colsig = (PifSolenoidColSig*)it->data;
		PifSolenoid* p_owner = p_colsig->p_owner;
		if (p_colsig->flag & 1) {
			p_colsig->p_device[0] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_WIRE, 1, prefix[0], 0);
		}
		if (p_colsig->flag & 2) {
			p_colsig->p_device[1] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_WIRE, 2, prefix[1], 0);
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "SN_CS:Add(DC:%u F:%u)", p_owner->_id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

void pifSolenoid_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifSolenoid_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifSolenoid_Create
 * @brief
 * @param id
 * @param p_timer
 * @param type
 * @param on_time
 * @param act_control
 * @return
 */
PifSolenoid* pifSolenoid_Create(PifId id, PifPulse* p_timer, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control)
{
    PifSolenoid *p_owner = NULL;

    if (!p_timer || !act_control) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

    p_owner = calloc(sizeof(PifSolenoid), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    p_owner->__p_timer_on = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__p_timer_on) return NULL;
    pifPulse_AttachEvtFinish(p_owner->__p_timer_on, _evtTimerOnFinish, p_owner);

    p_owner->__p_timer_delay = pifPulse_AddItem(p_timer, PT_ONCE);
    if (!p_owner->__p_timer_delay) return NULL;
    pifPulse_AttachEvtFinish(p_owner->__p_timer_delay, _evtTimerDelayFinish, p_owner);

    p_owner->__p_timer = p_timer;
    p_owner->__act_control = act_control;
    p_owner->__state = FALSE;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_type = type;
    p_owner->on_time = on_time;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_SOLENOID, _addDeviceInCollectSignal);
	PifSolenoidColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSolenoidColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return p_owner;
}

/**
 * @fn pifSolenoid_Destroy
 * @brief
 * @param pp_owner
 */
void pifSolenoid_Destroy(PifSolenoid** pp_owner)
{
    if (*pp_owner) {
		PifSolenoid* p_owner = *pp_owner;
		if (p_owner->__p_timer_on) {
			pifPulse_RemoveItem(p_owner->__p_timer_on);
		}
		if (p_owner->__p_timer_delay) {
			pifPulse_RemoveItem(p_owner->__p_timer_delay);
		}
		pifRingData_Destroy(&p_owner->__p_buffer);
    	free(*pp_owner);
        *pp_owner = NULL;
    }
}

/**
 * @fn pifSolenoid_SetBuffer
 * @brief
 * @param p_owner
 * @param size
 * @return
 */
BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint16_t size)
{
	p_owner->__p_buffer = pifRingData_Create(PIF_ID_AUTO, sizeof(PifSolenoidContent), size);
	if (!p_owner->__p_buffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifSolenoid_SetInvalidDirection
 * @brief
 * @param p_owner
 */
void pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner)
{
	p_owner->__current_dir = SD_INVALID;
}

/**
 * @fn pifSolenoid_SetOnTime
 * @brief
 * @param p_owner
 * @param on_time
 * @return
 */
BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint16_t on_time)
{
    if (!on_time) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->on_time = on_time;
    return TRUE;
}

/**
 * @fn pifSolenoid_ActionOn
 * @brief
 * @param p_owner
 * @param delay
 */
void pifSolenoid_ActionOn(PifSolenoid* p_owner, uint16_t delay)
{
	PifSolenoidContent *pstContent;
	int32_t time;

	if (p_owner->__p_buffer) {
		if (p_owner->__p_timer_delay->_step == PS_RUNNING) {
			time = _calcurateTime(p_owner);
			pstContent = pifRingData_Add(p_owner->__p_buffer);
			pstContent->delay = delay > time ? delay - time : 0;
			return;
		}
	}

	_actionOn(p_owner, delay, SD_INVALID);
}

/**
 * @fn pifSolenoid_ActionOnDir
 * @brief 
 * @param p_owner
 * @param delay
 * @param dir
 */
void pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint16_t delay, PifSolenoidDir dir)
{
	PifSolenoidContent *pstContent;
	int32_t time;

	if (p_owner->__p_buffer) {
		if (p_owner->__p_timer_delay->_step == PS_RUNNING) {
			time = _calcurateTime(p_owner);
			pstContent = pifRingData_Add(p_owner->__p_buffer);
			pstContent->delay = delay >= time ? delay - time : 0;
			pstContent->dir = dir;
			return;
		}
	}

    _actionOn(p_owner, delay, dir);
}

/**
 * @fn pifSolenoid_ActionOff
 * @brief
 * @param p_owner
 */
void pifSolenoid_ActionOff(PifSolenoid* p_owner)
{
	if (p_owner->__state) {
		pifPulse_StopItem(p_owner->__p_timer_on);
		_action(p_owner, OFF, SD_INVALID);
		p_owner->__state = FALSE;
    }
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSolenoid_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSolenoid_SetCsFlagAll(PifSolenoidCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSolenoidColSig* p_colsig = (PifSolenoidColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSolenoid_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSolenoid_ResetCsFlagAll(PifSolenoidCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSolenoidColSig* p_colsig = (PifSolenoidColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifSolenoid_SetCsFlagEach
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_SetCsFlagEach(PifSolenoid *p_owner, PifSolenoidCsFlag flag)
{
	((PifSolenoid *)p_owner)->__p_colsig->flag |= flag;
}

/**
 * @fn pifSolenoid_ResetCsFlagEach
 * @brief
 * @param pstSensor
 * @param flag
 */
void pifSolenoid_ResetCsFlagEach(PifSolenoid *p_owner, PifSolenoidCsFlag flag)
{
	((PifSolenoid *)p_owner)->__p_colsig->flag &= ~flag;
}

#endif
