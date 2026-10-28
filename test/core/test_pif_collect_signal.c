// SPDX-License-Identifier: BSD-3-Clause
// pif_collect_signal.c is included so the tests can run its transfer task directly.
#include "../../source/core/pif_collect_signal.c"
#include "core/pif_sequence.h"

#include <stdio.h>
#include <string.h>


#define LOG_SIZE		8192

// Works regardless of NDEBUG, and reports where the failure happened.
#define CHECK(COND) \
    do { \
        if (!(COND)) { \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #COND); \
            s_failed = TRUE; \
            return; \
        } \
    } while (0)

static BOOL s_failed;

// pif_log.c is not linked: these keep the VCD output, and can refuse lines the way a full
// transmit buffer does.
static char s_log[LOG_SIZE];
static size_t s_log_len;
static BOOL s_log_enable = TRUE;
static uint32_t s_log_drop;
static int s_log_refuse;			// Prints still to refuse

PifLogFlag pif_log_flag;

void pifLog_Print(PifLogType type, const char *p_string)
{
    size_t len = strlen(p_string);

    if (type != LT_VCD) return;
    if (s_log_refuse) {
        s_log_refuse--;
        s_log_drop++;
        return;
    }
    if (s_log_len + len < LOG_SIZE) {
        memcpy(s_log + s_log_len, p_string, len + 1);
        s_log_len += len;
    }
}

void pifLog_Printf(PifLogType type, const char *p_format, ...)
{
    (void)type;
    (void)p_format;
}

void pifLog_Enable() { s_log_enable = TRUE; }
void pifLog_Disable() { s_log_enable = FALSE; }
uint32_t pifLog_DropCount() { return s_log_drop; }


static PifCollectSignalChannel s_clk, s_bus, s_temp, s_late;
static uint8_t s_buffer[1024];

static uint32_t _timer1us(void)
{
    return pif_cumulative_timer1ms * 1000;
}

static void _resetLog()
{
    s_log_len = 0;
    s_log[0] = 0;
    s_log_drop = 0;
    s_log_refuse = 0;
}

// The $date line depends on the clock, so a dump is compared from the line after it.
static const char *_afterDate(const char *p_dump)
{
    const char *p = strstr(p_dump, "$end\n");
    return p ? p + 5 : "";
}

static BOOL _addChannels()
{
    memset(&s_clk, 0, sizeof(s_clk));
    memset(&s_bus, 0, sizeof(s_bus));
    memset(&s_temp, 0, sizeof(s_temp));
    return pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 1, 0) &&
            pifCollectSignal_AddChannel(&s_bus, "bus", 0x12, CSVT_REG, 4, 5) &&
            pifCollectSignal_AddChannel(&s_temp, "temp", PIF_ID_AUTO, CSVT_REAL, 0, 0);
}

// The same changes for every method: times are relative to the start at 100 ms.
static void _runCapture()
{
    pif_cumulative_timer1ms = 100;
    pifCollectSignal_Put(&s_clk, 1);			// Before the start: the initial value
    pifCollectSignal_Start();
    pifCollectSignal_Put(&s_clk, 0);
    pif_cumulative_timer1ms = 103;
    pifCollectSignal_Put(&s_bus, 0x1A);		// Masked to 4 bits
    pifCollectSignal_Put(&s_bus, 0x0A);		// No change
    pifCollectSignal_PutReal(&s_temp, 1.5f);
    pif_cumulative_timer1ms = 107;
    pifCollectSignal_Put(&s_clk, 1);
    pif_cumulative_timer1ms = 110;
    pifCollectSignal_Stop();
}

static const char *kExpect =
        "$version PIF 0.1.0 $end\n"
        "$timescale 1ms $end\n"
        "$scope module top $end\n"
        "$var wire 1 ! clk $end\n"
        "$var reg 4 \" bus_12[3:0] $end\n"
        "$var real 64 # temp $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n"
        "$dumpvars\n"
        "1!\n"
        "b101 \"\n"
        "r0.000000 #\n"
        "$end\n"
        "0!\n"
        "#3\n"
        "b1010 \"\n"
        "r1.500000 #\n"
        "#7\n"
        "1!\n"
        "#10\n";

