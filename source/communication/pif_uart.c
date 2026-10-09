// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "communication/pif_uart.h"


static void _setFcState(PifUart* p_owner, SWITCH state)
{
	p_owner->_fc_state = state;
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[UA_CSF_FC_IDX], state);
#endif
}

static void _setTxState(PifUart* p_owner, PifUartTxState state)
{
	p_owner->__tx_state = state;
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[UA_CSF_TX_IDX], state);
#endif
}


static BOOL _isTxBlocked(PifUart* p_owner)
{
	// With host flow control, _fc_state is OFF while the other side asks us not to send.
	return (p_owner->_flow_control & UFC_HOST_MASK) && !p_owner->_fc_state;
}

static uint16_t _filterXonXoff(PifUart* p_owner, uint8_t* p_data, uint16_t len)
{
	uint16_t i, count = 0;

	// XON / XOFF are control characters, so they are taken out of the data given to the client.
	for (i = 0; i < len; i++) {
		switch (p_data[i]) {
		case ASCII_XON:
			_setFcState(p_owner, ON);
			if (p_owner->_p_tx_task) pifTask_SetTrigger(p_owner->_p_tx_task, 0);
			if (p_owner->__evt_host_flow_state) (*p_owner->__evt_host_flow_state)(p_owner->__p_client, ON);
			break;

		case ASCII_XOFF:
			_setFcState(p_owner, OFF);
			if (p_owner->__evt_host_flow_state) (*p_owner->__evt_host_flow_state)(p_owner->__p_client, OFF);
			break;

		default:
			p_data[count++] = p_data[i];
			break;
		}
	}
	return count;
}

static uint16_t _actReceiveData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t len;

	// Read again when every byte read was XON / XOFF, so that a return of 0 still means no data.
	do {
		if (p_owner->act_receive_data) {
			len = (*p_owner->act_receive_data)(p_owner, p_data, length);
			if (!len) return 0;
			else if (p_owner->__rx_state == URS_IDLE) p_owner->__rx_state = URS_FIRST;
		}
		else if (p_owner->_p_rx_buffer) {
			len = pifRingBuffer_GetBytes(p_owner->_p_rx_buffer, p_data, length);
			if (!len) return 0;
		}
		else {
			return 0;
		}

		if (p_owner->_flow_control == UFC_HOST_SOFTWARE) len = _filterXonXoff(p_owner, p_data, len);
	} while (!len);

	return len;
}

static uint16_t _actSendData(PifUart* p_owner, uint8_t* p_data, uint16_t size)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner->_p_tx_buffer);

	if (!remain) return 0;
	if (size > remain) size = remain;
	if (pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_data, size)) {
		return size;
	}
	return 0;
}

static void _sendXonXoff(PifUart* p_owner, uint8_t data)
{
	if (p_owner->act_send_data) {
		(*p_owner->act_send_data)(p_owner, &data, 1);
	}
	else if (p_owner->_p_tx_buffer) {
		if (p_owner->act_start_transfer) {
			pifRingBuffer_PutData(p_owner->_p_tx_buffer, &data, 1);
			if ((*p_owner->act_start_transfer)(p_owner)) _setTxState(p_owner, UTS_SENDING);
		}
	}
}

BOOL pifUart_Init(PifUart* p_owner, PifId id, uint32_t baudrate)
{
	if (!p_owner || !baudrate) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifUart));

	p_owner->fc_limit = 50;					// default: 50%
    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->_baudrate = baudrate;
    p_owner->_transfer_time = 1000000L * 10 / baudrate;
    p_owner->_frame_size = 1;
	_setFcState(p_owner, ON);
    return TRUE;
}

