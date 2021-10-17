#ifdef __PIF_COLLECT_SIGNAL__

#include <string.h>

#include "pif_list.h"
#include "pifCollectSignal.h"
#include "pifLog.h"


typedef enum EnPifCollectSignalStep
{
	CSS_IDLE		= 0,
	CSS_COLLECT		= 1,
	CSS_SEND_LOG	= 2
} PifCollectSignalStep;


typedef struct StPifCollectSignalDevice
{
	PifId id;
	PifCollectSignalVarType var_type;
	uint8_t index;
	uint16_t size;
	uint16_t initial_value;
	char p_reference[10];
} PifCollectSignalDevice;

typedef struct StPifCollectSignal
{
	const char* p_module_name;
	PifCollectSignalMethod method;
	PifCollectSignalStep step;
	uint8_t device_count;
	uint32_t timer1ms;
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

	pifLog_Printf(LT_VCD, "$timescale 1ms $end\n");

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

/**
 * @fn pifCollectSignal_Init
 * @brief CollectSignal 구조체 초기화한다.
 * @param p_module_name
 */
void pifCollectSignal_Init(const char* p_module_name)
{
	s_collect_signal.p_module_name = p_module_name;
	s_collect_signal.method = CSM_REALTIME;
	s_collect_signal.step = CSS_IDLE;
	s_collect_signal.device_count = 0;
	pifDList_Init(&s_collect_signal.device);
	memset(s_collect_signal.add_device, 0, sizeof(s_collect_signal.add_device));
}

/**
 * @fn pifCollectSignal_InitHeap
 * @brief CollectSignal 구조체 초기화한다.
 * @param p_module_name
 * @param size
 * @return
 */
BOOL pifCollectSignal_InitHeap(const char *p_module_name, uint16_t size)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitHeap(&s_collect_signal.buffer, PIF_ID_AUTO, size)) return FALSE;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;
	return TRUE;
}

/**
 * @fn pifCollectSignal_InitStatic
 * @brief CollectSignal 구조체 초기화한다.
 * @param p_module_name
 * @param size
 * @param p_buffer
 * @return
 */
BOOL pifCollectSignal_InitStatic(const char *p_module_name, uint16_t size, uint8_t* p_buffer)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitStatic(&s_collect_signal.buffer, PIF_ID_AUTO, size, p_buffer)) return FALSE;
	pifRingBuffer_ChopsOffChar(&s_collect_signal.buffer, '#');
	s_collect_signal.method = CSM_BUFFER;
	return TRUE;
}

/**
 * @fn pifCollectSignal_Clear
 * @brief CollectSignal 구조체를 파기하다.
 */
void pifCollectSignal_Clear()
{
	pifRingBuffer_Clear(&s_collect_signal.buffer);
}

/**
 * @fn pifCollectSignal_ChangeFlag
 * @brief
 * @param p_flag
 * @param index
 * @param flag
 */
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

/**
 * @fn pifCollectSignal_ChangeMethod
 * @brief
 * @param method
 * @return
 */
BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method)
{
	if (s_collect_signal.step != CSS_IDLE) return FALSE;

	s_collect_signal.method = method;
	return TRUE;
}

/**
 * @fn pifCollectSignal_Attach
 * @brief
 * @param flag
 * @param add_device
 */
void pifCollectSignal_Attach(PifCollectSignalFlag flag, PifAddCollectSignalDevice add_device)
{
	s_collect_signal.add_device[flag] = add_device;
}

/**
 * @fn pifCollectSignal_AddDevice
 * @brief
 * @param id
 * @param var_type
 * @param size
 * @param p_reference
 * @param initial_value
 * @return
 */
void* pifCollectSignal_AddDevice(PifId id, PifCollectSignalVarType var_type, uint16_t size,
		const char* p_reference, uint16_t initial_value)
{
	PifCollectSignalDevice* p_device;

	p_device = pifDList_AddLast(&s_collect_signal.device, sizeof(PifCollectSignalDevice));
	if (!p_device) return NULL;

	p_device->id = id;
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

/**
 * @fn pifCollectSignal_Start
 * @brief
 */
void pifCollectSignal_Start()
{
	char cBuffer[4];

	for (int i = 0; i < 32; i++) {
		if (s_collect_signal.add_device[i]) (*s_collect_signal.add_device[i])();
	}
	s_collect_signal.timer1ms = 0L;

	switch (s_collect_signal.method) {
	case CSM_REALTIME:
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

/**
 * @fn pifCollectSignal_Stop
 * @brief
 */
void pifCollectSignal_Stop()
{
	char buffer[12];

	s_collect_signal.step = CSS_IDLE;

	switch (s_collect_signal.method) {
	case CSM_REALTIME:
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

/**
 * @fn pifCollectSignal_AddSignal
 * @brief
 * @param p_dev
 * @param state
 */
void pifCollectSignal_AddSignal(void* p_dev, uint16_t state)
{
	PifCollectSignalDevice* p_device = (PifCollectSignalDevice*)p_dev;
	char buffer[24];

	if (s_collect_signal.step != CSS_COLLECT) return;

	switch (s_collect_signal.method) {
	case CSM_REALTIME:
		if (s_collect_signal.timer1ms != pif_cumulative_timer1ms) {
			pifLog_Printf(LT_VCD, "#%u\n", pif_cumulative_timer1ms);
			s_collect_signal.timer1ms = pif_cumulative_timer1ms;
		}
		if (p_device->size == 1) {
			pifLog_Printf(LT_VCD, "%u%c\n", state, '!' + p_device->index);
		}
		else {
			pifLog_Printf(LT_VCD, "b%b %c\n", state, '!' + p_device->index);
		}
		break;

	case CSM_BUFFER:
		if (s_collect_signal.timer1ms != pif_cumulative_timer1ms) {
			pif_Printf(buffer, "#%u\n", pif_cumulative_timer1ms);
			pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
			s_collect_signal.timer1ms = pif_cumulative_timer1ms;
		}
		if (p_device->size == 1) {
			pif_Printf(buffer, "%u%c\n", state, '!' + p_device->index);
		}
		else {
			pif_Printf(buffer, "b%b %c\n", state, '!' + p_device->index);
		}
		pifRingBuffer_PutString(&s_collect_signal.buffer, buffer);
		break;
	}
}

/**
 * @fn pifCollectSignal_PrintLog
 * @brief
 */
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

/**
 * @fn pifCollectSignal_AttachTask
 * @brief Task를 추가한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifCollectSignal_AttachTask(PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _doTask, &s_collect_signal, start);
}

#endif	// __PIF_COLLECT_SIGNAL__
