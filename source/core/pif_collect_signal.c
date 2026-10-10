// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_collect_signal.h"
#include "core/pif_log.h"

#include <string.h>

// Signal collection as a VCD file for debugging and monitoring.

#ifdef PIF_COLLECT_SIGNAL

#define PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS	20

// A record of CSM_BUFFER: time (4 bytes), channel index (2 bytes) and value (4 bytes).
#define CS_RECORD_SIZE		10
#define CS_INDEX_END		0xFFFF		// Record of the end of a capture

// Longest line but a $var line, whose name is cut to fit: "#<time>\n" followed by a 32 bit vector.
#define CS_LINE_SIZE		64


typedef enum EnPifCollectSignalStep
{
	CS_STEP_IDLE		= 0,
	CS_STEP_COLLECT		= 1,
	CS_STEP_PRINT		= 2
} PifCollectSignalStep;

typedef enum EnPifCollectSignalChannelState
{
	CS_CH_DETACHED		= 0,
	CS_CH_ATTACHED		= 1,
	CS_CH_CAPTURE		= 2		// Added before the last start, so it is in the header
} PifCollectSignalChannelState;

// Lines of the header in order. CS_HS_VAR and CS_HS_DUMPVALUE take one line per channel.
typedef enum EnPifCollectSignalHeaderStage
{
	CS_HS_DATE				= 0,
	CS_HS_VERSION,
	CS_HS_DROP,
	CS_HS_TIMESCALE,
	CS_HS_SCOPE,
	CS_HS_VAR,
	CS_HS_UPSCOPE,
	CS_HS_ENDDEF,
	CS_HS_TIME0,
	CS_HS_DUMPVARS,
	CS_HS_DUMPVALUE,
	CS_HS_DUMPEND,
	CS_HS_DONE
} PifCollectSignalHeaderStage;


typedef struct StPifCollectSignal
{
	const char *p_module_name;
	PifCollectSignalScale scale;
	PifCollectSignalMethod method;
	PifCollectSignalOverflow overflow;
	PifCollectSignalStep step;
	PifTask *p_task;
	uint32_t transfer_period_1us;
	PifRingBuffer buffer;
	PifCollectSignalChannel *p_head;
	PifDateTime start_datetime;
	uint32_t start_time;
	uint32_t base_time;				// Time the values of $dumpvars hold from
	uint32_t last_time;				// Time of the last "#" line printed
	uint32_t drop_count;

	// Position of the transfer task
	uint8_t stage;					// PifCollectSignalHeaderStage
	BOOL retry;
	PifCollectSignalChannel *p_cursor;
} PifCollectSignal;


static PifCollectSignal s_collect_signal;


static uint32_t _getTime()
{
	switch (s_collect_signal.scale) {
	case CSS_1S:
		return pif_timer1sec;

	case CSS_1US:
		return (*pif_act_timer1us)();

	default:
		return pif_cumulative_timer1ms;
	}
}

static uint32_t _maskValue(const PifCollectSignalChannel *p_channel, uint32_t value)
{
	if (p_channel->_var_type != CSVT_REAL && p_channel->_width && p_channel->_width < 32) {
		value &= (1UL << p_channel->_width) - 1;
	}
	return value;
}

// VCD identifier of a channel: base 94 digits of the printable characters '!' to '~'.
static int _formatCode(char *p_buffer, uint16_t index)
{
	int len = 0;

	do {
		p_buffer[len++] = '!' + index % 94;
		index /= 94;
	} while (index);
	p_buffer[len] = 0;
	return len;
}

static int _formatTime(char *p_buffer, uint32_t time, uint32_t *p_last_time)
{
	int len = 0;

	if (time == *p_last_time) return 0;
	*p_last_time = time;

	p_buffer[len++] = '#';
	len += pif_DecToString(p_buffer + len, time, 0);
	p_buffer[len++] = '\n';
	p_buffer[len] = 0;
	return len;
}

