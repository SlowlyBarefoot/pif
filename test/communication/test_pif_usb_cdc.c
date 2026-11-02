// SPDX-License-Identifier: BSD-3-Clause
// pifUsbCdc with act functions of a stack the test plays, and with PIF_USE_TINYUSB on a stand-in of TinyUSB
// (fake/tusb.h): packets of PIF_USB_CDC_PACKET_SIZE while the stack has room, what is sent while the port is
// closed dropped or held, and received bytes polled or handed over by the receive callback.
#include "communication/pif_usb_cdc.h"
#ifdef PIF_USE_TINYUSB
	#include "tusb.h"
#endif

#include <stdio.h>
#include <string.h>


#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;
static uint32_t s_now;
static PifUsbCdc s_cdc;

// The stack
static BOOL s_open = TRUE;
static uint16_t s_room = 1000;
static uint8_t s_host[1024];		// what reached the host
static uint16_t s_host_size;
static uint16_t s_writes[32];		// size of every write
static int s_write_count, s_flush_count;
static uint8_t s_rx[16];			// bytes the host sent, for act_read
static uint16_t s_rx_fill;


static uint32_t _timer1us(void)
{
	return s_now;
}

static BOOL _portSetup(void)
{
	return pifTaskManager_Init(4, 0);
}

static uint16_t _write(const uint8_t *p_data, uint16_t size)
{
	if (size > s_room) size = s_room;
	memcpy(s_host + s_host_size, p_data, size);
	s_host_size += size;
	s_room -= size;
	if (s_write_count < 32) s_writes[s_write_count++] = size;
	return size;
}

static uint16_t _read(uint8_t *p_data, uint16_t size)
{
	if (size > s_rx_fill) size = s_rx_fill;
	memcpy(p_data, s_rx, size);
	memmove(s_rx, s_rx + size, s_rx_fill - size);
	s_rx_fill -= size;
	return size;
}

#ifdef PIF_USE_TINYUSB

uint32_t tud_cdc_n_write_available(uint8_t itf) { return itf == 1 ? s_room : 0; }
uint32_t tud_cdc_n_write(uint8_t itf, const void *buffer, uint32_t bufsize) { return _write(buffer, bufsize); }
uint32_t tud_cdc_n_write_flush(uint8_t itf) { s_flush_count++; return 0; }
uint32_t tud_cdc_n_available(uint8_t itf) { return s_rx_fill; }
uint32_t tud_cdc_n_read(uint8_t itf, void *buffer, uint32_t bufsize) { return _read(buffer, bufsize); }
bool tud_cdc_n_connected(uint8_t itf) { return s_open; }

#else

static uint16_t _actGetRoom(PifUsbCdc *p_cdc) { return s_room; }
static uint16_t _actWrite(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size) { return _write(p_data, size); }
static void _actFlush(PifUsbCdc *p_cdc) { s_flush_count++; }
static uint16_t _actRead(PifUsbCdc *p_cdc, uint8_t *p_data, uint16_t size) { return _read(p_data, size); }
static BOOL _actIsOpen(PifUsbCdc *p_cdc) { return s_open; }

#endif

static void _runTx(void)
{
	PifTask *p_task = s_cdc.parent._p_tx_task;

	(*p_task->__evt_loop)(p_task);
}

static void _reset(void)
{
	s_host_size = 0;
	s_write_count = 0;
	s_flush_count = 0;
	s_room = 1000;
}

static void _testInit(void)
{
	CHECK(pif_Init(_timer1us, _portSetup, NULL));
	CHECK(!pifUsbCdc_Init(&s_cdc, PIF_ID_AUTO, 0, 0) && pif_error == E_INVALID_PARAM);
#ifdef PIF_USE_TINYUSB
	CHECK(pifUsbCdc_InitTinyUsb(&s_cdc, PIF_ID_AUTO, 1, 256));
	CHECK(s_cdc.itf == 1 && s_cdc.act_write && s_cdc.act_read && s_cdc.act_is_open);
#else
	CHECK(pifUsbCdc_Init(&s_cdc, PIF_ID_AUTO, 256, 0));
	s_cdc.act_get_room = _actGetRoom;
	s_cdc.act_write = _actWrite;
	s_cdc.act_flush = _actFlush;
	s_cdc.act_read = _actRead;
	s_cdc.act_is_open = _actIsOpen;
#endif
	CHECK(s_cdc.parent._transfer_time == 1);
	CHECK(pifUart_AttachTxTask(&s_cdc.parent, PIF_ID_AUTO, TM_PERIOD, 1000, "CdcTx"));
}