void pifUart_Clear(PifUart* p_owner)
{
#ifdef PIF_COLLECT_SIGNAL
	pifUart_ResetCsFlag(p_owner, UA_CSF_ALL_BIT);
#endif
	if (p_owner->_p_tx_task) {
		pifTaskManager_Remove(p_owner->_p_tx_task);
		p_owner->_p_tx_task = NULL;
	}
	if (p_owner->_p_rx_task) {
		pifTaskManager_Remove(p_owner->_p_rx_task);
		p_owner->_p_rx_task = NULL;
	}
	if (p_owner->_p_rx_buffer) pifRingBuffer_Destroy(&p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Destroy(&p_owner->_p_tx_buffer);
}

BOOL pifUart_AllocRxBuffer(PifUart* p_owner, uint16_t rx_size)
{
    if (!rx_size) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->_p_rx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, rx_size);
    if (!p_owner->_p_rx_buffer) return FALSE;
    pifRingBuffer_SetName(p_owner->_p_rx_buffer, "RB");
    return TRUE;
}

BOOL pifUart_AssignRxBuffer(PifUart* p_owner, uint16_t rx_size, uint8_t* p_buffer)
{
    if (!rx_size || !p_buffer) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

    p_owner->_p_rx_buffer = pifRingBuffer_CreateStatic(PIF_ID_AUTO, rx_size, p_buffer);
    if (!p_owner->_p_rx_buffer) return FALSE;
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

BOOL pifUart_AssignTxBuffer(PifUart* p_owner, uint16_t tx_size, uint8_t* p_buffer)
{
	if (!tx_size || !p_buffer) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    p_owner->_p_tx_buffer = pifRingBuffer_CreateStatic(PIF_ID_AUTO, tx_size, p_buffer);
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

BOOL pifUart_ChangeBaudrate(PifUart* p_owner, uint32_t baudrate)
{
	if (p_owner->act_set_baudrate) {
		if (!(*p_owner->act_set_baudrate)(p_owner, baudrate)) return FALSE;
	}
	p_owner->_baudrate = baudrate;
    p_owner->_transfer_time = 1000000L * 10 / baudrate;
	return TRUE;
}

void pifUart_AttachClient(PifUart* p_owner, void* p_client, PifEvtUartParsing evt_parsing, PifEvtUartSending evt_sending)
{
	p_owner->__p_client = p_client;
	if (p_owner->_p_rx_task) {
		p_owner->__evt_parsing = evt_parsing;
		p_owner->_p_rx_task->pause = FALSE;
	}
	if (p_owner->_p_tx_task) {
		p_owner->__evt_sending = evt_sending;
		p_owner->_p_tx_task->pause = FALSE;
	}
}

void pifUart_AttachActDirection(PifUart* p_owner, PifActUartDirection act_direction, PifUartDirection init_state)
{
	p_owner->__act_direction = act_direction;
	pifUart_SetDirection(p_owner, init_state);
}

void pifUart_SetDirection(PifUart* p_owner, PifUartDirection direction)
{
	if (!p_owner->__act_direction) return;

	(*p_owner->__act_direction)(direction);
#ifdef PIF_COLLECT_SIGNAL
	pifCollectSignal_Put(&p_owner->__cs[UA_CSF_DIR_IDX], direction);
#endif
}

void pifUart_DetachClient(PifUart* p_owner)
{
	if (p_owner->_p_rx_buffer) pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->_p_tx_buffer) pifRingBuffer_Empty(p_owner->_p_tx_buffer);
	if (p_owner->_p_rx_task) {
		p_owner->_p_rx_task->pause = TRUE;
		p_owner->__evt_parsing = NULL;
	}
	if (p_owner->_p_tx_task) {
		p_owner->_p_tx_task->pause = TRUE;
		p_owner->__evt_sending = NULL;
	}
	p_owner->__p_client = NULL;
}

void pifUart_ResetFlowControl(PifUart* p_owner)
{
	p_owner->_flow_control = UFC_NONE;
	p_owner->__evt_host_flow_state = NULL;
}

void pifUart_SetFlowControl(PifUart* p_owner, PifUartFlowControl flow_control, PifEvtUartHostFlowState evt_host_flow_state)
{
	p_owner->_flow_control = flow_control;
	p_owner->__evt_host_flow_state = evt_host_flow_state;
	_setFcState(p_owner, ON);
	if (flow_control == UFC_DEVICE_HARDWARE && p_owner->act_device_flow_state) (*p_owner->act_device_flow_state)(p_owner, ON);
}

BOOL pifUart_ChangeRxFlowState(PifUart* p_owner, SWITCH state)
{
	uint8_t data;

	if (p_owner->_flow_control == UFC_DEVICE_SOFTWARE) {
		data = state ? ASCII_XON : ASCII_XOFF;
		if (!pifUart_SendTxData(p_owner, &data, 1)) return FALSE;
	}
	else if (p_owner->_flow_control == UFC_DEVICE_HARDWARE) {
		if (!p_owner->act_device_flow_state) {
			pif_error = E_INVALID_STATE;
			return FALSE;
		}
		(*p_owner->act_device_flow_state)(p_owner, state);
	}
	else {
		return TRUE;
	}
	_setFcState(p_owner, state);
	return TRUE;
}

void pifUart_SigTxFlowState(PifUart* p_owner, SWITCH state)
{
	if (p_owner->_flow_control != UFC_HOST_HARDWARE) return;

	_setFcState(p_owner, state);
	if (state) {
		pifTask_SetTrigger(p_owner->_p_tx_task, 0);
	}
	if (p_owner->__evt_host_flow_state) {
		(*p_owner->__evt_host_flow_state)(p_owner->__p_client, state);
	}
}

uint16_t pifUart_GetFillSizeOfRxBuffer(PifUart* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer);
}

