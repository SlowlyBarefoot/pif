#include "pifComm.h"
#ifndef __PIF_NO_LOG__
#include "pifLog.h"
#endif


static BOOL _actReceiveData(PifComm* p_owner, uint8_t* p_data)
{
	return pifRingBuffer_GetByte(p_owner->_p_rx_buffer, p_data);
}

static uint16_t _actSendData(PifComm* p_owner, uint8_t* p_buffer, uint16_t size)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner->_p_tx_buffer);

	if (!remain) return 0;
	if (size > remain) size = remain;
	if (pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_buffer, size)) {
		return size;
	}
	return 0;
}

static void _sendData(PifComm* p_owner)
{
	if (p_owner->__act_send_data) {
		(*p_owner->evt_sending)(p_owner->__p_client, p_owner->__act_send_data);
	}
	else if (p_owner->_p_tx_buffer) {
		if ((*p_owner->evt_sending)(p_owner->__p_client, _actSendData)) {
			if (p_owner->__state == CTS_IDLE) {
				p_owner->__state = CTS_SENDING;
				if (p_owner->__act_start_transfer) {
					if (!(*p_owner->__act_start_transfer)()) p_owner->__state = CTS_IDLE;
				}
			}
		}
	}
}

/**
 * @fn pifComm_Create
 * @brief 
 * @param id
 * @return 
 */
PifComm* pifComm_Create(PifId id)
{
    PifComm* p_owner = NULL;

    p_owner = calloc(sizeof(PifComm), 1);
    if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
	    return NULL;
	}

    if (id == PIF_ID_AUTO) id = pif_id++;

    p_owner->_id = id;
    p_owner->__state = CTS_IDLE;
    return p_owner;
}

/**
 * @fn pifComm_Destroy
 * @brief 
 * @param pp_owner
 */
void pifComm_Destroy(PifComm** pp_owner)
{
	if (*pp_owner) {
		PifComm* p_owner = *pp_owner;
		if (p_owner->_p_rx_buffer)	pifRingBuffer_Exit(p_owner->_p_rx_buffer);
		if (p_owner->_p_tx_buffer)	pifRingBuffer_Exit(p_owner->_p_tx_buffer);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

/**
 * @fn pifComm_AllocRxBuffer
 * @brief
 * @param p_owner
 * @param rx_Size
 * @return
 */
BOOL pifComm_AllocRxBuffer(PifComm* p_owner, uint16_t rx_Size)
{
    if (!rx_Size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->_p_rx_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, rx_Size);
    if (!p_owner->_p_rx_buffer) return FALSE;
    pifRingBuffer_SetName(p_owner->_p_rx_buffer, "RB");
    return TRUE;
}

/**
 * @fn pifComm_AllocTxBuffer
 * @brief
 * @param p_owner
 * @param tx_size
 * @return
 */
BOOL pifComm_AllocTxBuffer(PifComm* p_owner, uint16_t tx_size)
{
	if (!tx_size) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    p_owner->_p_tx_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, tx_size);
    if (!p_owner->_p_tx_buffer) return FALSE;
    pifRingBuffer_SetName(p_owner->_p_tx_buffer, "TB");
	return TRUE;
}

/**
 * @fn pifComm_AttachClient
 * @brief
 * @param p_owner
 * @param p_client
 */
void pifComm_AttachClient(PifComm* p_owner, void* p_client)
{
	p_owner->__p_client = p_client;
}

/**
 * @fn pifComm_AttachActReceiveData
 * @brief
 * @param p_owner
 * @param act_receive_data
 */
void pifComm_AttachActReceiveData(PifComm* p_owner, PifActCommReceiveData act_receive_data)
{
	p_owner->__act_receive_data = act_receive_data;
}

/**
 * @fn pifComm_AttachActSendData
 * @brief
 * @param p_owner
 * @param act_send_data
 */
void pifComm_AttachActSendData(PifComm* p_owner, PifActCommSendData act_send_data)
{
	p_owner->__act_send_data = act_send_data;
}

/**
 * @fn pifComm_AttachActStartTransfer
 * @brief
 * @param p_owner
 * @param act_start_transfer
 */
void pifComm_AttachActStartTransfer(PifComm* p_owner, PifActCommStartTransfer act_start_transfer)
{
	p_owner->__act_start_transfer = act_start_transfer;
}

/**
 * @fn pifComm_GetRemainSizeOfRxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifComm_GetRemainSizeOfRxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetRemainSize(p_owner->_p_rx_buffer);
}

/**
 * @fn pifComm_GetFillSizeOfTxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifComm_GetFillSizeOfTxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
}

/**
 * @fn pifComm_ReceiveData
 * @brief
 * @param p_owner
 * @param data
 * @return 
 */
BOOL pifComm_ReceiveData(PifComm* p_owner, uint8_t data)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	return pifRingBuffer_PutByte(p_owner->_p_rx_buffer, data);
}