static void _testTx(void)
{
	uint8_t data[150];
	int i;

	// Packets of 64 bytes at most, one flush after them.
	for (i = 0; i < (int)sizeof(data); i++) data[i] = (uint8_t)(i + 1);
	CHECK(pifUart_SendTxData(&s_cdc.parent, data, sizeof(data)) == sizeof(data));
	_runTx();
	CHECK(s_host_size == sizeof(data) && memcmp(s_host, data, sizeof(data)) == 0);
	for (i = 0; i < s_write_count; i++) CHECK(s_writes[i] <= PIF_USB_CDC_PACKET_SIZE);
	CHECK(s_flush_count == 1);
	CHECK(pifUart_GetFillSizeOfTxBuffer(&s_cdc.parent) == 0);

	// A busy stack takes nothing; the bytes wait and go when it has room, nothing flushed for nothing.
	_reset();
	s_room = 0;
	CHECK(pifUart_SendTxData(&s_cdc.parent, data, 100) == 100);
	_runTx();
	CHECK(s_host_size == 0 && s_flush_count == 0);
	s_room = 30;									// room for less than a packet
	_runTx();
	CHECK(s_host_size == 30 && pifUart_GetFillSizeOfTxBuffer(&s_cdc.parent) == 70);
	s_room = 1000;
	pifUsbCdc_SigTxComplete(&s_cdc);
	_runTx();
	CHECK(s_host_size == 100 && memcmp(s_host, data, 100) == 0);

	// Closed: dropped and counted, or held.
	_reset();
	s_open = FALSE;
	CHECK(!pifUsbCdc_IsOpen(&s_cdc));
	CHECK(pifUart_SendTxData(&s_cdc.parent, data, 20) == 20);
	_runTx();
	CHECK(s_host_size == 0 && s_cdc._tx_dropped == 20);
	CHECK(pifUart_GetFillSizeOfTxBuffer(&s_cdc.parent) == 0);
	s_cdc.hold_while_closed = TRUE;
	CHECK(pifUart_SendTxData(&s_cdc.parent, data, 20) == 20);
	_runTx();
	CHECK(s_host_size == 0 && s_cdc._tx_dropped == 20);
	CHECK(pifUart_GetFillSizeOfTxBuffer(&s_cdc.parent) == 20);
	s_open = TRUE;
	_runTx();
	CHECK(s_host_size == 20 && memcmp(s_host, data, 20) == 0);
}

static void _testRx(void)
{
	PifUsbCdc pushed;
	uint8_t data[16];

	// Polled through act_read.
	memcpy(s_rx, "ping", 4);
	s_rx_fill = 4;
	CHECK(pifUart_ReceiveRxData(&s_cdc.parent, data, sizeof(data)) == 4);
	CHECK(memcmp(data, "ping", 4) == 0);
	CHECK(pifUart_ReceiveRxData(&s_cdc.parent, data, sizeof(data)) == 0);

	// Handed over by the receive callback, all or none.
	CHECK(pifUsbCdc_Init(&pushed, PIF_ID_AUTO, 32, 8));
	CHECK(pushed.parent.act_receive_data == NULL);
	CHECK(pifUsbCdc_GetRxRoom(&pushed) >= 5);
	CHECK(pifUsbCdc_SigReceive(&pushed, (uint8_t *)"pong!", 5));
	CHECK(!pifUsbCdc_SigReceive(&pushed, (uint8_t *)"too long", 8));
	CHECK(pifUart_ReceiveRxData(&pushed.parent, data, sizeof(data)) == 5);
	CHECK(memcmp(data, "pong!", 5) == 0);
	CHECK(pifUsbCdc_GetRxRoom(&s_cdc) == 0);			// polled: no RX buffer
}

int main(void)
{
#ifdef PIF_USE_TINYUSB
	printf("test_pif_usb_cdc (TinyUSB)\n");
#else
	printf("test_pif_usb_cdc\n");
#endif
	_testInit();
	if (!s_failed) _testTx();
	if (!s_failed) _testRx();
	printf("%s\n", s_failed ? "FAILED" : "  ok   all");
	return s_failed ? 1 : 0;
}
