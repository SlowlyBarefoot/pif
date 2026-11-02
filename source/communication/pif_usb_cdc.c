// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_usb_cdc.h"

#include <string.h>

#ifdef PIF_USE_TINYUSB
	#include "tusb.h"
#endif


// Takes the TX buffer away while the port is closed, counting the bytes.
static void _drop(PifUsbCdc *p_owner)
{
	PifUart *p_uart = &p_owner->parent;
	uint8_t *p_data;
	uint16_t length;

	for (;;) {
		length = 0;
		if (!(pifUart_StartGetTxData(p_uart, &p_data, &length) & PIF_UART_SEND_DATA_STATE_DATA)) break;
		p_owner->_tx_dropped += length;
		if (pifUart_EndGetTxData(p_uart, length) & PIF_UART_SEND_DATA_STATE_EMPTY) break;
	}
}

// act_start_transfer: hands the stack packets while it has room and returns FALSE, so the transfer counts as
// done and the TX task comes back for the rest. The bytes are taken out of the TX buffer as the stack takes
// them, so the TX buffer is the only one that ever waits.
static BOOL _actStartTransfer(PifUart *p_uart)
{
	PifUsbCdc *p_owner = (PifUsbCdc *)p_uart;
	uint8_t *p_data;
	uint16_t length, sent, room;
	BOOL any = FALSE;

	if (!pifUsbCdc_IsOpen(p_owner)) {
		if (!p_owner->hold_while_closed) _drop(p_owner);
		return FALSE;
	}

	for (;;) {
		room = p_owner->act_get_room ? (*p_owner->act_get_room)(p_owner) : PIF_USB_CDC_PACKET_SIZE;
		if (!room) break;
		length = room < PIF_USB_CDC_PACKET_SIZE ? room : PIF_USB_CDC_PACKET_SIZE;
		if (!(pifUart_StartGetTxData(p_uart, &p_data, &length) & PIF_UART_SEND_DATA_STATE_DATA)) break;
		memcpy(p_owner->_packet, p_data, length);
		sent = (*p_owner->act_write)(p_owner, p_owner->_packet, length);
		if (sent) any = TRUE;
		if (pifUart_EndGetTxData(p_uart, sent) & PIF_UART_SEND_DATA_STATE_EMPTY) break;
		if (sent < length) break;
	}
	if (any && p_owner->act_flush) (*p_owner->act_flush)(p_owner);
	return FALSE;
}

// Without an RX buffer the RX task polls the stack through act_read.
static uint16_t _actReceiveData(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
	PifUsbCdc *p_owner = (PifUsbCdc *)p_uart;

	return p_owner->act_read ? (*p_owner->act_read)(p_owner, p_data, size) : 0;
}

BOOL pifUsbCdc_Init(PifUsbCdc *p_owner, PifId id, uint16_t tx_size, uint16_t rx_size)
{
	if (!p_owner || !tx_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifUsbCdc));
	if (!pifUart_Init(&p_owner->parent, id, PIF_USB_CDC_BAUDRATE)) return FALSE;
	if (!pifUart_AllocTxBuffer(&p_owner->parent, tx_size)) return FALSE;
	if (rx_size) {
		if (!pifUart_AllocRxBuffer(&p_owner->parent, rx_size)) return FALSE;
	}
	else {
		p_owner->parent.act_receive_data = _actReceiveData;
	}
	p_owner->parent.act_start_transfer = _actStartTransfer;
	return TRUE;
}

BOOL pifUsbCdc_SigReceive(PifUsbCdc *p_owner, uint8_t *p_data, uint16_t length)
{
	return pifUart_PutRxData(&p_owner->parent, p_data, length);
}

uint16_t pifUsbCdc_GetRxRoom(PifUsbCdc *p_owner)
{
	return p_owner->parent._p_rx_buffer ? pifRingBuffer_GetRemainSize(p_owner->parent._p_rx_buffer) : 0;
}

void pifUsbCdc_SigTxComplete(PifUsbCdc *p_owner)
{
	if (p_owner->parent._p_tx_task && !pifRingBuffer_IsEmpty(p_owner->parent._p_tx_buffer)) {
		pifTask_SetTrigger(p_owner->parent._p_tx_task, 0);
	}
}

BOOL pifUsbCdc_IsOpen(PifUsbCdc *p_owner)
{
	return p_owner->act_is_open ? (*p_owner->act_is_open)(p_owner) : TRUE;
}

#ifdef PIF_USE_TINYUSB

static uint16_t _tusbGetRoom(PifUsbCdc *p_cdc)
{
	uint32_t room = tud_cdc_n_write_available(p_cdc->itf);

	return room > 0xFFFF ? 0xFFFF : (uint16_t)room;
}

static uint16_t _tusbWrite(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size)
{
	return (uint16_t)tud_cdc_n_write(p_cdc->itf, p_data, size);
}

static void _tusbFlush(PifUsbCdc *p_cdc)
{
	tud_cdc_n_write_flush(p_cdc->itf);
}

static uint16_t _tusbRead(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size)
{
	return tud_cdc_n_available(p_cdc->itf) ? (uint16_t)tud_cdc_n_read(p_cdc->itf, p_data, size) : 0;
}

static BOOL _tusbIsOpen(PifUsbCdc *p_cdc)
{
	return tud_cdc_n_connected(p_cdc->itf);
}

BOOL pifUsbCdc_InitTinyUsb(PifUsbCdc *p_owner, PifId id, uint8_t itf, uint16_t tx_size)
{
	if (!pifUsbCdc_Init(p_owner, id, tx_size, 0)) return FALSE;
	p_owner->itf = itf;
	p_owner->act_get_room = _tusbGetRoom;
	p_owner->act_write = _tusbWrite;
	p_owner->act_flush = _tusbFlush;
	p_owner->act_read = _tusbRead;
	p_owner->act_is_open = _tusbIsOpen;
	return TRUE;
}

#endif	// PIF_USE_TINYUSB
