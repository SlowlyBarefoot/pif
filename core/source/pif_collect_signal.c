#ifdef __PIF_COLLECT_SIGNAL__

#include <string.h>

#include "pif_collect_signal.h"
#include "pif_list.h"
#include "pif_log.h"


typedef enum EnPifCollectSignalStep
{
	CSS_IDLE		= 0,
	CSS_COLLECT		= 1,
	CSS_SEND_LOG	= 2
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

void pifCollectSignal_Init(const char* p_module_name)
{
	s_collect_signal.p_module_name = p_module_name;
	s_collect_signal.scale = CSS_1MS;
	s_collect_signal.method = CSM_LOG;
	s_collect_signal.step = CSS_IDLE;
	s_collect_signal.device_count = 0;
	pifDList_Init(&s_collect_signal.device);
	memset(s_collect_signal.add_device, 0, sizeof(s_collect_signal.add_device));
}

BOOL pifCollectSignal_InitHeap(const char *p_module_name, uint16_t size)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitHeap(&s_collect_signal.buffer, PIF_ID_AUTO, size)) return FALSE;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;
	return TRUE;
}

BOOL pifCollectSignal_InitStatic(const char *p_module_name, uint16_t size, uint8_t* p_buffer)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitStatic(&s_collect_signal.buffer, PIF_ID_AUTO, size, p_buffer)) return FALSE;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;
	return TRUE;
}

void pifCollectSignal_Clear()
{
	pifRingBuffer_Clear(&s_collect_signal.buffer);
}

BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale)
{
	if (s_collect_signal.step != CSS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (scale >= CSS_1NS) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (scale == CSS_1US && !pif_act_timer1us) {
		pif_error = E_CANNOT_USE;
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
	pif_Printf(p_device->p_reference, "%s_%x", p_reference, id);
	p_device->initial_value = initial_value;
	s_collect_signal.device_count++;
#ifndef __PIF_NO_LOG__
	pifLog_Printf(LT_INFO, "CS:Add(ID:%u VT:%u SZ:%u) DC:%d", id, var_type, size, p_device->size);
#endif
	return p_device;
}

void pifCollectSignal_Start()
{
	char cBuffer[4];

	for (int i = 0; i < 32; i++) {
		if (s_collect_signal.add_device[i]) (*s_collect_signal.add_device[i])();
	}
	s_collect_signal.timer = 0L;

	switch (s_collect_signal.method) {
	case CSM_LOG:
		_printHeader();

		pifLog_Printf(LT_VCD, "#0\n");
		break;

	case CSM_BUFFER:
		pif_Printf(cBuffer, "#0\n");
		pifRingBuffer_PutString(&s_collect_signal.buffer, cBuffer);
		break;
	}

	s_collect_signal.step = CSS_COLLECT;
}

void pifCollectSignal_Stop()
{
	char buffer[12];

	s_collect_signal.step = CSS_IDLE;

	switch (s_collect_signal.method) {
	case CSM_LOG:
		pifLog_Printf(LT_VCD, "#%u\n", pif_cumulative_timer1ms);
		break;

	case CSM_BUFFER:
		pif_Printf(buffer, "#%u\n", pif_cumulative_timer1ms);
		pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
		break;
	}

	pifDList_Clear(&s_collect_signal.device);
	s_collect_signal.device_count = 0;
}

void pifCollectSignal_AddSignal(void* p_dev, uint16_t state)
{
	PifCollectSignalDevice* p_device = (PifCollectSignalDevice*)p_dev;
	char buffer[64];
	uint32_t timer;

	if (s_collect_signal.step != CSS_COLLECT) return;

	switch (s_collect_signal.method) {
	case CSM_LOG:
		switch (s_collect_signal.scale) {
		case CSS_1S:
			if (s_collect_signal.timer != pif_timer1sec) {
				pifLog_Printf(LT_VCD, "#%lu\n", pif_timer1sec);
				s_collect_signal.timer = pif_timer1sec;
			}
			break;

		case CSS_1MS:
			if (s_collect_signal.timer != pif_cumulative_timer1ms) {
				pifLog_Printf(LT_VCD, "#%lu\n", pif_cumulative_timer1ms);
				s_collect_signal.timer = pif_cumulative_timer1ms;
			}
			break;

		case CSS_1US:
			timer = (*pif_act_timer1us)();
			if (s_collect_signal.timer != timer) {
				pifLog_Printf(LT_VCD, "#%lu\n", timer);
				s_collect_signal.timer = timer;
			}
			break;

		default:
			break;
		}

		switch (p_device->var_type) {
		case CSVT_INTEGER:
		case CSVT_REG:
			pifLog_Printf(LT_VCD, "b%b %c\n", state, '!' + p_device->index);
			break;

		case CSVT_REAL:
			pifLog_Printf(LT_VCD, "r%d %c\n", (double)state, '!' + p_device->index);
			break;

		case CSVT_WIRE:
			pifLog_Printf(LT_VCD, "%u%c\n", state, '!' + p_device->index);
			break;

		default:
			break;
		}
		break;

	case CSM_BUFFER:
		switch (s_collect_signal.scale) {
		case CSS_1S:
			if (s_collect_signal.timer != pif_timer1sec) {
				pif_Printf(buffer, "#%lu\n", pif_timer1sec);
				pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
				s_collect_signal.timer = pif_timer1sec;
			}
			break;

		case CSS_1MS:
			if (s_collect_signal.timer != pif_cumulative_timer1ms) {
				pif_Printf(buffer, "#%lu\n", pif_cumulative_timer1ms);
				pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
				s_collect_signal.timer = pif_cumulative_timer1ms;
			}
			break;

		case CSS_1US:
			timer = (*pif_act_timer1us)();
			if (s_collect_signal.timer != timer) {
				pif_Printf(buffer, "#%lu\n", timer);
				pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
				s_collect_signal.timer = timer;
			}
			break;

		default:
			break;
		}

		switch (p_device->var_type) {
		case CSVT_INTEGER:
		case CSVT_REG:
			pif_Printf(buffer, "b%b %c\n", state, '!' + p_device->index);
			break;

		case CSVT_REAL:
			pif_Printf(buffer, "r%d %c\n", (double)state, '!' + p_device->index);
			break;

		case CSVT_WIRE:
			pif_Printf(buffer, "%u%c\n", state, '!' + p_device->index);
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
	_printHeader();

	pifLog_Disable();
	s_collect_signal.step = CSS_SEND_LOG;
}

static uint16_t _doTask(PifTask* p_task)
{
	uint16_t size, length;
	static uint8_t tmp_buf[PIF_LOG_LINE_SIZE + 1];

	(void)p_task;

	if (s_collect_signal.step == CSS_SEND_LOG) {
		size = pifRingBuffer_GetFillSize(&s_collect_signal.buffer);
		length = PIF_LOG_LINE_SIZE;
		if (size > length) {
			pifRingBuffer_CopyToArray(tmp_buf, length, &s_collect_signal.buffer, 0);
			while (length) {
				if (tmp_buf[length - 1] == '\n') {
					tmp_buf[length] = 0;
					break;
				}
				else {
					length--;
				}
			}
			pifRingBuffer_Remove(&s_collect_signal.buffer, length);
			pifLog_Printf(LT_VCD, (char *)tmp_buf);
		}
		else {
			pifRingBuffer_CopyToArray(tmp_buf, size, &s_collect_signal.buffer, 0);
			tmp_buf[size] = 0;
			pifRingBuffer_Remove(&s_collect_signal.buffer, size);
			pifLog_Printf(LT_VCD, (char *)tmp_buf);
			s_collect_signal.step = CSS_IDLE;
			pifLog_Enable();
		}
	}
	return 0;
}

PifTask* pifCollectSignal_AttachTask(PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, &s_collect_signal, start);
}

#endif	// __PIF_COLLECT_SIGNAL__