uint16_t pifUart_GetFillSizeOfTxBuffer(PifUart* p_owner)
{
	return pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
}

BOOL pifUart_PutRxByte(PifUart* p_owner, uint8_t data)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutByte(p_owner->_p_rx_buffer, data)) return FALSE;
	if (p_owner->__rx_state == URS_IDLE && p_owner->_p_rx_task) {
		p_owner->__rx_state = URS_FIRST;
		pifTask_SetTrigger(p_owner->_p_rx_task, 0);
	}
	return TRUE;
}

BOOL pifUart_PutRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	if (!p_owner->_p_rx_buffer) return FALSE;

	if (!pifRingBuffer_PutData(p_owner->_p_rx_buffer, p_data, length)) return FALSE;
	if (p_owner->__rx_state == URS_IDLE && p_owner->_p_rx_task) {
		p_owner->__rx_state = URS_FIRST;
		pifTask_SetTrigger(p_owner->_p_rx_task, 0);
	}
	return TRUE;
}

uint8_t pifUart_GetTxByte(PifUart* p_owner, uint8_t* p_data)
{
	uint8_t ucState;

    if (!p_owner->_p_tx_buffer) return PIF_UART_SEND_DATA_STATE_INIT;
    if (_isTxBlocked(p_owner)) {
    	// Stop here. The TX task starts the transfer again when the other side allows it.
		_setTxState(p_owner, UTS_IDLE);
    	return PIF_UART_SEND_DATA_STATE_EMPTY;
    }

    ucState = pifRingBuffer_GetByte(p_owner->_p_tx_buffer, p_data);
	if (ucState) {
		if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) {
			_setTxState(p_owner, UTS_IDLE);
			ucState |= PIF_UART_SEND_DATA_STATE_EMPTY;
		}
	}
	else {
		_setTxState(p_owner, UTS_IDLE);
		ucState |= PIF_UART_SEND_DATA_STATE_EMPTY;
	}
	return ucState;
}

uint8_t pifUart_StartGetTxData(PifUart* p_owner, uint8_t** pp_data, uint16_t* p_length)
{
	uint16_t usLength;

    if (!p_owner->_p_tx_buffer) return PIF_UART_SEND_DATA_STATE_INIT;
    if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer) || _isTxBlocked(p_owner)) {
		_setTxState(p_owner, UTS_IDLE);
    	return PIF_UART_SEND_DATA_STATE_EMPTY;
    }

    *pp_data = pifRingBuffer_GetReadPointer(p_owner->_p_tx_buffer, 0, &usLength);
    if (!*p_length || usLength <= *p_length) *p_length = usLength;
	return PIF_UART_SEND_DATA_STATE_DATA;
}

