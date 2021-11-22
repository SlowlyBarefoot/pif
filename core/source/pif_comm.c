#include "pif_comm.h"


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
	if (p_owner->act_send_data) {
		(*p_owner->evt_sending)(p_owner->__p_client, p_owner->act_send_data);
	}
	else if (p_owner->_p_tx_buffer) {
		if ((*p_owner->evt_sending)(p_owner->__p_client, _actSendData)) {
			if (p_owner->__state == CTS_IDLE) {
				p_owner->__state = CTS_SENDING;
				if (p_owner->act_start_transfer) {
					if (!(*p_owner->act_start_transfer)()) p_owner->__state = CTS_IDLE;
				}
			}
		}
	}
}

BOOL pifComm_Init(PifComm* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifComm));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;
}

void pifComm_Clear(PifComm* p_owner)
{
	if (p_owner->_p_rx_buffer) pifRingBuffer_Destroy(&p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Destroy(&p_owner->_p_tx_buffer);
}

BOOL pifComm_AllocRxBuffer(PifComm* p_owner, uint16_t rx_Size)
{
    if (!rx_Size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->_p_rx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, rx_Size);
    if (!p_owner->_p_rx_buffer) return FALSE;
    pifRingBuffer_SetName(p_owner->_p_rx_buffer, "RB");
    return TRUE;
}

BOOL pifComm_AllocTxBuffer(PifComm* p_owner, uint16_t tx_size)
{
	if (!tx_size) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    p_owner->_p_tx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, tx_size);
    if (!p_owner->_p_tx_buffer) return FALSE;
    pifRingBuffer_SetName(p_owner->_p_tx_buffer, "TB");
	return TRUE;
}

void pifComm_AttachClient(PifComm* p_owner, void* p_client)
{
	p_owner->__p_client = p_client;
}

uint16_t pifComm_GetRemainSizeOfRxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetRemainSize(p_owner->_p_rx_buffer);
}

uint16_t pifComm_GetFillSizeOfTxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
}

BOOL pifComm_ReceiveData(PifComm* p_owner, uint8_t data)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	return pifRingBuffer_PutByte(p_owner->_p_rx_buffer, data);
}

BOOL pifComm_ReceiveDatas(PifComm* p_owner, uint8_t* p_data, uint16_t length)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	return pifRingBuffer_PutData(p_owner->_p_rx_buffer, p_data, length);
}

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

uint8_t pifComm_EndSendDatas(PifComm* p_owner, uint16_t length)
{
    pifRingBuffer_Remove(p_owner->_p_tx_buffer, length);
	return pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer) << 1;
}

void pifComm_FinishTransfer(PifComm* p_owner)
{
	p_owner->__state = CTS_IDLE;
	p_owner->_p_task->immediate = TRUE;
}

void pifComm_ForceSendData(PifComm* p_owner)
{
	if (p_owner->evt_sending) _sendData(p_owner);
}

static uint16_t _doTask(PifTask* p_task)
{
	PifComm *p_owner = p_task->_p_client;

	if (p_owner->evt_parsing) {
		if (p_owner->act_receive_data) {
			(*p_owner->evt_parsing)(p_owner->__p_client, p_owner->act_receive_data);
		}
		else if (p_owner->_p_rx_buffer) {
			(*p_owner->evt_parsing)(p_owner->__p_client, _actReceiveData);
		}
	}

	if (p_owner->evt_sending) _sendData(p_owner);
	return 0;
}

PifTask* pifComm_AttachTask(PifComm* p_owner, PifTaskMode mode, uint16_t period, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(mode, period, _doTask, p_owner, start);
	return p_owner->_p_task;
}

