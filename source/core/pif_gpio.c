// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_gpio.h"

#include <string.h>

// GPIO creation, state control, and optional signal collection integration.

static PifGpioState _mask(PifGpio* p_owner)
{
	// Shifting by the full width is undefined, so a full instance takes every bit.
	if (p_owner->count >= PIF_GPIO_WIDTH) return (PifGpioState)~(PifGpioState)0;
	return ((PifGpioState)1 << p_owner->count) - 1;
}

BOOL pifGpio_Init(PifGpio* p_owner, PifId id, uint8_t count)
{
	if (!p_owner || !count || count > PIF_GPIO_MAX_COUNT) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGpio));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;
	return TRUE;
}

void pifGpio_Clear(PifGpio* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifGpio_ResetCsFlag(p_owner, GP_CSF_ALL_BIT);
#else
	(void)p_owner;
#endif
}

PifGpioState pifGpio_ReadAll(PifGpio* p_owner)
{
	if (!p_owner->__act_in) {
		pif_error = E_CANNOT_USE;
		return 0;
	}

	PifGpioState state = p_owner->__act_in(p_owner->_id) & _mask(p_owner);

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[GP_CSF_STATE_IDX], state);
#endif

	return state;
}

SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index)
{
	if (index >= p_owner->count) {
		pif_error = E_INVALID_PARAM;
		return OFF;
	}

	// __read_state is left alone: it is the reference of the polling task, and changing it here would hide events.
	return (pifGpio_ReadAll(p_owner) >> index) & 1;
}

BOOL pifGpio_WriteAll(PifGpio* p_owner, PifGpioState state)
{
	if (!p_owner->__act_out) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	p_owner->__write_state = state & _mask(p_owner);
	p_owner->__act_out(p_owner->_id, p_owner->__write_state);

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[GP_CSF_STATE_IDX], p_owner->__write_state);
#endif
	return TRUE;
}

BOOL pifGpio_WriteCell(PifGpio* p_owner, uint8_t index, SWITCH state)
{
	if (index >= p_owner->count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	PifGpioState bit = (PifGpioState)1 << index;
	return pifGpio_WriteAll(p_owner, state ? p_owner->__write_state | bit : p_owner->__write_state & ~bit);
}

static uint32_t _doTask(PifTask* p_task)
{
	PifGpio* p_owner = p_task->_p_client;
	PifGpioState state, changed;

	state = p_owner->__act_in(p_owner->_id) & _mask(p_owner);
	changed = state ^ p_owner->__read_state;
	p_owner->__read_state = state;

#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[GP_CSF_STATE_IDX], state);
#endif

	if (changed && p_owner->evt_in) {
		for (int i = 0; i < p_owner->count; i++) {
			if (changed & ((PifGpioState)1 << i)) (*p_owner->evt_in)(p_owner, i, (state >> i) & 1);
		}
	}
    return 0;
}

void pifGpio_sigData(PifGpio* p_owner, uint8_t index, SWITCH state)
{
	if (index >= p_owner->count) {
		pif_error = E_INVALID_PARAM;
		return;
	}

	state = state ? ON : OFF;
	if (state != ((p_owner->__read_state >> index) & 1)) {
		p_owner->__read_state = (p_owner->__read_state & ~((PifGpioState)1 << index)) | ((PifGpioState)state << index);

#ifdef PIF_COLLECT_SIGNAL
		pifCollectSignal_Put(&p_owner->__cs[GP_CSF_STATE_IDX], p_owner->__read_state);
#endif

		if (p_owner->evt_in) (*p_owner->evt_in)(p_owner, index, state);
	}
}

void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in)
{
    p_owner->__act_in = act_in;
}

void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out)
{
    p_owner->__act_out = act_out;
}

PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifId id, PifTaskMode mode, uint16_t period, BOOL start)
{
	PifTask* p_task;
	if (!p_owner->__act_in) {
		pif_error = E_CANNOT_USE;
		return NULL;
	}

	// Start from the current input so pins that are already set do not raise events on the first poll.
	p_owner->__read_state = p_owner->__act_in(p_owner->_id) & _mask(p_owner);

	p_task = pifTaskManager_Add(id, mode, period, _doTask, p_owner, start);
	if (p_task) p_task->name = "Gpio";
	return p_task;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifGpio_SetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag)
{
	if (flag & GP_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[GP_CSF_STATE_IDX], "GP", p_owner->_id, CSVT_REG,
				p_owner->count, p_owner->__write_state)) return FALSE;
	}
	return TRUE;
}

void pifGpio_ResetCsFlag(PifGpio* p_owner, PifGpioCsFlag flag)
{
	if (flag & GP_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[GP_CSF_STATE_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
