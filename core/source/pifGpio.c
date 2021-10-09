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
	const char *prefix[GpCsF_enCount] = { "GP" };

	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		PIF_stGpio* pstOwner = p_colsig->p_owner;
		for (int f = 0; f < GpCsF_enCount; f++) {
			if (p_colsig->flag & (1 << f)) {
				p_colsig->p_device[f] = pifCollectSignal_AddDevice(pstOwner->_usPifId, CSVT_enReg, pstOwner->ucGpioCount,
						prefix[f], pstOwner->__write_state);
			}
		}
#ifndef __PIF_NO_LOG__
		pifLog_Printf(LT_enInfo, "GP_CS:Add(DC:%u CNT:%u)", pstOwner->_usPifId, pstOwner->ucGpioCount);
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
 * @param usPifId
 * @param ucCount
 * @return
 */
PIF_stGpio* pifGpio_Create(PifId usPifId, uint8_t ucCount)
{
    PIF_stGpio* pstOwner = NULL;

    if (!ucCount || ucCount > PIF_GPIO_MAX_COUNT) {
        pif_error = E_INVALID_PARAM;
	    return NULL;
    }

    pstOwner = calloc(sizeof(PIF_stGpio), 1);
    if (!pstOwner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (usPifId == PIF_ID_AUTO) usPifId = pif_id++;
    pstOwner->_usPifId = usPifId;
    pstOwner->ucGpioCount = ucCount;

#ifdef __PIF_COLLECT_SIGNAL__
	pifCollectSignal_Attach(CSF_enGpio, _AddDeviceInCollectSignal);
	PIF_GpioColSig* p_colsig = pifDList_AddLast(&s_cs_list, sizeof(PIF_GpioColSig));
	if (!p_colsig) return NULL;
	p_colsig->p_owner = pstOwner;
	pstOwner->__p_colsig = p_colsig;
#endif
    return pstOwner;
}

/**
 * @fn pifGpio_Destroy
 * @brief
 * @param pp_owner
 */
void pifGpio_Destroy(PIF_stGpio** pp_owner)
{
    if (*pp_owner) {
    	free(*pp_owner);
        *pp_owner = NULL;
    }
}

/**
 * @fn pifGpio_ReadAll
 * @brief
 * @param pstOwner
 * @return
 */
uint8_t pifGpio_ReadAll(PIF_stGpio *pstOwner)
{
	if (!pstOwner->__ui.actIn) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = pstOwner->__ui.actIn(pstOwner->_usPifId);

#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__p_colsig->flag & GpCsF_enStateBit) {
		pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[GpCsF_enStateIdx], state);
	}
#endif

	return state;
}

/**
 * @fn pifGpio_ReadCell
 * @brief
 * @param pstOwner
 * @param ucIndex
 * @return
 */
SWITCH pifGpio_ReadCell(PIF_stGpio *pstOwner, uint8_t ucIndex)
{
	if (!pstOwner->__ui.actIn) {
		pif_error = E_CANNOT_USE;
		return 0xFF;
	}

	uint8_t state = (pstOwner->__ui.actIn(pstOwner->_usPifId) >> ucIndex) & 1;
	pstOwner->__read_state = (pstOwner->__read_state & (1 << ucIndex)) | state;

#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__p_colsig->flag & GpCsF_enStateBit) {
		pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[GpCsF_enStateIdx], pstOwner->__read_state);
	}
#endif

	return state;
}

/**
 * @fn pifGpio_WriteAll
 * @brief
 * @param pstOwner
 * @param ucState
 * @return
 */
BOOL pifGpio_WriteAll(PIF_stGpio *pstOwner, uint8_t ucState)
{
	if (!pstOwner->__ui.actOut) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	pstOwner->__write_state = ucState;
	pstOwner->__ui.actOut(pstOwner->_usPifId, ucState);

#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__p_colsig->flag & GpCsF_enStateBit) {
		pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[GpCsF_enStateIdx], pstOwner->__write_state);
	}
#endif
	return TRUE;
}

/**
 * @fn pifGpio_WriteCell
 * @brief
 * @param pstOwner
 * @param ucIndex
 * @param swState
 * @return
 */