static int _formatValue(char *p_buffer, const PifCollectSignalChannel *p_channel, uint32_t value)
{
	int len = 0;
	float real;

	if (p_channel->_var_type == CSVT_REAL) {
		memcpy(&real, &value, sizeof(real));
		p_buffer[len++] = 'r';
		len += pif_FloatToString(p_buffer + len, real, 0);
		p_buffer[len++] = ' ';
	}
	else if (p_channel->_width == 1) {
		p_buffer[len++] = '0' + (value & 1);
	}
	else {
		p_buffer[len++] = 'b';
		len += pif_BinToString(p_buffer + len, value, 0);
		p_buffer[len++] = ' ';
	}
	len += _formatCode(p_buffer + len, p_channel->__index);
	p_buffer[len++] = '\n';
	p_buffer[len] = 0;
	return len;
}

static PifCollectSignalChannel *_findCapture(uint16_t index)
{
	PifCollectSignalChannel *p_channel = s_collect_signal.p_head;

	while (p_channel) {
		if (p_channel->__state == CS_CH_CAPTURE && p_channel->__index == index) break;
		p_channel = p_channel->__p_next;
	}
	return p_channel;
}

static PifCollectSignalChannel *_nextCapture(PifCollectSignalChannel *p_channel)
{
	while (p_channel && p_channel->__state != CS_CH_CAPTURE) p_channel = p_channel->__p_next;
	return p_channel;
}

// Formats the header line at *p_stage and *pp_cursor and moves them past it. Returns the length
// of the line, which is 0 for a stage that has nothing to print.
static int _formatHeader(char *p_buffer, size_t size, uint8_t *p_stage, PifCollectSignalChannel **pp_cursor)
{
	static const char *kVarType[] = { "integer", "real", "reg", "wire" };
	static const char *kTimeScale[] = { "1s", "1ms", "1us" };
	PifCollectSignal *p_cs = &s_collect_signal;
	PifCollectSignalChannel *p_channel;
	char code[4];
	char tail[16];

	p_buffer[0] = 0;
	switch (*p_stage) {
	case CS_HS_DATE:
		pif_Printf(p_buffer, size, "$date %s %u, %u %2u:%2u:%2u $end\n",
				kPifMonth3[(p_cs->start_datetime.month + 11) % 12], p_cs->start_datetime.day,
				2000 + p_cs->start_datetime.year, p_cs->start_datetime.hour,
				p_cs->start_datetime.minute, p_cs->start_datetime.second);
		break;

	case CS_HS_VERSION:
		pif_Printf(p_buffer, size, "$version PIF %u.%u.%u $end\n", PIF_VERSION_MAJOR, PIF_VERSION_MINOR,
				PIF_VERSION_PATCH);
		break;

	case CS_HS_DROP:
		if (p_cs->drop_count) {
			pif_Printf(p_buffer, size, "$comment %lu %s changes dropped, buffer full $end\n",
					(unsigned long)p_cs->drop_count, p_cs->overflow == CSO_KEEP_FIRST ? "later" : "earlier");
		}
		break;

	case CS_HS_TIMESCALE:
		pif_Printf(p_buffer, size, "$timescale %s $end\n", kTimeScale[p_cs->scale]);
		break;

	case CS_HS_SCOPE:
		pif_Printf(p_buffer, size, "$scope module %s $end\n", p_cs->p_module_name);
		break;

	case CS_HS_VAR:
	case CS_HS_DUMPVALUE:
		p_channel = _nextCapture(*pp_cursor);
		if (!p_channel) break;
		*pp_cursor = p_channel->__p_next;

		if (*p_stage == CS_HS_DUMPVALUE) return _formatValue(p_buffer, p_channel, p_channel->__start_value);

		// The tail goes first, so that a name too long for the line is what gets cut.
		if (p_channel->_var_type != CSVT_REAL && p_channel->_width > 1) {
			pif_Printf(tail, sizeof(tail), "[%u:0] $end\n", p_channel->_width - 1);
		}
		else {
			strcpy(tail, " $end\n");
		}
		_formatCode(code, p_channel->__index);
		if (p_channel->_id == PIF_ID_AUTO) {
			pif_Printf(p_buffer, size - strlen(tail), "$var %s %u %s %s", kVarType[p_channel->_var_type],
					p_channel->_width, code, p_channel->_name);
		}
		else {
			pif_Printf(p_buffer, size - strlen(tail), "$var %s %u %s %s_%x", kVarType[p_channel->_var_type],
					p_channel->_width, code, p_channel->_name, p_channel->_id);
		}
		strcat(p_buffer, tail);
		return strlen(p_buffer);

	case CS_HS_UPSCOPE:
		pif_Printf(p_buffer, size, "$upscope $end\n");
		break;

	case CS_HS_ENDDEF:
		pif_Printf(p_buffer, size, "$enddefinitions $end\n");
		break;

	case CS_HS_TIME0:
		pif_Printf(p_buffer, size, "#%lu\n", (unsigned long)p_cs->base_time);
		break;

	case CS_HS_DUMPVARS:
		pif_Printf(p_buffer, size, "$dumpvars\n");
		break;

	case CS_HS_DUMPEND:
		pif_Printf(p_buffer, size, "$end\n");
		break;

	default:
		return 0;
	}

	(*p_stage)++;
	*pp_cursor = p_cs->p_head;
	return strlen(p_buffer);
}