uint8_t pifUart_EndGetTxData(PifUart* p_owner, uint16_t length)
{
    pifRingBuffer_Remove(p_owner->_p_tx_buffer, length);
    if (pifRingBuffer_IsEmpty(p_owner->_p_tx_buffer)) {
		_setTxState(p_owner, UTS_IDLE);
    	return PIF_UART_SEND_DATA_STATE_EMPTY;
    }
    return PIF_UART_SEND_DATA_STATE_INIT;
}

uint16_t pifUart_ReceiveRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	return _actReceiveData(p_owner, p_data, length);
}

uint16_t pifUart_SendTxData(PifUart* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t len = 0;

	if (p_owner->act_send_data) {
		len = (*p_owner->act_send_data)(p_owner, p_data, length);
	}
	else if (p_owner->_p_tx_buffer) {
		len = pifRingBuffer_PutData(p_owner->_p_tx_buffer, p_data, length) ? length : 0;
	}
	if (len && p_owner->_p_tx_task) pifTask_SetTrigger(p_owner->_p_tx_task, 0);
	return len;
}

void pifUart_AbortRx(PifUart* p_owner)
{
	pifRingBuffer_Empty(p_owner->_p_rx_buffer);
	if (p_owner->evt_abort_rx) (*p_owner->evt_abort_rx)(p_owner->__p_client);
}

BOOL pifUart_CheckTxTransfer(PifUart* p_owner)
{
	if (p_owner->act_send_data) {
		if (!p_owner->act_get_tx_rate) return TRUE;
		if ((*p_owner->act_get_tx_rate)(p_owner) >= 100) return TRUE;
	}
	else if (p_owner->_p_tx_buffer) {
		if (!pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer)) return TRUE;
	}
	return FALSE;
}

static uint32_t _doRxTask(PifTask* p_task)
{
	PifUart *p_owner = p_task->_p_client;
	uint8_t data, rate, tx;
	BOOL rtn = FALSE;
	uint32_t period = 0;

#ifdef PIF_DEBUG
	if (pif_act_gpio_write) {
		(*pif_act_gpio_write)(p_owner->_id, 0);
	}
#endif

	if (p_owner->_flow_control & UFC_DEVICE_MASK) {
		if (p_owner->act_get_rx_rate) {
			rate = (*p_owner->act_get_rx_rate)(p_owner);
		}
		else if (p_owner->_p_rx_buffer) {
			rate = 100UL * pifRingBuffer_GetFillSize(p_owner->_p_rx_buffer) / p_owner->_p_rx_buffer->_size;
		}
		else goto next1;

		switch (p_owner->_flow_control) {
		case UFC_DEVICE_SOFTWARE:
			if (p_owner->_fc_state) {
				if (rate > p_owner->fc_limit) {
					_setFcState(p_owner, OFF);
					tx = ASCII_XOFF;
					_sendXonXoff(p_owner, tx);
				}
			}
			else {
				if (rate == 0) {
					_setFcState(p_owner, ON);
					tx = ASCII_XON;
					_sendXonXoff(p_owner, tx);
					period = 1;
				}
			}
			break;

		case UFC_DEVICE_HARDWARE:
			if (p_owner->act_device_flow_state) {
				if (p_owner->_fc_state) {
					if (rate > p_owner->fc_limit) {
						_setFcState(p_owner, OFF);
						(*p_owner->act_device_flow_state)(p_owner, OFF);
					}
				}
				else {
					if (rate == 0) {
						_setFcState(p_owner, ON);
						(*p_owner->act_device_flow_state)(p_owner, ON);
						period = 1;
					}
				}
			}
			break;

		default:
			break;
		}
	}

next1:
	if (p_owner->__evt_parsing) {
		rtn = (*p_owner->__evt_parsing)(p_owner->__p_client, _actReceiveData);
	}
	else {
		rtn = _actReceiveData(p_owner, &data, 1) > 0;
	}
	switch (p_owner->__rx_state) {
	case URS_FIRST:
		if (rtn) p_owner->__rx_state = URS_NEXT;
		period = 2 * p_owner->_transfer_time;
		break;

	case URS_NEXT:
		if (!rtn) {
			p_owner->__rx_state = URS_IDLE;
			// A byte put by the interrupt just before URS_IDLE was set did not trigger the task, since
			// the state was still URS_NEXT. Check the buffer after the change so such a byte is not left behind.
			if (p_owner->_p_rx_buffer && !pifRingBuffer_IsEmpty(p_owner->_p_rx_buffer)) {
				p_owner->__rx_state = URS_FIRST;
				period = 2 * p_owner->_transfer_time;
			}
		}
		else {
			period = 5 * p_owner->_transfer_time;
		}
		break;

	default:
		break;
	}
	return period;
}