BOOL pifGpio_WriteCell(PIF_stGpio *pstOwner, uint8_t ucIndex, SWITCH swState)
{
	if (!pstOwner->__ui.actOut) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	if (swState) {
		pstOwner->__write_state |= 1 << ucIndex;
	}
	else {
		pstOwner->__write_state &= ~(1 << ucIndex);
	}
	pstOwner->__ui.actOut(pstOwner->_usPifId, pstOwner->__write_state);

#ifdef __PIF_COLLECT_SIGNAL__
	if (pstOwner->__p_colsig->flag & GpCsF_enStateBit) {
		pifCollectSignal_AddSignal(pstOwner->__p_colsig->p_device[GpCsF_enStateIdx], pstOwner->__write_state);
	}
#endif
	return TRUE;
}

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifGpio_SetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifGpio_SetCsFlagAll(PIF_enGpioCsFlag enFlag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		p_colsig->flag |= enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifGpio_ResetCsFlagAll
 * @brief
 * @param enFlag
 */
void pifGpio_ResetCsFlagAll(PIF_enGpioCsFlag enFlag)
{
	PifDListIterator it = pifDList_Begin(&s_cs_list);
	while (it) {
		PIF_GpioColSig* p_colsig = (PIF_GpioColSig*)it->data;
		p_colsig->flag &= ~enFlag;
		it = pifDList_Next(it);
	}
}

/**
 * @fn pifGpio_SetCsFlagEach
 * @brief
 * @param pstOwner
 * @param enFlag
 */
void pifGpio_SetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag)
{
	pstOwner->__p_colsig->flag |= enFlag;
}

/**
 * @fn pifGpio_ResetCsFlagEach
 * @brief
 * @param pstOwner
 * @param enFlag
 */
void pifGpio_ResetCsFlagEach(PIF_stGpio *pstOwner, PIF_enGpioCsFlag enFlag)
{
	pstOwner->__p_colsig->flag &= ~enFlag;
}

#endif

static uint16_t _DoTask(PIF_stTask *pstTask)
{
	PIF_stGpio *p_owner = pstTask->_pvClient;
	uint8_t state, bit;
#ifdef __PIF_COLLECT_SIGNAL__
	BOOL change = FALSE;
#endif

	state = p_owner->__ui.actIn(p_owner->_usPifId);
	for (int i = 0; i < p_owner->ucGpioCount; i++) {
		bit = 1 << i;
		if ((state & bit) != (p_owner->__read_state & bit)) {
			if (p_owner->evtIn) (*p_owner->evtIn)(i, (state >> 1) & 1);
#ifdef __PIF_COLLECT_SIGNAL__
			change = TRUE;
#endif
		}
	}
	p_owner->__read_state = state;

#ifdef __PIF_COLLECT_SIGNAL__
	if (change && (p_owner->__p_colsig->flag & GpCsF_enStateBit)) {
		pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GpCsF_enStateIdx], state);
	}
#endif

    return 0;
}

void pifGpio_sigData(PIF_stGpio *p_owner, uint8_t index, SWITCH state)
{
	if (state != ((p_owner->__read_state >> index) & 1)) {
		p_owner->__read_state = (p_owner->__read_state & (1 << index)) | state;
		if (p_owner->evtIn) (*p_owner->evtIn)(index, state);

#ifdef __PIF_COLLECT_SIGNAL__
		if (p_owner->__p_colsig->flag & GpCsF_enStateBit) {
			pifCollectSignal_AddSignal(p_owner->__p_colsig->p_device[GpCsF_enStateIdx], p_owner->__read_state);
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
void pifGpio_AttachActIn(PIF_stGpio* p_owner, PIF_actGpioIn act_in)
{
    p_owner->__ui.actIn = act_in;
}

/**
 * @fn pifGpio_AttachActOut
 * @brief
 * @param p_owner
 * @param act_out
 */
void pifGpio_AttachActOut(PIF_stGpio* p_owner, PIF_actGpioOut act_out)
{
    p_owner->__ui.actOut = act_out;
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
PIF_stTask *pifGpio_AttachTaskIn(PIF_stGpio *p_owner, PIF_enTaskMode mode, uint16_t period, BOOL start)
{
	if (!p_owner->__ui.actIn) {
		pif_error = E_CANNOT_USE;
		return NULL;
	}

	return pifTaskManager_Add(mode, period, _DoTask, p_owner, start);
}