static int _printAll()
{
    int passes = 0;

    while (pifCollectSignal_IsPrinting() && passes < 1000) {
        _doTask(s_collect_signal.p_task);
        passes++;
    }
    return passes;
}

static void testLog()
{
    _resetLog();
    pifCollectSignal_Init("top", NULL, 0);
    CHECK(_addChannels());

    _runCapture();
    CHECK(s_log_enable);
    CHECK(strncmp(s_log, "$date ", 6) == 0);
    CHECK(strcmp(_afterDate(s_log), kExpect) == 0);
    pifCollectSignal_Clear();
}

static void testBuffer()
{
    int passes;

    _resetLog();
    CHECK(pifCollectSignal_InitStatic("top", NULL, 0, sizeof(s_buffer), s_buffer));
    CHECK(_addChannels());

    _runCapture();
    CHECK(s_log_len == 0);
    CHECK(pifCollectSignal_GetDropCount() == 0);

    CHECK(pifCollectSignal_PrintLog());
    CHECK(!s_log_enable);
    CHECK(!pifCollectSignal_Start());			// Not while printing
    passes = _printAll();
    CHECK(passes > 1);							// Spread over the task passes
    CHECK(!pifCollectSignal_IsPrinting());
    CHECK(s_log_enable);
    CHECK(s_collect_signal.p_task->_pause);
    CHECK(strcmp(_afterDate(s_log), kExpect) == 0);
    pifCollectSignal_Clear();
}

// Text the log refuses is printed again on the next pass, so the dump stays whole.
static void testBufferRetry()
{
    _resetLog();
    CHECK(pifCollectSignal_InitStatic("top", NULL, 0, sizeof(s_buffer), s_buffer));
    CHECK(_addChannels());

    _runCapture();
    CHECK(pifCollectSignal_PrintLog());
    _doTask(s_collect_signal.p_task);
    s_log_refuse = 3;
    _printAll();
    CHECK(s_log_drop == 3);
    CHECK(strcmp(_afterDate(s_log), kExpect) == 0);
    pifCollectSignal_Clear();
}

// Ten changes of clk, at 1 to 10 ms, into a buffer of 4 records, and the end at 20 ms.
static BOOL _captureOverflow(PifCollectSignalOverflow overflow)
{
    int i;

    _resetLog();
    if (!pifCollectSignal_InitStatic("top", NULL, 0, 4 * CS_RECORD_SIZE + 1, s_buffer)) return FALSE;
    if (!pifCollectSignal_ChangeOverflow(overflow)) return FALSE;
    memset(&s_clk, 0, sizeof(s_clk));
    if (!pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 1, 0)) return FALSE;

    pif_cumulative_timer1ms = 0;
    if (!pifCollectSignal_Start()) return FALSE;
    if (pifCollectSignal_ChangeOverflow(CSO_KEEP_FIRST)) return FALSE;	// Not during a capture
    for (i = 1; i <= 10; i++) {
        pif_cumulative_timer1ms = i;
        pifCollectSignal_Put(&s_clk, i & 1);
    }
    pif_cumulative_timer1ms = 20;
    pifCollectSignal_Stop();

    if (!pifCollectSignal_PrintLog()) return FALSE;
    _printAll();
    return TRUE;
}

// A full buffer keeps the beginning and the end time, and the dump says what was dropped.
static void testOverflowKeepFirst()
{
    CHECK(_captureOverflow(CSO_KEEP_FIRST));
    // 3 changes and the end of the capture fit.
    CHECK(pifCollectSignal_GetDropCount() == 7);
    CHECK(strstr(s_log, "$comment 7 later changes dropped, buffer full $end\n") != NULL);
    CHECK(strstr(s_log, "#0\n$dumpvars\n0!\n$end\n#1\n1!\n#2\n0!\n#3\n1!\n#20\n") != NULL);
    pifCollectSignal_Clear();
}

