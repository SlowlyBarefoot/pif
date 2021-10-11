#include "pif_list.h"
#ifdef __PIF_COLLECT_SIGNAL__
#include "pifCollectSignal.h"
#endif
#include "pifGpio.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


#ifdef __PIF_COLLECT_SIGNAL__
static PifDList s_cs_list;
#endif


#ifdef __PIF_COLLECT_SIGNAL__

static void _AddDeviceInCollectSignal()
{
	const char* prefix[GP_CSF_COUNT] = { "GP" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		PifGpio* p_owner = p_colsig->p_owner;
		for (int f = 0; f < GP_CSF_COUNT; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(p_owner->_id, CSVT_REG, p_owner->count,
						prefix[f], p_owner->__write_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_INFO, "GP_CS:Add(DC:%u CNT:%u)", p_owner->_id, p_owner->count);
#endif

		it = pifDList_Next(it);
	}
}

void pifGpio_ColSigInit()
{
	pifDList_Init(&s_cs_list);
}

void pifGpio_ColSigClear()
{
	pifDList_Clear(&s_cs_list);
}

#endif

/**
 * @fn pifGpio_Create
 * @brief
 * @param id
 * @param count
 * @return
 */
PifGpio* pifGpio_Create(PifId id, uint8_t count)
{
    PifGpio* p_owner = NULL;

    if (!count || count > PIF_GPIO_MAX_COUNT) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
    }

    p_owner = calloc(sizeof(PifGpio), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->count = count;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_GPIO, _AddDeviceInCollectSignal);
	PIF_GpioColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_GpioColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = p_owner;
	p_owner->__p_colsig = p_colsig;
#endif
    return p_owner;
}

/**
 * @fn pifGpio_Destroy
 * @brief
 * @param pp_owner
 */
void pifGpio_Destroy(PifGpio** pp_owner)
{
    if (*pp_owner) {
    	free(*pp_owner);
        *pp_owner = NULL;
    }
}

/**
 * @fn pifGpio_ReadAll
 * @brief
 * @param p_owner
 * @return
 */
uint8_t pifGpio_ReadAll(PifGpio* p_owner)
{
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = p_owner->__ui.act_in(p_owner->_id);

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], state);
	}
#endif

	return state;
}

/**
 * @fn pifGpio_ReadCell
 * @brief
 * @param p_owner
 * @param index
 * @return
 */
SWITCH pifGpio_ReadCell(PifGpio* p_owner, uint8_t index)
{
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = (p_owner->__ui.act_in(p_owner->_id) >> index) & 1;
	p_owner->__read_state = (p_owner->__read_state & (1 << index)) | state;

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__read_state);
	}
#endif

	return state;
}

/**
 * @fn pifGpio_WriteAll
 * @brief
 * @param p_owner
 * @param state
 * @return
 */
BOOL pifGpio_WriteAll(PifGpio* p_owner, uint8_t state)
{
	if (!p_owner->__ui.act_out) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	p_owner->__write_state = state;
	p_owner->__ui.act_out(p_owner->_id, state);

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__write_state);
	}
#endif
	return TRUE;
}

/**
 * @fn pifGpio_WriteCell
 * @brief
 * @param p_owner
 * @param ucIndex
 * @param swState
 * @return
 */
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

#ifdef __PIF_COLLECT_SIGNAL__
	if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__write_state);
	}
#endif
	return TRUE;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifGpio_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifGpio_SetCsFlagAll(PifGpioCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		p_colsig->flag |= flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifGpio_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifGpio_ResetCsFlagAll(PifGpioCsFlag flag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		p_colsig->flag &= ~flag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifGpio_SetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_SetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag)
{
	p_owner->__p_colsig->flag |= flag;
}

/**
 * @fn pifGpio_ResetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifGpio_ResetCsFlagEach(PifGpio* p_owner, PifGpioCsFlag flag)
{
	p_owner->__p_colsig->flag &= ~flag;
}

#endif

static uint16_t _DoTask(PifTask* p_task)
{
	PifGpio* p_owner = p_task->_p_client;
	uint8_t state, bit;
#ifdef __PIF_COLLECT_SIGNAL__
	BOOL change = FALSE;
#endif

	state = p_owner->__ui.act_in(p_owner->_id);
	for (int i = 0; i < p_owner->count; i++) {
		bit = 1 << i;
		if ((state & bit) != (p_owner->__read_state & bit)) {
			if (p_owner->evt_in) (*p_owner->evt_in)(i, (state >> 1) & 1);
#ifdef __PIF_COLLECT_SIGNAL__
			change = TRUE;
#endif
		}
	}
	p_owner->__read_state = state;

#ifdef __PIF_COLLECT_SIGNAL__
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

#ifdef __PIF_COLLECT_SIGNAL__
		if (p_owner->__p_colsig->flag & GP_CSF_STATE_BIT) {
			pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GP_CSF_STATE_IDX], p_owner->__read_state);
		}
#endif
	}
}

/**
 * @fn pifGpio_AttachActIn
 * @brief
 * @param p_owner
 * @param act_in
 */
void pifGpio_AttachActIn(PifGpio* p_owner, PifActGpioIn act_in)
{
    p_owner->__ui.act_in = act_in;
}

/**
 * @fn pifGpio_AttachActOut
 * @brief
 * @param p_owner
 * @param act_out
 */
void pifGpio_AttachActOut(PifGpio* p_owner, PifActGpioOut act_out)
{
    p_owner->__ui.act_out = act_out;
}

/**
 * @fn pifGpio_AttachTaskIn
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifGpio_AttachTaskIn(PifGpio* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	if (!p_owner->__ui.act_in) {
		pif_error = E_CANNOT_USE;
		return NULL;
	}

	return pifTaskManager_Add(mode, period, _DoTask, p_owner, start);
}
