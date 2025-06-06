#include "core/pif_gpio.h"
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


#ifdef PIF_COLLECT_SIGNAL

static void _addDeviceInCollectSignal()
{
	const char* prefix[GP_CSF_COUNT] = { "GP" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifGpioColSig* p_colsig = (PifGpioColSig*)it->data;
		PifGpio* p_owner = p_colsig->p_owner;
		for (int f = 0; f < GP_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_REG, p_owner->count,
						prefix[f], p_owner->__write_state);
			}
		}
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_INFO, "GP_CS:Add(DC:%u CNT:%u)", p_owner->_id, p_owner->count);
#endif

		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL

BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count)
{
	if (!p_owner || !count || count > PIF_GPIO_MAX_COUNT) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;

#ifdef PIF_COLLECT_SIGNAL
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Attach(CSF_GPIO, _addDeviceInCollectSignal);
	}
	PifGpioColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PifGpioColSig));
	if (!p_colsig) goto fail;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
	return TRUE;

#ifdef PIF_COLLECT_SIGNAL
fail:
	pifGpio_Clear(p_owner);
	return FALSE;	
#endif
}

void pifGpio_Clear(PifGpio* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifGpioColSig* p_colsig = (PifGpioColSig*)it->data;
		if (p_colsig == p_owner->__p_colsig) {
			pifDList_Remove(&s_cs_list, it);
			break;
		}
		it = pifDList_Next(it);
	}
	if (!pifDList_Size(&s_cs_list)) {
		pifCollectSignal_Detach(CSF_GPIO);
	}
	p_owner->__p_colsig = NULL;
#else
	(void)p_owner;
#endif
}

uint8_t pifGpio_ReadAll(PifGpio* p_owner)
{
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = p_owner->__ui.act_in(p_owner->_id);

#ifdef PIF_COLLECT_SIGNAL
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], state);
	}
#endif

	return state;
}

SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index)
{
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = (p_owner->__ui.act_in(p_owner->_id) >> index) & 1;
	p_owner->__read_state = (p_owner->__read_state & (1 << index)) | state;

#ifdef PIF_COLLECT_SIGNAL
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__read_state);
	}
#endif

	return state;
}

BOOL pifGpio_WriteAll(PifGpio* p_owner, uint8_t state)
{
	if (!p_owner->__ui.act_out) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	p_owner->__write_state = state;
	p_owner->__ui.act_out(p_owner->_id, state);

#ifdef PIF_COLLECT_SIGNAL
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__write_state);
	}
#endif
	return TRUE;
}

BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state)
{
	if (!p_owner->__ui.act_out) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	if (state) {
		p_owner->__write_state |= 1 << index;
	}
	else {
		p_owner->__write_state &= ~(1 << index);
	}
	p_owner->__ui.act_out(p_owner->_id, p_owner->__write_state);

#ifdef PIF_COLLECT_SIGNAL
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__write_state);
	}
#endif
	return TRUE;
}

static uint32_t _doTask(PifTask* p_task)
{
	PifGpio* p_owner = p_task->_p_client;
	uint8_t state, bit;
#ifdef PIF_COLLECT_SIGNAL
	BOOL change = FALSE;
#endif

	state = p_owner->__ui.act_in(p_owner->_id);
	for (int i = 0; i < p_owner->count; i++) {
		bit = 1 << i;
		if ((state & bit) != (p_owner->__read_state & bit)) {
			if (p_owner->evt_in) (*p_owner->evt_in)(i, (state >> 1) & 1);
#ifdef PIF_COLLECT_SIGNAL
			change = TRUE;
#endif
		}
	}
	p_owner->__read_state = state;

#ifdef PIF_COLLECT_SIGNAL
	if (change && (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT)) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], state);
	}
#endif

    return 0;
}

void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state)
{
	if (state != ((p_owner->__read_state >> index) & 1)) {
		p_owner->__read_state = (p_owner->__read_state & (1 << index)) | state;
		if (p_owner->evt_in) (*p_owner->evt_in)(index, state);

#ifdef PIF_COLLECT_SIGNAL
		if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
			pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__read_state);
		}
#endif
	}
}

void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in)
{
    p_owner->__ui.act_in = act_in;
}

void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out)
{
    p_owner->__ui.act_out = act_out;
}

PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifId id, PifTaskMode mode, uint16_t period, BOOL start)
{
	PifTask* p_task;
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return NULL;
	}

	p_task = pifTaskManager_Add(id, mode, period, _doTask, p_owner, start);
	if (p_task) p_task->name = "Gpio";
	return p_task;
}

#ifdef PIF_COLLECT_SIGNAL

void pifGpio_SetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

void pifGpio_ResetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

void pifGpioColSig_Init()
{
	pifDList_Init(&s_cs_list);
}

void pifGpioColSig_Clear()
{
	pifDList_Clear(&s_cs_list, NULL);
}

void pifGpioColSig_SetFlag(PifGpioCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifGpioColSig* p_colsig = (PifGpioColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

void pifGpioColSig_ResetFlag(PifGpioCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PifGpioColSig* p_colsig = (PifGpioColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

#endif	// PIF_COLLECT_SIGNAL