// The latest changes push the oldest out, and the dump begins at the last one pushed out, with
// the value it set.
static void testOverflowKeepLast()
{
    CHECK(_captureOverflow(CSO_KEEP_LAST));
    // The changes of 8 to 10 ms and the end of the capture are left. The end pushed out 7 ms.
    CHECK(pifCollectSignal_GetDropCount() == 7);
    CHECK(strstr(s_log, "$comment 7 earlier changes dropped, buffer full $end\n") != NULL);
    CHECK(strstr(s_log, "#7\n$dumpvars\n1!\n$end\n#8\n0!\n#9\n1!\n#10\n0!\n#20\n") != NULL);
    CHECK(strstr(s_log, "#0\n") == NULL);
    pifCollectSignal_Clear();

    // The next capture begins at 0 again.
    CHECK(pifCollectSignal_InitStatic("top", NULL, 0, sizeof(s_buffer), s_buffer));
    CHECK(pifCollectSignal_ChangeOverflow(CSO_KEEP_LAST));
    CHECK(pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 1, 0));
    _resetLog();
    CHECK(pifCollectSignal_Start());
    pifCollectSignal_Stop();
    CHECK(pifCollectSignal_PrintLog());
    _printAll();
    CHECK(strstr(s_log, "#0\n$dumpvars\n") != NULL);
    CHECK(pifCollectSignal_GetDropCount() == 0);
    pifCollectSignal_Clear();
}

static void testBufferSize()
{
    CHECK(!pifCollectSignal_InitStatic("top", NULL, 0, 2 * CS_RECORD_SIZE, s_buffer));
    CHECK(pifCollectSignal_InitStatic("top", NULL, 0, 2 * CS_RECORD_SIZE + 1, s_buffer));
    CHECK(!pifCollectSignal_ChangeOverflow((PifCollectSignalOverflow)2));
    pifCollectSignal_Clear();
}

// A channel removed before printing is left out, and one added during a capture waits for the
// next one.
static void testChannelChanges()
{
    _resetLog();
    CHECK(pifCollectSignal_InitStatic("top", NULL, 0, sizeof(s_buffer), s_buffer));
    CHECK(_addChannels());

    pif_cumulative_timer1ms = 0;
    CHECK(pifCollectSignal_Start());
    memset(&s_late, 0, sizeof(s_late));
    CHECK(pifCollectSignal_AddChannel(&s_late, "late", PIF_ID_AUTO, CSVT_INTEGER, 8, 0));
    pif_cumulative_timer1ms = 1;
    pifCollectSignal_Put(&s_clk, 1);
    pifCollectSignal_Put(&s_bus, 3);
    pifCollectSignal_Put(&s_late, 9);
    pif_cumulative_timer1ms = 2;
    pifCollectSignal_Stop();
    pifCollectSignal_RemoveChannel(&s_bus);
    pifCollectSignal_RemoveChannel(&s_bus);	// Ignored

    CHECK(pifCollectSignal_PrintLog());
    _printAll();
    CHECK(strstr(s_log, "bus") == NULL);
    CHECK(strstr(s_log, "late") == NULL);
    CHECK(strstr(s_log, "$end\n#1\n1!\n#2\n") != NULL);

    // The next capture has the new channel, and the codes follow the channels left.
    _resetLog();
    CHECK(pifCollectSignal_Start());
    pifCollectSignal_Stop();
    CHECK(pifCollectSignal_PrintLog());
    _printAll();
    CHECK(strstr(s_log, "$var integer 8 # late[7:0] $end\n") != NULL);
    CHECK(strstr(s_log, "b1001 #\n") != NULL);
    pifCollectSignal_Clear();
}