static int _formatRecord(char *p_buffer, const uint8_t *p_record, uint32_t *p_last_time)
{
	PifCollectSignalChannel *p_channel;
	uint32_t time, value;
	uint16_t index;
	int len;

	memcpy(&time, p_record, 4);
	memcpy(&index, p_record + 4, 2);
	memcpy(&value, p_record + 6, 4);

	len = _formatTime(p_buffer, time, p_last_time);
	if (index != CS_INDEX_END) {
		// A channel removed since the capture has no $var line, so its changes are left out.
		p_channel = _findCapture(index);
		if (p_channel) len += _formatValue(p_buffer + len, p_channel, value);
	}
	p_buffer[len] = 0;
	return len;
}

// Drops the oldest record. Its value becomes the initial value of its channel and its time the
// time of $dumpvars, so the dump stays true from there on and only shows less of the past.
static void _dropOldest()
{
	PifCollectSignalChannel *p_channel;
	uint8_t record[CS_RECORD_SIZE];
	uint32_t value;
	uint16_t index;

	pifRingBuffer_CopyToArray(record, CS_RECORD_SIZE, &s_collect_signal.buffer, 0);
	pifRingBuffer_Remove(&s_collect_signal.buffer, CS_RECORD_SIZE);

	memcpy(&s_collect_signal.base_time, record, 4);
	memcpy(&index, record + 4, 2);
	memcpy(&value, record + 6, 4);
	p_channel = _findCapture(index);
	if (p_channel) p_channel->__start_value = value;
	s_collect_signal.drop_count++;
}

static void _putRecord(uint16_t index, uint32_t value)
{
	PifRingBuffer *p_buffer = &s_collect_signal.buffer;
	uint8_t record[CS_RECORD_SIZE];
	uint32_t time = _getTime() - s_collect_signal.start_time;

	if (s_collect_signal.overflow == CSO_KEEP_LAST) {
		while (pifRingBuffer_GetRemainSize(p_buffer) < CS_RECORD_SIZE) _dropOldest();
	}
	// The room of one record is kept for the end of the capture, so the dump always ends at the
	// time of pifCollectSignal_Stop().
	else if (pifRingBuffer_GetRemainSize(p_buffer) < (index == CS_INDEX_END ? 1 : 2) * CS_RECORD_SIZE) {
		s_collect_signal.drop_count++;
		return;
	}

	memcpy(record, &time, 4);
	memcpy(record + 4, &index, 2);
	memcpy(record + 6, &value, 4);
	pifRingBuffer_PutData(p_buffer, record, CS_RECORD_SIZE);
}

