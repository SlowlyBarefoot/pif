// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_can.h"


// Data length of each DLC code by the CAN FD table.
static const uint8_t c_dlc_length[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };


static BOOL _isValidLength(uint8_t length, BOOL fd)
{
	if (length > PIF_CAN_MAX_DATA_SIZE) return FALSE;
	if (length <= 8) return TRUE;
	if (!fd) return FALSE;
	return c_dlc_length[pifCan_LengthToDlc(length)] == length;
}

static BOOL _checkFrame(const PifCanFrame *p_frame)
{
	uint32_t mask = (p_frame->flags & CAN_F_EXTENDED) ? PIF_CAN_EXT_ID_MASK : PIF_CAN_STD_ID_MASK;

	if (p_frame->id > mask) return FALSE;
	if (p_frame->flags & CAN_F_FD) {
		if (p_frame->flags & CAN_F_RTR) return FALSE;
	}
	else {
		if (p_frame->flags & CAN_F_BRS) return FALSE;
	}
	return _isValidLength(p_frame->length, p_frame->flags & CAN_F_FD);
}

static void _copyFrame(PifCanFrame *p_dst, const PifCanFrame *p_src)
{
	p_dst->id = p_src->id;
	p_dst->flags = p_src->flags;
	p_dst->length = p_src->length;
	// A remote frame carries a length but no data.
	if (!(p_src->flags & CAN_F_RTR)) memcpy(p_dst->data, p_src->data, p_src->length);
}

// The frame is complete before it is committed, so a reader in another context never sees half of it.
static BOOL _putQueue(PifRingData *p_queue, const PifCanFrame *p_frame)
{
	PifCanFrame *p_slot = pifRingData_BeginAdd(p_queue);

	if (!p_slot) return FALSE;
	_copyFrame(p_slot, p_frame);
	pifRingData_CommitAdd(p_queue);
	return TRUE;
}

// The frame stays in the queue until pifRingData_Remove(), so the writer cannot reuse its slot meanwhile.
static PifCanFrame *_peekQueue(PifRingData *p_queue)
{
	return pifRingData_GetData(p_queue, 0);
}


// Earlier of two delays, where 0 means none.
static uint32_t _minDelay(uint32_t delay, uint32_t other)
{
	if (!delay) return other;
	if (!other) return delay;
	return other < delay ? other : delay;
}

static uint32_t _doRxTask(PifTask *p_task)
{
	PifCan *p_owner = p_task->_p_client;
	PifCanFrame *p_frame;
	PifCanClient *p_client;
	uint16_t count = p_owner->__rx_queue._data_count;
	uint32_t period = 0;

	// Frames put while this runs wait for the next run, so a steady flow of frames cannot hold the task here.
	while (count-- && (p_frame = _peekQueue(&p_owner->__rx_queue))) {
		for (p_client = p_owner->__p_clients; p_client; p_client = p_client->__p_next) {
			if (p_client->evt_receive && (*p_client->evt_receive)(p_client, p_frame)) break;
		}
		if (!p_client && p_owner->evt_receive) (*p_owner->evt_receive)(p_owner, p_frame);
		pifRingData_Remove(&p_owner->__rx_queue);
	}

	for (p_client = p_owner->__p_clients; p_client; p_client = p_client->__p_next) {
		if (p_client->evt_rx_done) period = _minDelay(period, (*p_client->evt_rx_done)(p_client));
	}
	if (!pifRingData_IsEmpty(&p_owner->__rx_queue)) period = 1;
	return period;
}