static void testStateChecks()
{
    pifCollectSignal_Init("top", NULL, 0);
    memset(&s_clk, 0, sizeof(s_clk));
    CHECK(!pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 0, 0));
    CHECK(!pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 33, 0));
    CHECK(!pifCollectSignal_ChangeMethod(CSM_BUFFER));		// No buffer
    CHECK(!pifCollectSignal_PrintLog());

    // A name that leaves no room for the NUL is refused rather than cut.
    {
        char too_long[PIF_COLLECT_SIGNAL_NAME_SIZE + 1];

        memset(too_long, 'x', PIF_COLLECT_SIGNAL_NAME_SIZE);
        too_long[PIF_COLLECT_SIGNAL_NAME_SIZE] = 0;
        pif_error = E_SUCCESS;
        CHECK(!pifCollectSignal_AddChannel(&s_clk, too_long, PIF_ID_AUTO, CSVT_WIRE, 1, 0) && pif_error == E_INVALID_PARAM);
        too_long[PIF_COLLECT_SIGNAL_NAME_SIZE - 1] = 0;
        CHECK(pifCollectSignal_AddChannel(&s_clk, too_long, PIF_ID_AUTO, CSVT_WIRE, 1, 0));
        pifCollectSignal_RemoveChannel(&s_clk);
    }

    // The name is copied, so the buffer it came from may change or go away.
    {
        char name[] = "clk";

        CHECK(pifCollectSignal_AddChannel(&s_clk, name, PIF_ID_AUTO, CSVT_WIRE, 1, 0));
        name[0] = 'x';
    }
    CHECK(pifCollectSignal_AddChannel(&s_clk, "other", PIF_ID_AUTO, CSVT_REG, 8, 0));	// Already added
    CHECK(strcmp(s_clk._name, "clk") == 0);

    _resetLog();
    CHECK(pifCollectSignal_Start());
    CHECK(!pifCollectSignal_Start());
    CHECK(!pifCollectSignal_ChangeScale(CSS_1US));
    pifCollectSignal_Stop();
    CHECK(pifCollectSignal_ChangeScale(CSS_1US));

    // Clear lets go of every channel, so it can be added again.
    pifCollectSignal_Clear();
    CHECK(s_clk.__state == CS_CH_DETACHED);
    CHECK(s_collect_signal.p_head == NULL);
    pifCollectSignal_Init("top", NULL, 0);
    CHECK(pifCollectSignal_AddChannel(&s_clk, "clk", PIF_ID_AUTO, CSVT_WIRE, 1, 0));
    pifCollectSignal_Clear();
}

static void _stepEnd(PifSequence *p_owner)
{
    (void)p_owner;
}

static void _stepFirst(PifSequence *p_owner)
{
    pifSequence_Delay(p_owner, _stepEnd, 5);
}

// The state is put once a step has picked what follows, so the idle state a step runs in never
// shows, and the step count tells two steps of the same state apart.
static void testSequence()
{
    static const char *kSequence =
            "$version PIF 0.1.0 $end\n"
            "$timescale 1ms $end\n"
            "$scope module top $end\n"
            "$var reg 2 ! SQS_30[1:0] $end\n"
            "$var reg 8 \" SQN_30[7:0] $end\n"
            "$upscope $end\n"
            "$enddefinitions $end\n"
            "#0\n"
            "$dumpvars\n"
            "b0 !\n"
            "b0 \"\n"
            "$end\n"
            "b1 !\n"
            "b1 \"\n"
            "b10 !\n"
            "#5\n"
            "b10 \"\n"
            "b0 !\n"
            "#6\n";
    PifSequence sequence;

    _resetLog();
    pifCollectSignal_Init("top", NULL, 0);
    CHECK(pifSequence_Init(&sequence, 0x30, NULL));
    CHECK(pifSequence_SetCsFlag(&sequence, PIF_CS_INDEX_ALL, SQ_CSF_ALL_BIT));

    pif_cumulative_timer1ms = 0;
    CHECK(pifCollectSignal_Start());
    CHECK(pifSequence_Start(&sequence, _stepFirst));
    (*sequence._p_task->__evt_loop)(sequence._p_task);		// _stepFirst
    pif_cumulative_timer1ms = 5;
    (*sequence._p_task->__evt_loop)(sequence._p_task);		// _stepEnd
    CHECK(!pifSequence_IsRunning(&sequence));
    pif_cumulative_timer1ms = 6;
    pifCollectSignal_Stop();

    pifSequence_Clear(&sequence);
    CHECK(s_collect_signal.p_head == NULL);			// Clear took its channels out
    CHECK(strcmp(_afterDate(s_log), kSequence) == 0);
    pifCollectSignal_Clear();
}

static void testCode()
{
    char code[4];

    CHECK(_formatCode(code, 0) == 1 && strcmp(code, "!") == 0);
    CHECK(_formatCode(code, 93) == 1 && strcmp(code, "~") == 0);
    CHECK(_formatCode(code, 94) == 2 && strcmp(code, "!\"") == 0);
    CHECK(_formatCode(code, 65535) == 3);
}


