// SPDX-License-Identifier: BSD-3-Clause
// pifRtt on a stand-in of SEGGER RTT (fake/SEGGER_RTT.h): the TX task copies only what the up buffer has room
// for and leaves the rest for its next run, and received bytes are read from the down buffer.
#include "communication/pif_rtt.h"
#include "SEGGER_RTT.h"

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

#define UP_SIZE			16

static BOOL s_failed;
static uint32_t s_now;
static PifRtt s_rtt;

static uint8_t s_up[UP_SIZE];		// what the probe has not read yet
static unsigned s_up_fill;
static unsigned s_up_index;			// buffer index of the last write
static uint8_t s_probe[256];		// what the probe read
static unsigned s_probe_size;
static uint8_t s_down[16];
static unsigned s_down_fill;
static int s_init_calls, s_config_up, s_config_down;


void SEGGER_RTT_Init(void)
{
	s_init_calls++;
}

unsigned SEGGER_RTT_GetAvailWriteSpace(unsigned BufferIndex)
{
	return UP_SIZE - s_up_fill;
}

unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void *pBuffer, unsigned NumBytes)
{
	if (NumBytes > UP_SIZE - s_up_fill) NumBytes = UP_SIZE - s_up_fill;
	memcpy(s_up + s_up_fill, pBuffer, NumBytes);
	s_up_fill += NumBytes;
	s_up_index = BufferIndex;
	return NumBytes;
}

unsigned SEGGER_RTT_Read(unsigned BufferIndex, void *pBuffer, unsigned BufferSize)
{
	if (BufferSize > s_down_fill) BufferSize = s_down_fill;
	memcpy(pBuffer, s_down, BufferSize);
	memmove(s_down, s_down + BufferSize, s_down_fill - BufferSize);
	s_down_fill -= BufferSize;
	return BufferSize;
}

int SEGGER_RTT_ConfigUpBuffer(unsigned BufferIndex, const char *sName, void *pBuffer, unsigned BufferSize, unsigned Flags)
{
	s_config_up++;
	return BufferIndex == 2 ? 0 : -1;
}

int SEGGER_RTT_ConfigDownBuffer(unsigned BufferIndex, const char *sName, void *pBuffer, unsigned BufferSize, unsigned Flags)
{
	s_config_down++;
	return 0;
}

static uint32_t _timer1us(void)
{
	return s_now;
}

static BOOL _portSetup(void)
{
	return pifTaskManager_Init(4, 0);
}

// The probe reads the up buffer.
static void _probeRead(void)
{
	memcpy(s_probe + s_probe_size, s_up, s_up_fill);
	s_probe_size += s_up_fill;
	s_up_fill = 0;
}

static void _runTx(void)
{
	PifTask *p_task = s_rtt.parent._p_tx_task;

	(*p_task->__evt_loop)(p_task);
}

static void _testInit(void)
{
	uint8_t up[8], down[8];
	PifRtt other;

	CHECK(pif_Init(_timer1us, _portSetup, NULL));
	CHECK(!pifRtt_Init(&s_rtt, PIF_ID_AUTO, SEGGER_RTT_MAX_NUM_UP_BUFFERS, 64) && pif_error == E_INVALID_PARAM);
	CHECK(!pifRtt_Init(&s_rtt, PIF_ID_AUTO, 1, 0) && pif_error == E_INVALID_PARAM);
	CHECK(pifRtt_Init(&s_rtt, PIF_ID_AUTO, 1, 64));
	CHECK(s_init_calls == 1);
	CHECK(s_rtt._index == 1);
	CHECK(s_rtt.parent._p_tx_buffer != NULL);
	CHECK(s_rtt.parent._transfer_time == 10);
	CHECK(pifUart_AttachTxTask(&s_rtt.parent, PIF_ID_AUTO, TM_PERIOD, 1000, "RttTx"));

	// The up buffer of channel 1 is refused by the stand-in; a NULL buffer leaves a direction alone.
	CHECK(!pifRtt_ConfigBuffers(&s_rtt, "cap", up, sizeof(up), down, sizeof(down)) && pif_error == E_INVALID_PARAM);
	CHECK(s_config_up == 1 && s_config_down == 0);
	CHECK(pifRtt_ConfigBuffers(&s_rtt, "cap", NULL, 0, down, sizeof(down)));
	CHECK(s_config_up == 1 && s_config_down == 1);
	CHECK(pifRtt_Init(&other, PIF_ID_AUTO, 2, 16));
	CHECK(pifRtt_ConfigBuffers(&other, "two", up, sizeof(up), NULL, 0));
}

static void _testTx(void)
{
	uint8_t data[50];
	int i;

	for (i = 0; i < (int)sizeof(data); i++) data[i] = (uint8_t)(i * 7);
	CHECK(pifUart_SendTxData(&s_rtt.parent, data, sizeof(data)) == sizeof(data));

	// Each run takes what the up buffer has room for; nothing is lost while the probe is slow.
	_runTx();
	CHECK(s_up_fill == UP_SIZE && s_up_index == 1);
	CHECK(pifUart_GetFillSizeOfTxBuffer(&s_rtt.parent) == sizeof(data) - UP_SIZE);
	_runTx();										// the probe did not read: nothing moves
	CHECK(pifUart_GetFillSizeOfTxBuffer(&s_rtt.parent) == sizeof(data) - UP_SIZE);
	for (i = 0; i < 10 && pifUart_GetFillSizeOfTxBuffer(&s_rtt.parent); i++) {
		_probeRead();
		_runTx();
	}
	_probeRead();
	CHECK(s_probe_size == sizeof(data));
	CHECK(memcmp(s_probe, data, sizeof(data)) == 0);

	// The buffer wraps: the next transfer still comes out whole.
	s_probe_size = 0;
	CHECK(pifUart_SendTxData(&s_rtt.parent, data, 40) == 40);
	for (i = 0; i < 10 && pifUart_GetFillSizeOfTxBuffer(&s_rtt.parent); i++) {
		_runTx();
		_probeRead();
	}
	CHECK(s_probe_size == 40 && memcmp(s_probe, data, 40) == 0);
}

static void _testRx(void)
{
	uint8_t data[8];

	memcpy(s_down, "hello", 5);
	s_down_fill = 5;
	CHECK(pifUart_ReceiveRxData(&s_rtt.parent, data, sizeof(data)) == 5);
	CHECK(memcmp(data, "hello", 5) == 0);
	CHECK(pifUart_ReceiveRxData(&s_rtt.parent, data, sizeof(data)) == 0);
}

int main(void)
{
	printf("test_pif_rtt\n");
	_testInit();
	if (!s_failed) _testTx();
	if (!s_failed) _testRx();
	printf("%s\n", s_failed ? "FAILED" : "  ok   all");
	return s_failed ? 1 : 0;
}