static uint32_t _doTask(PifTask *p_task)
{
	PifCollectSignal *p_cs = &s_collect_signal;
	char text[PIF_COLLECT_SIGNAL_TEXT_SIZE + CS_LINE_SIZE];
	uint8_t record[CS_RECORD_SIZE];
	uint8_t stage = p_cs->stage;
	PifCollectSignalChannel *p_cursor = p_cs->p_cursor;
	uint32_t last_time = p_cs->last_time;
	uint32_t drop_count;
	uint16_t fill = pifRingBuffer_GetFillSize(&p_cs->buffer);
	uint16_t used = 0;
	size_t len = 0;

	if (p_cs->step != CS_STEP_PRINT) {
		p_task->pause = TRUE;
		return 0;
	}

	// After a pass that did not fit in the log, one line at a time, so that a small transmit
	// buffer still gets through.
	text[0] = 0;
	while (!len || (!p_cs->retry && len < PIF_COLLECT_SIGNAL_TEXT_SIZE)) {
		if (stage < CS_HS_DONE) {
			len += _formatHeader(text + len, sizeof(text) - len, &stage, &p_cursor);
		}
		else if (used < fill) {
			pifRingBuffer_CopyToArray(record, CS_RECORD_SIZE, &p_cs->buffer, used);
			used += CS_RECORD_SIZE;
			len += _formatRecord(text + len, record, &last_time);
		}
		else {
			break;
		}
	}

	if (len) {
		// pifLog drops a whole line it has no room for, so the same text is tried again on the
		// next pass and nothing of the dump is lost.
		drop_count = pifLog_DropCount();
		pifLog_Print(LT_VCD, text);
		if (pifLog_DropCount() != drop_count) {
			p_cs->retry = TRUE;
			return 0;
		}
	}

	p_cs->retry = FALSE;
	p_cs->stage = stage;
	p_cs->p_cursor = p_cursor;
	p_cs->last_time = last_time;
	if (used) pifRingBuffer_Remove(&p_cs->buffer, used);

	if (stage >= CS_HS_DONE && pifRingBuffer_IsEmpty(&p_cs->buffer)) {
		p_task->pause = TRUE;
		p_cs->step = CS_STEP_IDLE;
		pifLog_Enable();
	}
	return 0;
}

static void _detachAll()
{
	PifCollectSignalChannel *p_channel = s_collect_signal.p_head;
	PifCollectSignalChannel *p_next;

	while (p_channel) {
		p_next = p_channel->__p_next;
		p_channel->__state = CS_CH_DETACHED;
		p_channel->__p_next = NULL;
		p_channel = p_next;
	}
	s_collect_signal.p_head = NULL;
	s_collect_signal.p_cursor = NULL;
}

static BOOL _initBuffer(const char *p_task_name)
{
	// Room for one change and the end of a capture at least.
	if (pifRingBuffer_GetRemainSize(&s_collect_signal.buffer) < 2 * CS_RECORD_SIZE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	pifRingBuffer_SetName(&s_collect_signal.buffer, "CollectSignal");
	s_collect_signal.method = CSM_BUFFER;

	s_collect_signal.transfer_period_1us = PIF_COLLECT_SIGNAL_TRANSFER_PERIOD_1MS * 1000UL;
	s_collect_signal.p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, s_collect_signal.transfer_period_1us,
			_doTask, &s_collect_signal, FALSE);
	if (!s_collect_signal.p_task) return FALSE;
	s_collect_signal.p_task->name = p_task_name;
	return TRUE;
}

void pifCollectSignal_Init(const char *p_module_name)
{
	_detachAll();
	memset(&s_collect_signal, 0, sizeof(PifCollectSignal));

	s_collect_signal.p_module_name = p_module_name;
	s_collect_signal.scale = CSS_1MS;
	s_collect_signal.method = CSM_LOG;
}

BOOL pifCollectSignal_InitHeap(const char *p_module_name, uint16_t size)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitHeap(&s_collect_signal.buffer, PIF_ID_AUTO, size)) goto fail;
	if (!_initBuffer("CollectSignalHeap")) goto fail;
	return TRUE;