// What a backend was told, in order.
static char s_events[256];
static size_t s_events_len;
static BOOL s_backend_refuse;

static void _event(const char *p_text)
{
    size_t len = strlen(p_text);

    if (s_events_len + len < sizeof(s_events)) {
        memcpy(s_events + s_events_len, p_text, len + 1);
        s_events_len += len;
    }
}

static BOOL _evtStart(void *p_client)
{
    const PifCollectSignalChannel *p_channel;
    char text[32];

    _event(p_client == &s_events ? "start" : "start?");
    // The channels of the capture and their values at the start.
    for (p_channel = pifCollectSignal_GetFirstChannel(); p_channel; p_channel = pifCollectSignal_GetNextChannel(p_channel)) {
        if (!pifCollectSignal_IsCapturing(p_channel)) continue;
        snprintf(text, sizeof(text), " %s=%lx", p_channel->_name, (unsigned long)p_channel->_value);
        _event(text);
    }
    _event(";");
    return !s_backend_refuse;
}

static void _evtChange(void *p_client, const PifCollectSignalChannel *p_channel, uint32_t value)
{
    char text[32];

    (void)p_client;
    snprintf(text, sizeof(text), "%lu:%s=%lx;", (unsigned long)pif_cumulative_timer1ms, p_channel->_name,
            (unsigned long)value);
    _event(text);
}

static void _evtStop(void *p_client)
{
    (void)p_client;
    _event("stop;");
}

static const PifCollectSignalBackend kBackend = { _evtStart, _evtChange, _evtStop };

// act_enable of the sources: p_object is the channel, added under the name of the module.
static BOOL _actSource(const PifCollectSignalSource *p_source, BOOL enable)
{
    PifCollectSignalChannel *p_channel = (PifCollectSignalChannel *)p_source->p_object;

    if (!enable) {
        pifCollectSignal_RemoveChannel(p_channel);
        return TRUE;
    }
    return pifCollectSignal_AddChannel(p_channel, "MOD", 0x12, CSVT_WIRE, 1, 0);
}

static void testSource()
{
    static PifCollectSignalChannel s_a, s_b, s_c;
    static const PifCollectSignalSource kSource[] = {
        { _actSource, &s_a, "Alpha", 1, 0, TRUE },
        { _actSource, &s_b, "Beta", 1, 0, FALSE },
        { _actSource, &s_c, "Gamma", 1, 0, TRUE }
    };

    // Only the sources with enable, each with its name.
    CHECK(pifCollectSignal_Init("top", kSource, 3));
    CHECK(pifCollectSignal_IsSourceEnabled(0) && !pifCollectSignal_IsSourceEnabled(1) &&
            pifCollectSignal_IsSourceEnabled(2));
    CHECK(pifCollectSignal_GetSource(&s_a) == &kSource[0] && pifCollectSignal_GetSource(&s_c) == &kSource[2]);

    // Added later, it comes last and keeps its name.
    CHECK(pifCollectSignal_EnableSource(1, TRUE));
    CHECK(s_collect_signal.p_head == &s_a && s_a.__p_next == &s_c && s_c.__p_next == &s_b);
    CHECK(pifCollectSignal_GetSource(&s_b) == &kSource[1]);
    CHECK(pifCollectSignal_EnableSource(0, FALSE) && !pifCollectSignal_IsSourceEnabled(0));
    CHECK(!pifCollectSignal_EnableSource(3, TRUE) && pif_error == E_INVALID_PARAM);

    // A channel added before its source is named by it all the same.
    CHECK(pifCollectSignal_AddChannel(&s_a, "MOD", 0x12, CSVT_WIRE, 1, 0));
    CHECK(pifCollectSignal_GetSource(&s_a) == NULL);
    CHECK(pifCollectSignal_EnableSource(0, TRUE) && pifCollectSignal_GetSource(&s_a) == &kSource[0]);

    // A channel added outside the table has none.
    pifCollectSignal_Init("top", NULL, 0);
    CHECK(pifCollectSignal_AddChannel(&s_b, "MOD", 0x12, CSVT_WIRE, 1, 0));
    CHECK(pifCollectSignal_GetSource(&s_b) == NULL);
    pifCollectSignal_Clear();
}

