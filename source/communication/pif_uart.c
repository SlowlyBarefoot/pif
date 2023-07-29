#include "communication/pif_uart.h"


static BOOL _actReceiveData(PifUart* p_owner, uint8_t* p_data)
{
	return pifRingBuffer_GetByte(p_owner->_p_rx_buffer, p_data);
}

static uint16_t _actSendData(PifUart* p_owner, uint8_t* p_buffer, uint16_t size)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner->_p_tx_buffer);

	if (!remain) return 0;
	if (size > remain) size = remain;
	if (pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_buffer, size)) {
		return size;
	}
	return 0;
}

static void _sendData(PifUart* p_owner)
{
	if (p_owner->act_send_data) {
		(*p_owner->__evt_sending)(p_owner->__p_client, p_owner->act_send_data);
	}
	else if (p_owner->_p_tx_buffer) {
		if ((*p_owner->__evt_sending)(p_owner->__p_client, _actSendData)) {
			if (p_owner->__state == UTS_IDLE) {
				p_owner->__state = UTS_SENDING;
				if (p_owner->act_start_transfer) {
					if (!(*p_owner->act_start_transfer)(p_owner)) p_owner->__state = UTS_IDLE;
				}
			}
		}
	}
}

BOOL pifUart_Init(PifUart* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifUart));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_frame_size = 1;
    return TRUE;
}

void pifUart_Clear(PifUart* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_rx_buffer) pifRingBuffer_Destroy(&p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Destroy(&p_owner->_p_tx_buffer);
}

BOOL pifUart_AllocRxBuffer(PifUart* p_owner, uint16_t rx_size, uint8_t threshold)
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

BOOL pifUart_AllocTxBuffer(PifUart* p_owner, uint16_t tx_size)
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

BOOL pifUart_SetFrameSize(PifUart* p_owner, uint8_t frame_size)
{
	switch (frame_size) {
	case 1:
	case 2:
	case 4:
	case 8:
		p_owner->_frame_size = frame_size;
		return TRUE;
	}
	return FALSE;
}

void pifUart_AttachClient(PifUart* p_owner, void* p_client, PifEvtUartParsing evt_parsing, PifEvtUartSending evt_sending)
{
	p_owner->__p_client = p_client;
	p_owner->__evt_parsing = evt_parsing;
	p_owner->__evt_sending = evt_sending;
	p_owner->_p_task->pause = FALSE;
}

void pifUart_DetachClient(PifUart* p_owner)
{
	p_owner->_p_task->pause = TRUE;
	if (p_owner->_p_rx_buffer) pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Empty(p_owner->_p_tx_buffer);
	p_owner->__p_client = NULL;
	p_owner->__evt_parsing = NULL;
	p_owner->__evt_sending = NULL;
}

uint16_t pifUart_GetRemainSizeOfRxBuffer(PifUart* p_owner)
{
	return pifRingBuffer_GetRemainSize(p_owner->_p_rx_buffer);
}

uint16_t pifUart_GetFillSizeOfTxBuffer(PifUart* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
}

BOOL pifUart_PutRxByte(PifUart* p_owner, uint8_t data)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutByte(p_owner->_p_rx_buffer, data)) return FALSE;
	if (pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) >= p_owner->__rx_threshold) {
		pifTask_SetTrigger(p_owner->_p_task);
	}
	return TRUE;
}

BOOL pifUart_PutRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutData(p_owner->_p_rx_buffer, p_data, length)) return FALSE;
	if (pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) >= p_owner->__rx_threshold) {
		pifTask_SetTrigger(p_owner->_p_task);
	}
	return TRUE;
}

uint8_t pifUart_GetTxByte(PifUart* p_owner, uint8_t* p_data)
{
	uint8_t ucState = PIF_UART_SEND_DATA_STATE_INIT;

    if (!p_owner->_p_tx_buffer) return ucState;

    ucState = pifRingBuffer_GetByte(p_owner->_p_tx_buffer, p_data);
	if (ucState) {
		if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) {
			ucState |= PIF_UART_SEND_DATA_STATE_EMPTY;
		}
	}
	else ucState |= PIF_UART_SEND_DATA_STATE_EMPTY;
	return ucState;
}

uint8_t pifUart_StartGetTxData(PifUart* p_owner, uint8_t** pp_data, uint16_t* p_length)
{
	uint16_t usLength;

    if (!p_owner->_p_tx_buffer) return PIF_UART_SEND_DATA_STATE_INIT;
    if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) return PIF_UART_SEND_DATA_STATE_EMPTY;

    *pp_data = pifRingBuffer_GetTailPointer(p_owner->_p_tx_buffer, 0);
    usLength = pifRingBuffer_GetLinerSize(p_owner->_p_tx_buffer, 0);
    if (!*p_length || usLength <= *p_length) *p_length = usLength;
	return PIF_UART_SEND_DATA_STATE_DATA;
}

uint8_t pifUart_EndGetTxData(PifUart* p_owner, uint16_t length)
{
    pifRingBuffer_Remove(p_owner->_p_tx_buffer, length);
	return pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer) << 1;
}

uint16_t pifUart_ReceiveRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
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

BOOL pifUart_SendTxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	if (p_owner->act_send_data) {
		return (*p_owner->act_send_data)(p_owner, p_data, length) == length;
	}
	else if (p_owner->_p_tx_buffer) {
		return pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_data, length);
	}
	return FALSE;
}

void pifUart_FinishTransfer(PifUart* p_owner)
{
	p_owner->__state = UTS_IDLE;
	pifTask_SetTrigger(p_owner->_p_task);
}

void pifUart_ForceSendData(PifUart* p_owner)
{
	if (p_owner->__evt_sending) _sendData(p_owner);
}

void pifUart_AbortRx(PifUart* p_owner)
{
	pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->evt_abort_rx) (*p_owner->evt_abort_rx)(p_owner->__p_client);
}

static uint16_t _doTask(PifTask* p_task)
{
	PifUart *p_owner = p_task->_p_client;

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

PifTask* pifUart_AttachTask(PifUart* p_owner, PifTaskMode mode, uint16_t period, const char* name)
{
	p_owner->_p_task = pifTaskManager_Add(mode, period, _doTask, p_owner, FALSE);
	if (p_owner->_p_task) {
		if (name) p_owner->_p_task->name = name;
		else p_owner->_p_task->name = "Uart";
	}
	return p_owner->_p_task;
}