fail:
	pifCollectSignal_Clear();
	return FALSE;
}

BOOL pifCollectSignal_InitStatic(const char *p_module_name, uint16_t size, uint8_t *p_buffer)
{
	pifCollectSignal_Init(p_module_name);

	if (!pifRingBuffer_InitStatic(&s_collect_signal.buffer, PIF_ID_AUTO, size, p_buffer)) goto fail;
	if (!_initBuffer("CollectSignalStatic")) goto fail;
	return TRUE;

fail:
	pifCollectSignal_Clear();
	return FALSE;
}

void pifCollectSignal_Clear()
{
	// Only these two hold the other log output back.
	if (s_collect_signal.step == CS_STEP_PRINT ||
			(s_collect_signal.step == CS_STEP_COLLECT && s_collect_signal.method == CSM_LOG)) {
		pifLog_Enable();
	}
	s_collect_signal.step = CS_STEP_IDLE;

	if (s_collect_signal.p_task) {
		pifTaskManager_Remove(s_collect_signal.p_task);
		s_collect_signal.p_task = NULL;
	}
	pifRingBuffer_Clear(&s_collect_signal.buffer);
	s_collect_signal.method = CSM_LOG;
	_detachAll();
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

	s_collect_signal.transfer_period_1us = period1ms * 1000UL;
    if (s_collect_signal.p_task) pifTask_ChangePeriod(s_collect_signal.p_task, s_collect_signal.transfer_period_1us);
	return TRUE;
}

