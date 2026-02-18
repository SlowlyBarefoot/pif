#include "core/pif_collect_signal.h"
#include "core/pif_dlist.h"
#include "core/pif_log.h"

#include <string.h>

// Signal collection and periodic transfer support for debug/monitoring.

#ifdef PIF_COLLECT_SIGNAL

#define PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS	20


typedef enum EnPifCollectSignalStep
{
	CSS_IDLE		= 0,
	CSS_COLLECT		= 1
} PifCollectSignalStep;


typedef struct StPifCollectSignalDevice
{
	PifCollectSignalVarType var_type;
	uint8_t index;
	uint16_t size;
	uint16_t initial_value;
	char p_reference[10];
} PifCollectSignalDevice;

typedef struct StPifCollectSignal
{
	const char* p_module_name;
	PifCollectSignalScale scale;
	PifCollectSignalMethod method;
	PifCollectSignalStep step;
	PifTask* p_task;
	uint32_t transfer_period_1us;	// PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS
	uint8_t device_count;
	uint32_t timer;
	PifRingBuffer buffer;
	PifDList device;
	PifAddCollectSignalDevice add_device[32];
} PifCollectSignal;


static PifCollectSignal s_collect_signal;


static void _printHeader()
{
	const char *kVarType[] =
	{
			"event",
			"integer",
			"parameter",
			"real",
			"reg",
			"supply0",
			"supply1",
			"time",
			"tri",
			"triand",
			"trior",
			"triReg",
			"tri0",
			"tri1",
			"wand",
			"wire",
			"wor"
	};
	PifDListIterator it;

	pifLog_Printf(LT_VCD, "\n$date %s %u, %u %2u:%2u:%2u $end\n",
			kPifMonth3[pif_datetime.month - 1], pif_datetime.day, 2000 + pif_datetime.year,
			pif_datetime.hour, pif_datetime.minute, pif_datetime.second);

	pifLog_Printf(LT_VCD, "$version %u.%u.%u $end\n", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH);

	switch (s_collect_signal.scale) {
	case CSS_1S:
		pifLog_Printf(LT_VCD, "$timescale 1s $end\n");
		break;

	case CSS_1MS:
		pifLog_Printf(LT_VCD, "$timescale 1ms $end\n");
		break;

	case CSS_1US:
		pifLog_Printf(LT_VCD, "$timescale 1us $end\n");
		break;

	default:
		break;
	}

	pifLog_Printf(LT_VCD, "$scope module %s $end\n", s_collect_signal.p_module_name);
	it = pifDList_Begin(&s_collect_signal.device);
	while (it) {
		PifCollectSignalDevice* p_device = (PifCollectSignalDevice*)it->data;
		if (p_device->size == 1) {
			pifLog_Printf(LT_VCD, "$var %s 1 %c %s $end\n", kVarType[p_device->var_type], '!' + p_device->index, p_device->p_reference);
		}
		else {
			pifLog_Printf(LT_VCD, "$var %s %u %c %s[%u:0] $end\n", kVarType[p_device->var_type],
					p_device->size, '!' + p_device->index, p_device->p_reference, p_device->size - 1);
		}

		it = pifDList_Next(it);
	}
	pifLog_Printf(LT_VCD, "$upscope $end\n");
	pifLog_Printf(LT_VCD, "$enddefinitions $end\n");

	pifLog_Printf(LT_VCD, "$dumpvars\n");
	it = pifDList_Begin(&s_collect_signal.device);
	while (it) {
		PifCollectSignalDevice* p_device = (PifCollectSignalDevice*)it->data;
		if (p_device->size == 1) {
			pifLog_Printf(LT_VCD, "%u%c\n", p_device->initial_value, '!' + p_device->index);
		}
		else {
			pifLog_Printf(LT_VCD, "b%b %c\n", p_device->initial_value, '!' + p_device->index);
		}

		it = pifDList_Next(it);
	}
	pifLog_Printf(LT_VCD, "$end\n");
}

static uint32_t _doTask(PifTask* p_task)
{
	uint16_t length;
	uint8_t tmp_buf[64 + 1];

	if (!pifLog_IsEmpty()) return 0;

	if (!pifRingBuffer_IsEmpty(&s_collect_signal.buffer)) {
		length = pifRingBuffer_CopyToArray(tmp_buf, 64, &s_collect_signal.buffer, 0);
		tmp_buf[length] = 0;
		pifRingBuffer_Remove(&s_collect_signal.buffer, length);
		pifLog_Print(LT_VCD, (char *)tmp_buf);
	}
	else {
		p_task->pause = TRUE;
		pifLog_Enable();
	}
	return 0;
}