static uint32_t _doTxTask(PifTask* p_task)
{
	PifUart *p_owner = p_task->_p_client;
	uint32_t bytes = 0;

#ifdef PIF_DEBUG
	if (pif_act_gpio_write) {
		(*pif_act_gpio_write)(p_owner->_id, 1);
	}
#endif

	if (_isTxBlocked(p_owner)) {
		// XON or the CTS signal triggers this task again.
	}
	else if (p_owner->act_send_data) {
		if (p_owner->__evt_sending) {
			bytes = (*p_owner->__evt_sending)(p_owner->__p_client, p_owner->act_send_data);
		}
	}
	else if (p_owner->_p_tx_buffer) {
		if (p_owner->__evt_sending) {
			bytes = (*p_owner->__evt_sending)(p_owner->__p_client, _actSendData);
		}
		else {
			bytes = pifRingBuffer_GetFillSize(p_owner->_p_tx_buffer);
		}
		if (bytes && p_owner->__tx_state == UTS_IDLE) {
			if (p_owner->act_start_transfer) {
				if ((*p_owner->act_start_transfer)(p_owner)) _setTxState(p_owner, UTS_SENDING);
			}
		}
	}
	return bytes * p_owner->_transfer_time;
}

PifTask *pifUart_AttachRxTask(PifUart *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

    if (p_owner->_p_rx_task) {
    	pif_error = E_ALREADY_ATTACHED;
	    return NULL;
    }

	p_owner->_p_rx_task = pifTaskManager_Add(id, mode, period, _doRxTask, p_owner, FALSE);
	if (p_owner->_p_rx_task) {
		if (name) p_owner->_p_rx_task->name = name;
		else p_owner->_p_rx_task->name = "UartRx";
	}
	return p_owner->_p_rx_task;
}

PifTask *pifUart_AttachTxTask(PifUart *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return NULL;
	}

    if (p_owner->_p_tx_task) {
    	pif_error = E_ALREADY_ATTACHED;
	    return NULL;
    }

	p_owner->_p_tx_task = pifTaskManager_Add(id, mode, period, _doTxTask, p_owner, FALSE);
	if (p_owner->_p_tx_task) {
		if (name) p_owner->_p_tx_task->name = name;
		else p_owner->_p_tx_task->name = "UartTx";
	}
	return p_owner->_p_tx_task;
}

#ifdef PIF_COLLECT_SIGNAL

BOOL pifUart_SetCsFlag(PifUart* p_owner, PifUartCsFlag flag)
{
	if (flag & UA_CSF_FC_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[UA_CSF_FC_IDX], "UFC", p_owner->_id, CSVT_WIRE, 1,
				p_owner->_fc_state)) return FALSE;
	}
	if (flag & UA_CSF_TX_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[UA_CSF_TX_IDX], "UTX", p_owner->_id, CSVT_WIRE, 1,
				p_owner->__tx_state)) return FALSE;
	}
	if (flag & UA_CSF_DIR_BIT) {
		// The last direction set is kept in the channel, since the UART does not store it.
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[UA_CSF_DIR_IDX], "UDR", p_owner->_id, CSVT_WIRE, 1,
				p_owner->__cs[UA_CSF_DIR_IDX]._value)) return FALSE;
	}
	return TRUE;
}

void pifUart_ResetCsFlag(PifUart* p_owner, PifUartCsFlag flag)
{
	if (flag & UA_CSF_FC_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[UA_CSF_FC_IDX]);
	if (flag & UA_CSF_TX_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[UA_CSF_TX_IDX]);
	if (flag & UA_CSF_DIR_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[UA_CSF_DIR_IDX]);
}

#endif	// PIF_COLLECT_SIGNAL