static void testBackend()
{
    static const PifCollectSignalBackend kIncomplete = { _evtStart, NULL, _evtStop };
    float real = 1.5f;
    uint32_t raw;
    char expect[128];

    pifCollectSignal_InitStatic("top", NULL, 0, sizeof(s_buffer), s_buffer);
    CHECK(_addChannels());
    CHECK(!pifCollectSignal_AttachBackend(&kIncomplete, &s_events) && pif_error == E_INVALID_PARAM);
    CHECK(pifCollectSignal_AttachBackend(&kBackend, &s_events));

    // The channels in the order they were added.
    CHECK(pifCollectSignal_GetFirstChannel() == &s_clk);
    CHECK(pifCollectSignal_GetNextChannel(&s_clk) == &s_bus);
    CHECK(pifCollectSignal_GetNextChannel(&s_temp) == NULL);
    CHECK(!pifCollectSignal_IsCapturing(&s_clk));

    // The same capture as the other methods, to the backend and none of it to the log.
    _resetLog();
    s_events_len = 0;
    _runCapture();
    memcpy(&raw, &real, sizeof(raw));
    snprintf(expect, sizeof(expect), "start clk=1 bus=5 temp=0;100:clk=0;103:bus=a;103:temp=%lx;107:clk=1;stop;",
            (unsigned long)raw);
    CHECK(strcmp(s_events, expect) == 0);
    CHECK(s_log_len == 0 && s_log_enable);
    CHECK(!pifCollectSignal_PrintLog() && pif_error == E_CANNOT_USE);

    // Not while capturing.
    CHECK(pifCollectSignal_Start());
    CHECK(pifCollectSignal_IsCapturing(&s_bus));
    CHECK(!pifCollectSignal_AttachBackend(NULL, NULL) && pif_error == E_INVALID_STATE);
    pifCollectSignal_Stop();

    // A refused start leaves the collector idle, and changes go nowhere.
    s_backend_refuse = TRUE;
    s_events_len = 0;
    CHECK(!pifCollectSignal_Start() && !pifCollectSignal_IsCollecting());
    s_backend_refuse = FALSE;
    pifCollectSignal_Put(&s_clk, 0);
    CHECK(strcmp(s_events, "start clk=1 bus=a temp=3fc00000;") == 0);

    // Clear stops the capture through the backend and detaches it.
    CHECK(pifCollectSignal_Start());
    s_events_len = 0;
    pifCollectSignal_Clear();
    CHECK(strcmp(s_events, "stop;") == 0);
    CHECK(!pifCollectSignal_GetFirstChannel());

    // Detached, the VCD output is back.
    pifCollectSignal_Init("top", NULL, 0);
    CHECK(_addChannels());
    _resetLog();
    s_events_len = 0;
    s_events[0] = 0;
    _runCapture();
    CHECK(s_events_len == 0);
    CHECK(strcmp(_afterDate(s_log), kExpect) == 0);
    pifCollectSignal_Clear();
}

static const struct {
    const char *p_name;
    void (*run)(void);
} s_tests[] = {
    { "backend", testBackend },
    { "log", testLog },
    { "buffer", testBuffer },
    { "buffer retry", testBufferRetry },
    { "overflow keep first", testOverflowKeepFirst },
    { "overflow keep last", testOverflowKeepLast },
    { "buffer size", testBufferSize },
    { "channel changes", testChannelChanges },
    { "state checks", testStateChecks },
    { "sequence", testSequence },
    { "source", testSource },
    { "code", testCode }
};

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(4, 2);
}

int main(void)
{
    int failed = 0;
    size_t t;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("pif_collect_signal: setup failed (%d)\n", pif_error);
        return 1;
    }

    printf("pif_collect_signal\n");
    for (t = 0; t < sizeof(s_tests) / sizeof(s_tests[0]); t++) {
        s_failed = FALSE;
        (*s_tests[t].run)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", s_tests[t].p_name);
        if (s_failed) failed++;
    }
    printf("%d failed\n", failed);
    return failed ? 1 : 0;
}