void pifCollectSignal_Init(const char* p_module_name)
{
	memset(&s_collect_signal, 0, sizeof(PifCollectSignal));

	s_collect_signal.p_module_name = p_module_name;
	s_collect_signal.scale = CSS_1MS;
	s_collect_signal.method = CSM_LOG;
	pifDList_Init(&s_collect_signal.device);
}

BOOL pifCollectSignal_InitHeap(const char *p_module_name, uint16_t size)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitHeap(&s_collect_signal.buffer, PIF_ID_AUTO, size)) goto fail;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;

	s_collect_signal.p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS * 1000, _doTask, &s_collect_signal, FALSE);
	if (s_collect_signal.p_task == NULL) goto fail;
    s_collect_signal.transfer_period_1us = PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS * 1000;
	s_collect_signal.p_task->name = "CollectSignalHeap";
	return TRUE;

fail:
	pifCollectSignal_Clear();
	return FALSE;
}

BOOL pifCollectSignal_InitStatic(const char *p_module_name, uint16_t size, uint8_t* p_buffer)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitStatic(&s_collect_signal.buffer, PIF_ID_AUTO, size, p_buffer)) goto fail;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;

	s_collect_signal.p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS * 1000, _doTask, &s_collect_signal, FALSE);
	if (s_collect_signal.p_task == NULL) goto fail;
    s_collect_signal.transfer_period_1us = PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS * 1000;
	s_collect_signal.p_task->name = "CollectSignalStatic";
	return TRUE;

fail:
	pifCollectSignal_Clear();
	return FALSE;
}

void pifCollectSignal_Clear()
{
	if (s_collect_signal.p_task) {
		pifTaskManager_Remove(s_collect_signal.p_task);
		s_collect_signal.p_task = NULL;
	}
	pifRingBuffer_Clear(&s_collect_signal.buffer);
    pifDList_Clear(&s_collect_signal.device, NULL);
}

uint32_t pifCollectSignal_GetTransferPeriod()
{
	return s_collect_signal.transfer_period_1us;
}

BOOL pifCollectSignal_SetTransferPeriod(uint16_t period1ms)
{
	if (!period1ms) {
        pif_error = E_INVALID_PARAM;
        return FALSE;
	}

	s_collect_signal.transfer_period_1us = period1ms * 1000;
    if (s_collect_signal.p_task) pifTask_ChangePeriod(s_collect_signal.p_task, s_collect_signal.transfer_period_1us);
	return TRUE;
}

BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale)
{
	if (s_collect_signal.step != CSS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (scale > CSS_1US) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	s_collect_signal.scale = scale;
	return TRUE;
}

void pifCollectSignal_ChangeFlag(uint8_t* p_flag, uint8_t index, uint8_t flag)
{
	if (index & 1) {
		p_flag[index / 2] &= 0x0F;
		if (flag) p_flag[index / 2] |= flag << 4;
	}
	else {
		p_flag[index / 2] &= 0xF0;
		if (flag) p_flag[index / 2] |= flag;
	}
}

BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method)
{
	if (s_collect_signal.step != CSS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	s_collect_signal.method = method;
	return TRUE;
}

void pifCollectSignal_Attach(PifCollectSignalFlag flag, PifAddCollectSignalDevice add_device)
{
	s_collect_signal.add_device[flag] = add_device;
}

void pifCollectSignal_Detach(PifCollectSignalFlag flag)
{
	s_collect_signal.add_device[flag] = NULL;
}

void* pifCollectSignal_AddDevice(PifId id, PifCollectSignalVarType var_type, uint16_t size,
		const char* p_reference, uint16_t initial_value)
{
	PifCollectSignalDevice* p_device;

	p_device = pifDList_AddLast(&s_collect_signal.device, sizeof(PifCollectSignalDevice));
	if (!p_device) return NULL;

	p_device->var_type = var_type;
	p_device->index = s_collect_signal.device_count;
	p_device->size = size;
	pif_Printf(p_device->p_reference, sizeof(p_device->p_reference), "%s_%x", p_reference, id);
	p_device->initial_value = initial_value;
	s_collect_signal.device_count++;
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_INFO, "CS:Add(ID:%u VT:%u SZ:%u) DC:%d", id, var_type, size, s_collect_signal.device_count);
#endif
	return p_device;
}