BOOL pifCollectSignal_ChangeScale(PifCollectSignalScale scale)
{
	if (s_collect_signal.step != CS_STEP_IDLE) {
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

BOOL pifCollectSignal_ChangeMethod(PifCollectSignalMethod method)
{
	if (s_collect_signal.step != CS_STEP_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (method > CSM_BUFFER) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (method == CSM_BUFFER && !s_collect_signal.p_task) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	s_collect_signal.method = method;
	return TRUE;
}

BOOL pifCollectSignal_ChangeOverflow(PifCollectSignalOverflow overflow)
{
	if (s_collect_signal.step != CS_STEP_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	if (overflow > CSO_KEEP_LAST) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	s_collect_signal.overflow = overflow;
	return TRUE;
}

BOOL pifCollectSignal_AddChannel(PifCollectSignalChannel *p_channel, const char *p_name, PifId id,
		PifCollectSignalVarType var_type, uint8_t width, uint32_t initial_value)
{
	PifCollectSignalChannel **pp_link;

	if (!p_channel || !p_name || strlen(p_name) >= PIF_COLLECT_SIGNAL_NAME_SIZE || var_type > CSVT_WIRE ||
			(var_type != CSVT_REAL && (!width || width > 32))) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_channel->__state != CS_CH_DETACHED) return TRUE;

	strcpy(p_channel->_name, p_name);
	p_channel->_id = id;
	p_channel->_var_type = var_type;
	p_channel->_width = var_type == CSVT_REAL ? 64 : width;
	p_channel->_value = _maskValue(p_channel, initial_value);
	p_channel->__state = CS_CH_ATTACHED;
	p_channel->__p_next = NULL;

	// At the end, so the dump lists the channels in the order they were added.
	pp_link = &s_collect_signal.p_head;
	while (*pp_link) pp_link = &(*pp_link)->__p_next;
	*pp_link = p_channel;
	return TRUE;
}

void pifCollectSignal_RemoveChannel(PifCollectSignalChannel *p_channel)
{
	PifCollectSignalChannel **pp_link;

	if (!p_channel || p_channel->__state == CS_CH_DETACHED) return;

	pp_link = &s_collect_signal.p_head;
	while (*pp_link && *pp_link != p_channel) pp_link = &(*pp_link)->__p_next;
	if (*pp_link) *pp_link = p_channel->__p_next;

	// The transfer task may be in the middle of the header.
	if (s_collect_signal.p_cursor == p_channel) s_collect_signal.p_cursor = p_channel->__p_next;

	p_channel->__state = CS_CH_DETACHED;
	p_channel->__p_next = NULL;
}

BOOL pifCollectSignal_Start()
{
	PifCollectSignal *p_cs = &s_collect_signal;
	PifCollectSignalChannel *p_channel;
	uint16_t index = 0;
	char text[PIF_COLLECT_SIGNAL_TEXT_SIZE + CS_LINE_SIZE];
	uint8_t stage;

	if (p_cs->step != CS_STEP_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	for (p_channel = p_cs->p_head; p_channel; p_channel = p_channel->__p_next) {
		p_channel->__state = CS_CH_CAPTURE;
		p_channel->__index = index++;
		p_channel->__start_value = p_channel->_value;
	}

	p_cs->start_datetime = *(PifDateTime *)&pif_datetime;
	p_cs->drop_count = 0;
	p_cs->base_time = 0;
	p_cs->last_time = 0;

	if (p_cs->method == CSM_LOG) {
		pifLog_Disable();

		stage = CS_HS_DATE;
		p_channel = p_cs->p_head;
		while (stage < CS_HS_DONE) {
			if (_formatHeader(text, sizeof(text), &stage, &p_channel)) pifLog_Print(LT_VCD, text);
		}
	}
	else {
		pifRingBuffer_Empty(&p_cs->buffer);
	}

	p_cs->start_time = _getTime();
	p_cs->step = CS_STEP_COLLECT;
	return TRUE;
}

void pifCollectSignal_Stop()
{
	PifCollectSignal *p_cs = &s_collect_signal;
	char text[CS_LINE_SIZE];

	if (p_cs->step != CS_STEP_COLLECT) return;

	if (p_cs->method == CSM_LOG) {
		if (_formatTime(text, _getTime() - p_cs->start_time, &p_cs->last_time)) pifLog_Print(LT_VCD, text);
		pifLog_Enable();
	}
	else {
		_putRecord(CS_INDEX_END, 0);
	}
	p_cs->step = CS_STEP_IDLE;
}

BOOL pifCollectSignal_IsCollecting()
{
	return s_collect_signal.step == CS_STEP_COLLECT;
}

void pifCollectSignal_Put(PifCollectSignalChannel *p_channel, uint32_t value)
{
	PifCollectSignal *p_cs = &s_collect_signal;
	char text[CS_LINE_SIZE];
	int len;

	value = _maskValue(p_channel, value);
	if (value == p_channel->_value) return;
	p_channel->_value = value;

	if (p_cs->step != CS_STEP_COLLECT || p_channel->__state != CS_CH_CAPTURE) return;

	if (p_cs->method == CSM_LOG) {
		len = _formatTime(text, _getTime() - p_cs->start_time, &p_cs->last_time);
		_formatValue(text + len, p_channel, value);
		pifLog_Print(LT_VCD, text);
	}
	else {
		_putRecord(p_channel->__index, value);
	}
}

void pifCollectSignal_PutReal(PifCollectSignalChannel *p_channel, float value)
{
	uint32_t raw;

	memcpy(&raw, &value, sizeof(raw));
	pifCollectSignal_Put(p_channel, raw);
}

BOOL pifCollectSignal_PrintLog()
{
	PifCollectSignal *p_cs = &s_collect_signal;

	if (p_cs->method != CSM_BUFFER) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}

	if (p_cs->step != CS_STEP_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_cs->stage = CS_HS_DATE;
	p_cs->p_cursor = p_cs->p_head;
	p_cs->last_time = p_cs->base_time;
	p_cs->retry = FALSE;
	p_cs->step = CS_STEP_PRINT;

	pifLog_Disable();
	p_cs->p_task->pause = FALSE;
	return TRUE;
}

BOOL pifCollectSignal_IsPrinting()
{
	return s_collect_signal.step == CS_STEP_PRINT;
}

uint32_t pifCollectSignal_GetDropCount()
{
	return s_collect_signal.drop_count;
}

#endif	// PIF_COLLECT_SIGNAL