static uint32_t _doTxTask(PifTask *p_task)
{
	PifCan *p_owner = p_task->_p_client;
	PifCanFrame *p_frame;
	PifCanClient *p_client;
	uint32_t period = 0;

	// pifCan_SigBusState() triggers the task again when the bus recovers.
	if (p_owner->_bus_state == CBS_BUS_OFF) return 0;

	for (p_client = p_owner->__p_clients; p_client; p_client = p_client->__p_next) {
		if (p_client->evt_sending) period = _minDelay(period, (*p_client->evt_sending)(p_client));
	}

	while ((p_frame = _peekQueue(&p_owner->__tx_queue))) {
		if (!p_owner->act_send_frame || !(*p_owner->act_send_frame)(p_owner, p_frame)) {
			// Tried again after one frame, or earlier by pifCan_SigTxComplete(). The clients wait as long,
			// since the queue they write to is not moving.
			period = pifCan_GetFrameTime(p_owner, p_frame->flags, p_frame->length);
			break;
		}
#ifdef PIF_USE_COMM_TAP
		if (p_owner->evt_tap) (*p_owner->evt_tap)(p_owner, TD_TX, p_frame);
#endif
		pifRingData_Remove(&p_owner->__tx_queue);
	}
	return period;
}


BOOL pifCan_Init(PifCan *p_owner, PifId id, uint32_t bitrate, uint32_t data_bitrate, uint16_t rx_count,
		uint16_t tx_count)
{
	if (!p_owner || !bitrate) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifCan));

	if (!pifRingData_Init(&p_owner->__rx_queue, PIF_ID_AUTO, sizeof(PifCanFrame), rx_count)) goto fail;
	if (!pifRingData_Init(&p_owner->__tx_queue, PIF_ID_AUTO, sizeof(PifCanFrame), tx_count)) goto fail;

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_bitrate = bitrate;
	p_owner->_data_bitrate = data_bitrate ? data_bitrate : bitrate;
	p_owner->_bus_state = CBS_ERROR_ACTIVE;
	return TRUE;

fail:
	pifCan_Clear(p_owner);
	return FALSE;
}

void pifCan_Clear(PifCan *p_owner)
{
	if (p_owner->_p_tx_task) {
		pifTaskManager_Remove(p_owner->_p_tx_task);
		p_owner->_p_tx_task = NULL;
	}
	if (p_owner->_p_rx_task) {
		pifTaskManager_Remove(p_owner->_p_rx_task);
		p_owner->_p_rx_task = NULL;
	}
	pifRingData_Clear(&p_owner->__rx_queue);
	pifRingData_Clear(&p_owner->__tx_queue);
}

BOOL pifCan_SetBitrate(PifCan *p_owner, uint32_t bitrate, uint32_t data_bitrate)
{
	if (!bitrate) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->_bitrate = bitrate;
	p_owner->_data_bitrate = data_bitrate ? data_bitrate : bitrate;
	return TRUE;
}

PifTask *pifCan_AttachRxTask(PifCan *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	if (p_owner->_p_rx_task) {
		pif_error = E_ALREADY_ATTACHED;
		return NULL;
	}

	p_owner->_p_rx_task = pifTaskManager_Add(id, mode, period, _doRxTask, p_owner, TRUE);
	if (p_owner->_p_rx_task) p_owner->_p_rx_task->name = name ? name : "CanRx";
	return p_owner->_p_rx_task;
}

PifTask *pifCan_AttachTxTask(PifCan *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	if (p_owner->_p_tx_task) {
		pif_error = E_ALREADY_ATTACHED;
		return NULL;
	}

	p_owner->_p_tx_task = pifTaskManager_Add(id, mode, period, _doTxTask, p_owner, TRUE);
	if (p_owner->_p_tx_task) p_owner->_p_tx_task->name = name ? name : "CanTx";
	return p_owner->_p_tx_task;
}

