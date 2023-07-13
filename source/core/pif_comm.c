#include "core/pif_comm.h"


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
		(*p_owner->__evt_sending)(p_owner->__p_client, p_owner->act_send_data);
	}
	else if (p_owner->_p_tx_buffer) {
		if ((*p_owner->__evt_sending)(p_owner->__p_client, _actSendData)) {
			if (p_owner->__state == CTS_IDLE) {
				p_owner->__state = CTS_SENDING;
				if (p_owner->act_start_transfer) {
					if (!(*p_owner->act_start_transfer)(p_owner)) p_owner->__state = CTS_IDLE;
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
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_rx_buffer) pifRingBuffer_Destroy(&p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Destroy(&p_owner->_p_tx_buffer);
}

BOOL pifComm_AllocRxBuffer(PifComm* p_owner, uint16_t rx_size, uint8_t threshold)
{
    if (!rx_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->_p_rx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, rx_size);
    if (!p_owner->_p_rx_buffer) return FALSE;
    if (threshold > 100) threshold = 100;
    p_owner->__rx_threshold = rx_size * 100 / threshold;
    if (p_owner->__rx_threshold == 0) p_owner->__rx_threshold = 1;
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

void pifComm_AttachClient(PifComm* p_owner, void* p_client, PifEvtCommParsing evt_parsing, PifEvtCommSending evt_sending)
{
	p_owner->__p_client = p_client;
	p_owner->__evt_parsing = evt_parsing;
	p_owner->__evt_sending = evt_sending;
	p_owner->_p_task->pause = FALSE;
}

void pifComm_DetachClient(PifComm* p_owner)
{
	p_owner->_p_task->pause = TRUE;
	if (p_owner->_p_rx_buffer) pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Empty(p_owner->_p_tx_buffer);
	p_owner->__p_client = NULL;
	p_owner->__evt_parsing = NULL;
	p_owner->__evt_sending = NULL;
}

uint16_t pifComm_GetRemainSizeOfRxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetRemainSize(p_owner->_p_rx_buffer);
}

uint16_t pifComm_GetFillSizeOfTxBuffer(PifComm* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
}

BOOL pifComm_PutRxByte(PifComm* p_owner, uint8_t data)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutByte(p_owner->_p_rx_buffer, data)) return FALSE;
	if (pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) >= p_owner->__rx_threshold) {
		pifTask_SetTrigger(p_owner->_p_task);
	}
	return TRUE;
}

BOOL pifComm_PutRxData(PifComm* p_owner, uint8_t* p_data, uint16_t length)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutData(p_owner->_p_rx_buffer, p_data, length)) return FALSE;
	if (pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) >= p_owner->__rx_threshold) {
		pifTask_SetTrigger(p_owner->_p_task);
	}
	return TRUE;
}

uint8_t pifComm_GetTxByte(PifComm* p_owner, uint8_t* p_data)
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

uint8_t pifComm_StartGetTxData(PifComm* p_owner, uint8_t** pp_data, uint16_t* p_length)
{
	uint16_t usLength;

    if (!p_owner->_p_tx_buffer) return PIF_COMM_SEND_DATA_STATE_INIT;
    if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) return PIF_COMM_SEND_DATA_STATE_EMPTY;

    *pp_data = pifRingBuffer_GetTailPointer(p_owner->_p_tx_buffer, 0);
    usLength = pifRingBuffer_GetLinerSize(p_owner->_p_tx_buffer, 0);
    if (!*p_length || usLength <= *p_length) *p_length = usLength;
	return PIF_COMM_SEND_DATA_STATE_DATA;
}

uint8_t pifComm_EndGetTxData(PifComm* p_owner, uint16_t length)
{
    pifRingBuffer_Remove(p_owner->_p_tx_buffer, length);
	return pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer) << 1;
}

uint16_t pifComm_ReceiveRxData(PifComm* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t i = 0, len;

	if (p_owner->act_receive_data) {
		i = 0;
		while (i < length) {
			if (!(*p_owner->act_receive_data)(p_owner, p_data + i)) break;
			i++;
		}
		return i;
	}
	else if (p_owner->_p_rx_buffer) {
		len = pifRingBuffer_CopyToArray(p_data, length, p_owner->_p_rx_buffer, 0);
		if (pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) >= p_owner->__rx_threshold) {
			pifTask_SetTrigger(p_owner->_p_task);
		}
		return len;
	}
	return 0;
}

BOOL pifComm_SendTxData(PifComm* p_owner, uint8_t* p_data, uint16_t length)
{
	if (p_owner->act_send_data) {
		return (*p_owner->act_send_data)(p_owner, p_data, length) == length;
	}
	else if (p_owner->_p_tx_buffer) {
		return pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_data, length);
	}
	return FALSE;
}

void pifComm_FinishTransfer(PifComm* p_owner)
{
	p_owner->__state = CTS_IDLE;
	pifTask_SetTrigger(p_owner->_p_task);
}

void pifComm_ForceSendData(PifComm* p_owner)
{
	if (p_owner->__evt_sending) _sendData(p_owner);
}

void pifComm_AbortRx(PifComm* p_owner)
{
	pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->evt_abort_rx) (*p_owner->evt_abort_rx)(p_owner->__p_client);
}

static uint16_t _doTask(PifTask* p_task)
{
	PifComm *p_owner = p_task->_p_client;

	if (p_owner->__evt_parsing) {
		if (p_owner->act_receive_data) {
			(*p_owner->__evt_parsing)(p_owner->__p_client, p_owner->act_receive_data);
		}
		else if (p_owner->_p_rx_buffer) {
			(*p_owner->__evt_parsing)(p_owner->__p_client, _actReceiveData);
		}
	}

	if (p_owner->__evt_sending) _sendData(p_owner);
	return 0;
}

PifTask* pifComm_AttachTask(PifComm* p_owner, PifTaskMode mode, uint16_t period, const char* name)
{
	p_owner->_p_task = pifTaskManager_Add(mode, period, _doTask, p_owner, FALSE);
	if (p_owner->_p_task) {
		if (name) p_owner->_p_task->name = name;
		else p_owner->_p_task->name = "Comm";
	}
	return p_owner->_p_task;
}