/**
 * @fn pifComm_ReceiveDatas
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
BOOL pifComm_ReceiveDatas(PifComm* p_owner, uint8_t* p_data, uint16_t length)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	return pifRingBuffer_PutData(p_owner->_p_rx_buffer, p_data, length);
}

/**
 * @fn pifComm_SendData
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
uint8_t pifComm_SendData(PifComm* p_owner, uint8_t* p_data)
{
	uint8_t ucState = PIF_COMM_SEND_DATA_STATE_INIT;

    if (!p_owner->_p_tx_buffer) return ucState;

    ucState = pifRingBuffer_GetByte(p_owner->_p_tx_buffer, p_data);
	if (ucState) {
		if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) {
			ucState |= PIF_COMM_SEND_DATA_STATE_EMPTY;
		}
	}
	else ucState |= PIF_COMM_SEND_DATA_STATE_EMPTY;
	return ucState;
}

/**
 * @fn pifComm_StartSendDatas
 * @brief
 * @param p_owner
 * @param pp_data
 * @param p_length
 * @return
 */
uint8_t pifComm_StartSendDatas(PifComm* p_owner, uint8_t** pp_data, uint16_t* p_length)
{
	uint16_t usLength;

    if (!p_owner->_p_tx_buffer) return PIF_COMM_SEND_DATA_STATE_INIT;
    if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) return PIF_COMM_SEND_DATA_STATE_EMPTY;

    *pp_data = pifRingBuffer_GetTailPointer(p_owner->_p_tx_buffer, 0);
    usLength = pifRingBuffer_GetLinerSize(p_owner->_p_tx_buffer, 0);
    if (!*p_length || usLength <= *p_length) *p_length = usLength;
	return PIF_COMM_SEND_DATA_STATE_DATA;
}

/**
 * @fn pifComm_EndSendDatas
 * @brief
 * @param p_owner
 * @param length
 * @return
 */
uint8_t pifComm_EndSendDatas(PifComm* p_owner, uint16_t length)
{
    pifRingBuffer_Remove(p_owner->_p_tx_buffer, length);
	return pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer) << 1;
}

/**
 * @fn pifComm_FinishTransfer
 * @brief
 * @param p_owner
 */
void pifComm_FinishTransfer(PifComm* p_owner)
{
	p_owner->__state = CTS_IDLE;
	p_owner->_p_task->immediate = TRUE;
}

/**
 * @fn pifComm_ForceSendData
 * @brief
 * @param p_owner
 */
void pifComm_ForceSendData(PifComm* p_owner)
{
	if (p_owner->evt_sending) _sendData(p_owner);
}

static uint16_t _doTask(PifTask* p_task)
{
	PifComm *p_owner = p_task->_p_client;

	if (p_owner->evt_parsing) {
		if (p_owner->__act_receive_data) {
			(*p_owner->evt_parsing)(p_owner->__p_client, p_owner->__act_receive_data);
		}
		else if (p_owner->_p_rx_buffer) {
			(*p_owner->evt_parsing)(p_owner->__p_client, _actReceiveData);
		}
	}

	if (p_owner->evt_sending) _sendData(p_owner);
	return 0;
}

/**
 * @fn pifComm_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifComm_AttachTask(PifComm* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(mode, period, _doTask, p_owner, start);
	return p_owner->_p_task;
}