BOOL pifCan_SendFrame(PifCan *p_owner, const PifCanFrame *p_frame)
{
	if (!p_frame || !_checkFrame(p_frame)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (!_putQueue(&p_owner->__tx_queue, p_frame)) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	pifCan_TriggerTx(p_owner);
	return TRUE;
}

uint16_t pifCan_GetTxRemain(PifCan *p_owner)
{
	return pifRingData_GetRemainSize(&p_owner->__tx_queue);
}

void pifCan_TriggerTx(PifCan *p_owner)
{
	if (p_owner->_p_tx_task) pifTask_SetTrigger(p_owner->_p_tx_task, 0);
}

BOOL pifCan_PutRxFrame(PifCan *p_owner, const PifCanFrame *p_frame)
{
	if (p_frame->length > PIF_CAN_MAX_DATA_SIZE) return FALSE;

	if (!_putQueue(&p_owner->__rx_queue, p_frame)) {
		p_owner->_rx_lost++;
#ifdef PIF_USE_COMM_TAP
		if (p_owner->evt_tap) (*p_owner->evt_tap)(p_owner, TD_RX_LOST, p_frame);
#endif
		return FALSE;
	}
#ifdef PIF_USE_COMM_TAP
	if (p_owner->evt_tap) (*p_owner->evt_tap)(p_owner, TD_RX, p_frame);
#endif
	if (p_owner->_p_rx_task) pifTask_SetTrigger(p_owner->_p_rx_task, 0);
	return TRUE;
}

void pifCan_SigTxComplete(PifCan *p_owner)
{
	pifCan_TriggerTx(p_owner);
}

void pifCan_SigBusState(PifCan *p_owner, PifCanBusState state)
{
	if (state == p_owner->_bus_state) return;

	p_owner->_bus_state = state;
	if (state != CBS_BUS_OFF) pifCan_TriggerTx(p_owner);
	if (p_owner->evt_bus_state) (*p_owner->evt_bus_state)(p_owner, state);
}

uint32_t pifCan_GetFrameTime(PifCan *p_owner, uint8_t flags, uint8_t length)
{
	uint32_t nominal, data, crc, rate;

	if (!(flags & CAN_F_FD)) {
		// SOF to the end of the intermission: 47 bits with an 11 bit ID, 67 with a 29 bit one.
		nominal = (flags & CAN_F_EXTENDED) ? 67 : 47;
		if (!(flags & CAN_F_RTR)) nominal += 8UL * length;
		return (nominal * 1000000UL + p_owner->_bitrate - 1) / p_owner->_bitrate;
	}

	// Up to BRS, and from the CRC delimiter to the end of the intermission, at the nominal rate.
	nominal = ((flags & CAN_F_EXTENDED) ? 36 : 17) + 13;
	// ESI, DLC, data, stuff count, CRC and the fixed stuff bits of the CRC field, at the data rate.
	crc = length > 16 ? 21 : 17;
	data = 1 + 4 + 8UL * length + 4 + crc + (crc + 3) / 4;
	rate = (flags & CAN_F_BRS) ? p_owner->_data_bitrate : p_owner->_bitrate;
	return (nominal * 1000000UL + p_owner->_bitrate - 1) / p_owner->_bitrate + (data * 1000000UL + rate - 1) / rate;
}

uint8_t pifCan_DlcToLength(uint8_t dlc)
{
	return c_dlc_length[dlc & 0x0F];
}

uint8_t pifCan_LengthToDlc(uint8_t length)
{
	uint8_t dlc;

	if (length <= 8) return length;
	for (dlc = 9; dlc < 15 && c_dlc_length[dlc] < length; dlc++);
	return dlc;
}

BOOL pifCan_AddClient(PifCan *p_owner, PifCanClient *p_client)
{
	PifCanClient **pp_link;

	for (pp_link = &p_owner->__p_clients; *pp_link; pp_link = &(*pp_link)->__p_next) {
		if (*pp_link == p_client) {
			pif_error = E_ALREADY_ATTACHED;
			return FALSE;
		}
	}
	p_client->__p_next = NULL;
	*pp_link = p_client;
	return TRUE;
}

void pifCan_RemoveClient(PifCan *p_owner, PifCanClient *p_client)
{
	PifCanClient **pp_link;

	for (pp_link = &p_owner->__p_clients; *pp_link; pp_link = &(*pp_link)->__p_next) {
		if (*pp_link == p_client) {
			*pp_link = p_client->__p_next;
			p_client->__p_next = NULL;
			return;
		}
	}
}