void pifCollectSignal_Start()
{
	char cBuffer[4];

	if (pifDList_Size(&s_collect_signal.device)) {
		pifDList_Clear(&s_collect_signal.device, NULL);
		s_collect_signal.device_count = 0;
	}
	for (int i = 0; i < 32; i++) {
		if (s_collect_signal.add_device[i]) (*s_collect_signal.add_device[i])();
	}
	s_collect_signal.timer = 0L;

	switch (s_collect_signal.method) {
	case CSM_LOG:
		pifLog_Disable();

		_printHeader();

		pifLog_Printf(LT_VCD, "#0\n");
		break;

	case CSM_BUFFER:
		pif_Printf(cBuffer, sizeof(cBuffer), "#0\n");
		pifRingBuffer_PutString(&s_collect_signal.buffer, cBuffer);
		break;
	}

	s_collect_signal.step = CSS_COLLECT;
}

void pifCollectSignal_Stop()
{
	uint32_t timer;
	char buffer[16];

	s_collect_signal.step = CSS_IDLE;

	switch (s_collect_signal.scale) {
	case CSS_1S:
		timer = pif_timer1sec;
		break;

	case CSS_1US:
		timer = (*pif_act_timer1us)();
		break;

	default:
		timer = pif_cumulative_timer1ms;
		break;
	}

	switch (s_collect_signal.method) {
	case CSM_LOG:
		pifLog_Printf(LT_VCD, "#%lu\n", timer);

		pifLog_Enable();
		break;

	case CSM_BUFFER:
		pif_Printf(buffer, sizeof(buffer), "#%lu\n", timer);
		pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
		break;
	}
}

void pifCollectSignal_AddSignal(void* p_dev, uint16_t state)
{
	PifCollectSignalDevice* p_device = (PifCollectSignalDevice*)p_dev;
	char buffer[64];
	uint32_t timer;

	if (s_collect_signal.step != CSS_COLLECT) return;

	switch (s_collect_signal.scale) {
	case CSS_1S:
		timer = pif_timer1sec;
		break;

	case CSS_1US:
		timer = (*pif_act_timer1us)();
		break;

	default:
		timer = pif_cumulative_timer1ms;
		break;
	}

	switch (s_collect_signal.method) {
	case CSM_LOG:
		if (s_collect_signal.timer != timer) {
			pifLog_Printf(LT_VCD, "#%lu\n", timer);
			s_collect_signal.timer = timer;
		}

		switch (p_device->var_type) {
		case CSVT_INTEGER:
		case CSVT_REG:
			pifLog_Printf(LT_VCD, "b%b %c\n", state, (int)('!' + p_device->index));
			break;

		case CSVT_REAL:
			pifLog_Printf(LT_VCD, "r%f %c\n", (double)state, (int)('!' + p_device->index));
			break;

		case CSVT_WIRE:
			pifLog_Printf(LT_VCD, "%u%c\n", state, (int)('!' + p_device->index));
			break;

		default:
			break;
		}
		break;

	case CSM_BUFFER:
		if (s_collect_signal.timer != timer) {
			pif_Printf(buffer, sizeof(buffer), "#%lu\n", timer);
			pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
			s_collect_signal.timer = timer;
		}

		switch (p_device->var_type) {
		case CSVT_INTEGER:
		case CSVT_REG:
			pif_Printf(buffer, sizeof(buffer), "b%b %c\n", state, (int)('!' + p_device->index));
			break;

		case CSVT_REAL:
			pif_Printf(buffer, sizeof(buffer), "r%f %c\n", (double)state, (int)('!' + p_device->index));
			break;

		case CSVT_WIRE:
			pif_Printf(buffer, sizeof(buffer), "%u%c\n", state, (int)('!' + p_device->index));
			break;

		default:
			buffer[0] = 0;
			break;
		}
		if (buffer[0]) pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
		break;
	}
}

void pifCollectSignal_PrintLog()
{
	if (s_collect_signal.method != CSM_BUFFER) return;

	_printHeader();

	pifLog_Disable();
	s_collect_signal.p_task->pause = FALSE;
}

#endif	// PIF_COLLECT_SIGNAL
