#include "actulator/pif_solenoid.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif
#include "core/pif_list.h"
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif


#ifdef PIF_COLLECT_SIGNAL
	static PifDList s_cs_list;
#endif


static void _action(PifSolenoid* p_owner, BOOL state, PifSolenoidDir dir)
{
	(*p_owner->__act_control)(state, dir);
#ifdef PIF_COLLECT_SIGNAL
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
				if (!pifTimer_Start(p_owner->__p_timer_on, p_owner->on_time)) {
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
		if (!pifTimer_Start(p_owner->__p_timer_delay, delay)) {
			if (p_owner->evt_error) (*p_owner->evt_error)(p_owner);
		}
	}
}

static void _evtTimerDelayFinish(PifIssuerP p_issuer)
{
    PifSolenoid* p_owner = (PifSolenoid*)p_issuer;

	if (p_owner->_type != ST_2POINT || p_owner->__dir != p_owner->__current_dir) {
		p_owner->__current_dir = p_owner->__dir;
		_action(p_owner, ON, p_owner->__dir);
		p_owner->__state = TRUE;
		if (p_owner->on_time) {
			if (!pifTimer_Start(p_owner->__p_timer_on, p_owner->on_time)) {
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

static void _evtTimerOnFinish(PifIssuerP p_issuer)
{
    PifSolenoid* p_owner = (PifSolenoid*)p_issuer;

    if (p_owner->evt_off) (*p_owner->evt_off)(p_owner);
}

static void _evtTimerOnIntFinish(PifIssuerP p_issuer)
{
    PifSolenoid* p_owner = (PifSolenoid*)p_issuer;

    if (p_owner->__state) {
        _action(p_owner, OFF, SD_INVALID);
        p_owner->__state = FALSE;
    }
}

static int32_t _calcurateTime(PifSolenoid* p_owner)
{
	PifSolenoidContent* p_content;
	int32_t time;

	time = pifTimer_Remain(p_owner->__p_timer_delay);
	p_content = pifRingData_GetFirstData(p_owner->__p_buffer);
	while (p_content) {
		time += p_content->delay;
		p_content = pifRingData_GetNextData(p_owner->__p_buffer);
	}
	return time;
}

#ifdef PIF_COLLECT_SIGNAL

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
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "SN_CS:Add(DC:%u F:%u)", p_owner->_id, p_colsig->flag);
#endif

		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL

BOOL pifSolenoid_Init(PifSolenoid* p_owner, PifId id, PifTimerManager* p_timer_manager, PifSolenoidType type, uint16_t on_time,
		PifActSolenoidControl act_control)
{
    if (!p_owner || !p_timer_manager || !act_control) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSolenoid));

    p_owner->__p_timer_on = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__p_timer_on) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__p_timer_on, _evtTimerOnFinish, p_owner);
    pifTimer_AttachEvtIntFinish(p_owner->__p_timer_on, _evtTimerOnIntFinish, p_owner);

    p_owner->__p_timer_delay = pifTimerManager_Add(p_timer_manager, TT_ONCE);
    if (!p_owner->__p_timer_delay) goto fail;
    pifTimer_AttachEvtFinish(p_owner->__p_timer_delay, _evtTimerDelayFinish, p_owner);

    p_owner->__p_timer_manager = p_timer_manager;
    p_owner->__act_control = act_control;

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_type = type;
    p_owner->on_time = on_time;

#ifdef PIF_COLLECT_SIGNAL
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_SOLENOID, _addDeviceInCollectSignal);
	}
	PifSolenoidColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifSolenoidColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return TRUE;

fail:
	pifSolenoid_Clear(p_owner);
	return FALSE;	
}

void pifSolenoid_Clear(PifSolenoid* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifDList_Remove(&s_cs_list, p_owner->__p_colsig);
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_SOLENOID);
	}
	p_owner->__p_colsig = NULL;
#endif
	if (p_owner->__p_timer_on) {
		pifTimerManager_Remove(p_owner->__p_timer_on);
		p_owner->__p_timer_on = NULL;
	}
	if (p_owner->__p_timer_delay) {
		pifTimerManager_Remove(p_owner->__p_timer_delay);
		p_owner->__p_timer_delay = NULL;
	}
	pifRingData_Destroy(&p_owner->__p_buffer);
}

BOOL pifSolenoid_SetBuffer(PifSolenoid* p_owner, uint16_t size)
{
	p_owner->__p_buffer = pifRingData_Create(PIF_ID_AUTO, sizeof(PifSolenoidContent), size);
	if (!p_owner->__p_buffer) return FALSE;
	return TRUE;
}

void pifSolenoid_SetInvalidDirection(PifSolenoid* p_owner)
{
	p_owner->__current_dir = SD_INVALID;
}

BOOL pifSolenoid_SetOnTime(PifSolenoid* p_owner, uint16_t on_time)
{
    if (!on_time) {
        pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->on_time = on_time;
    return TRUE;
}

void pifSolenoid_ActionOn(PifSolenoid* p_owner, uint16_t delay)
{
	PifSolenoidContent *pstContent;
	int32_t time;

	if (p_owner->__p_buffer) {
		if (p_owner->__p_timer_delay->_step == TS_RUNNING) {
			time = _calcurateTime(p_owner);
			pstContent = pifRingData_Add(p_owner->__p_buffer);
			pstContent->delay = delay > time ? delay - time : 0;
			return;
		}
	}

	_actionOn(p_owner, delay, SD_INVALID);
}

void pifSolenoid_ActionOnDir(PifSolenoid* p_owner, uint16_t delay, PifSolenoidDir dir)
{
	PifSolenoidContent *pstContent;
	int32_t time;

	if (p_owner->__p_buffer) {
		if (p_owner->__p_timer_delay->_step == TS_RUNNING) {
			time = _calcurateTime(p_owner);
			pstContent = pifRingData_Add(p_owner->__p_buffer);
			pstContent->delay = delay >= time ? delay - time : 0;
			pstContent->dir = dir;
			return;
		}
	}

    _actionOn(p_owner, delay, dir);
}

void pifSolenoid_ActionOff(PifSolenoid* p_owner)
{
	if (p_owner->__state) {
		pifTimer_Stop(p_owner->__p_timer_on);
		_action(p_owner, OFF, SD_INVALID);
		p_owner->__state = FALSE;
    }
}


#ifdef PIF_COLLECT_SIGNAL

void pifSolenoid_SetCsFlag(PifSolenoid *p_owner, PifSolenoidCsFlag flag)
{
	((PifSolenoid *)p_owner)->__p_colsig->flag |= flag;
}

void pifSolenoid_ResetCsFlag(PifSolenoid *p_owner, PifSolenoidCsFlag flag)
{
	((PifSolenoid *)p_owner)->__p_colsig->flag &= ~flag;
}

void pifSolenoidColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifSolenoidColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifSolenoidColSig_SetFlag(PifSolenoidCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSolenoidColSig* p_colsig = (PifSolenoidColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifSolenoidColSig_ResetFlag(PifSolenoidCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifSolenoidColSig* p_colsig = (PifSolenoidColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL
