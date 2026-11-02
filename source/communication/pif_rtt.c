// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_rtt.h"

#ifdef PIF_USE_SEGGER_RTT

#include "SEGGER_RTT.h"


// act_start_transfer: copies what the up buffer has room for and returns FALSE, so the transfer counts as done
// and the TX task comes back for the rest after the time of the bytes left.
static BOOL _actStartTransfer(PifUart *p_uart)
{
	PifRtt *p_owner = (PifRtt *)p_uart;
	uint8_t *p_data;
	uint16_t length;
	unsigned room, sent;

	for (;;) {
		room = SEGGER_RTT_GetAvailWriteSpace(p_owner->_index);
		if (!room) break;
		length = room > 0xFFFF ? 0xFFFF : (uint16_t)room;
		if (!(pifUart_StartGetTxData(p_uart, &p_data, &length) & PIF_UART_SEND_DATA_STATE_DATA)) break;
		sent = SEGGER_RTT_Write(p_owner->_index, p_data, length);
		if (pifUart_EndGetTxData(p_uart, (uint16_t)sent) & PIF_UART_SEND_DATA_STATE_EMPTY) break;
		if (sent < length) break;
	}
	return FALSE;
}

static uint16_t _actReceiveData(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
	return (uint16_t)SEGGER_RTT_Read(((PifRtt *)p_uart)->_index, p_data, size);
}

BOOL pifRtt_Init(PifRtt *p_owner, PifId id, uint8_t index, uint16_t tx_size)
{
	if (!p_owner || index >= SEGGER_RTT_MAX_NUM_UP_BUFFERS || !tx_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!pifUart_Init(&p_owner->parent, id, PIF_RTT_BAUDRATE)) return FALSE;
	if (!pifUart_AllocTxBuffer(&p_owner->parent, tx_size)) return FALSE;
	p_owner->_index = index;
	p_owner->parent.act_start_transfer = _actStartTransfer;
	p_owner->parent.act_receive_data = _actReceiveData;
	SEGGER_RTT_Init();
	return TRUE;
}

BOOL pifRtt_ConfigBuffers(PifRtt *p_owner, const char *p_name, uint8_t *p_up, uint16_t up_size, uint8_t *p_down,
		uint16_t down_size)
{
	if (p_up && SEGGER_RTT_ConfigUpBuffer(p_owner->_index, p_name, p_up, up_size, SEGGER_RTT_MODE_NO_BLOCK_SKIP) < 0) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_down && SEGGER_RTT_ConfigDownBuffer(p_owner->_index, p_name, p_down, down_size,
			SEGGER_RTT_MODE_NO_BLOCK_SKIP) < 0) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

#endif	// PIF_USE_SEGGER_RTT
