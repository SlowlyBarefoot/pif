// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"

#include <stdio.h>
#include <string.h>


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

// Keeps the last piece it got, and counts them.
typedef struct
{
    PifLogSink parent;
    BOOL refuse;
    int count;
    PifLogType type;
    BOOL has_prefix;
    char prefix[40];
    char text[PIF_LOG_LINE_SIZE];
} TestSink;

static BOOL _actWrite(PifLogSink *p_sink, PifLogType type, const char *p_prefix, const char *p_text)
{
    TestSink *p_owner = (TestSink*)p_sink;

    if (p_owner->refuse) return FALSE;
    p_owner->count++;
    p_owner->type = type;
    p_owner->has_prefix = p_prefix != NULL;
    strcpy(p_owner->prefix, p_prefix ? p_prefix : "");
    strcpy(p_owner->text, p_text);
    return TRUE;
}

static void _initSink(TestSink *p_sink, uint8_t type_mask)
{
    memset(p_sink, 0, sizeof(TestSink));
    p_sink->parent.act_write = _actWrite;
    p_sink->parent.type_mask = type_mask;
}


static void _testPrefix(void)
{
    TestSink sink;

    _initSink(&sink, PIF_LOG_TYPE_ALL);
    CHECK(pifLog_Init());
    CHECK(pifLog_AddSink(&sink.parent));

    // The first line of a minute brings the time line in its prefix.
    pifLog_Print(LT_INFO, "first");
    CHECK(sink.count == 1);
    CHECK(sink.type == LT_INFO);
    CHECK(strstr(sink.prefix, " T ") != NULL);
    CHECK(strcmp(sink.prefix + strlen(sink.prefix) - 3, " I ") == 0);
    CHECK(strcmp(sink.text, "first") == 0);

    // The next one only the line prefix "\nSS.mmm X ".
    pifLog_Printf(LT_ERROR, "n=%d", 12);
    CHECK(sink.count == 2);
    CHECK(sink.type == LT_ERROR);
    CHECK(strlen(sink.prefix) == 10);
    CHECK(sink.prefix[0] == '\n' && sink.prefix[9] == ' ' && sink.prefix[8] == 'E');
    CHECK(strcmp(sink.text, "n=12") == 0);

    // No prefix for the plain types.
    pifLog_Print(LT_NONE, "plain");
    CHECK(sink.count == 3 && sink.type == LT_NONE && !sink.has_prefix);
    pifLog_PrintChar('x');
    CHECK(sink.count == 4 && sink.type == LT_NONE && !sink.has_prefix && strcmp(sink.text, "x") == 0);

    pifLog_Clear();
}

static void _testTypeMask(void)
{
    TestSink all, errors;

    _initSink(&all, PIF_LOG_TYPE_ALL);
    _initSink(&errors, PIF_LOG_TYPE_MASK(LT_ERROR));
    CHECK(pifLog_Init());
    CHECK(pifLog_AddSink(&all.parent));
    CHECK(pifLog_AddSink(&errors.parent));

    pifLog_Print(LT_INFO, "info");
    pifLog_Print(LT_VCD, "vcd");
    pifLog_Print(LT_ERROR, "error");
    CHECK(all.count == 3);
    CHECK(errors.count == 1 && strcmp(errors.text, "error") == 0);

    pifLog_Clear();
}

static void _testDisabled(void)
{
    TestSink sink;

    // pifLog_Disable() is for the UART only.
    _initSink(&sink, PIF_LOG_TYPE_ALL);
    CHECK(pifLog_Init());
    CHECK(pifLog_AddSink(&sink.parent));
    pifLog_Disable();
    pifLog_Print(LT_WARN, "warn");
    CHECK(sink.count == 1);

    pifLog_Clear();
}

static void _testAddRemove(void)
{
    TestSink a, b;
    PifLogSink empty = { 0 };

    _initSink(&a, PIF_LOG_TYPE_ALL);
    _initSink(&b, PIF_LOG_TYPE_ALL);
    CHECK(pifLog_Init());
    CHECK(!pifLog_AddSink(NULL));
    CHECK(!pifLog_AddSink(&empty));
    CHECK(pifLog_AddSink(&a.parent));
    CHECK(pifLog_AddSink(&a.parent));	// Twice is once
    CHECK(pifLog_AddSink(&b.parent));

    pifLog_Print(LT_NONE, "1");
    CHECK(a.count == 1 && b.count == 1);

    pifLog_RemoveSink(&a.parent);
    pifLog_Print(LT_NONE, "2");
    CHECK(a.count == 1 && b.count == 2);

    pifLog_Clear();
    CHECK(pifLog_Init());
    pifLog_Print(LT_NONE, "3");
    CHECK(b.count == 2);

    pifLog_Clear();
}

static void _testLineSize(void)
{
    TestSink sink;
    char text[PIF_LOG_LINE_SIZE * 2];

    memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;

    _initSink(&sink, PIF_LOG_TYPE_ALL);
    CHECK(pifLog_Init());
    CHECK(pifLog_AddSink(&sink.parent));

    // The line, the prefix included, is cut at PIF_LOG_LINE_SIZE.
    pifLog_Printf(LT_INFO, "%s", text);
    CHECK(strlen(sink.text) == PIF_LOG_LINE_SIZE - 10 - 1);
    pifLog_Printf(LT_NONE, "%s", text);
    CHECK(strlen(sink.text) == PIF_LOG_LINE_SIZE - 1);

    pifLog_Clear();
}

static void _testDirectRefused(void)
{
    TestSink sink;

    // Without the log buffer a refused piece is lost.
    _initSink(&sink, PIF_LOG_TYPE_ALL);
    sink.refuse = TRUE;
    CHECK(pifLog_Init());
    CHECK(pifLog_AddSink(&sink.parent));

    pifLog_Print(LT_NONE, "abc");
    pifLog_PrintChar('d');
    CHECK(sink.parent.drop_count == 2);
    CHECK(pifLog_IsEmpty());

    pifLog_Clear();
}

// Keeps everything it gets, prefixes and texts one after another.
typedef struct
{
    PifLogSink parent;
    BOOL refuse;
    int count;
    char all[2048];
} CollectSink;

static BOOL _actCollect(PifLogSink *p_sink, PifLogType type, const char *p_prefix, const char *p_text)
{
    CollectSink *p_owner = (CollectSink*)p_sink;

    (void)type;
    if (p_owner->refuse) return FALSE;
    p_owner->count++;
    if (p_prefix) strcat(p_owner->all, p_prefix);
    strcat(p_owner->all, p_text);
    return TRUE;
}

static void _initCollect(CollectSink *p_sink)
{
    memset(p_sink, 0, sizeof(CollectSink));
    p_sink->parent.act_write = _actCollect;
    p_sink->parent.type_mask = PIF_LOG_TYPE_ALL;
}

static void _drainAll(void)
{
    int i;

    for (i = 0; i < 100 && !pifLog_IsEmpty(); i++) pifLog_PrintInBuffer();
}

static void _testBuffered(void)
{
    CollectSink sink;
    TestSink plain;

    CHECK(!pifLog_InitHeap(32));    // Smaller than the longest piece
    CHECK(pifLog_InitHeap(512));
    _initCollect(&sink);
    _initSink(&plain, PIF_LOG_TYPE_MASK(LT_ERROR));
    CHECK(pifLog_AddSink(&sink.parent));
    CHECK(pifLog_AddSink(&plain.parent));

    // A print only stores the line. The outputs get it from the task.
    pifLog_Printf(LT_INFO, "value %d", 7);
    pifLog_Print(LT_ERROR, "bad");
    CHECK(sink.count == 0 && plain.count == 0);
    CHECK(!pifLog_IsEmpty());

    _drainAll();
    CHECK(pifLog_IsEmpty());
    CHECK(sink.count == 2);
    CHECK(strstr(sink.all, " I value 7") != NULL);
    CHECK(strstr(sink.all, " E bad") != NULL);
    CHECK(plain.count == 1 && strcmp(plain.text, "bad") == 0 && plain.has_prefix);

    // Collect signal lines do not wait.
    pifLog_Print(LT_VCD, "#100");
    CHECK(sink.count == 3 && strstr(sink.all, "#100") != NULL);

    pifLog_Clear();
}

static void _testRefusedWaits(void)
{
    CollectSink sink;

    CHECK(pifLog_InitHeap(512));
    _initCollect(&sink);
    CHECK(pifLog_AddSink(&sink.parent));

    // With the buffer a refused piece is offered again, not lost.
    sink.refuse = TRUE;
    pifLog_Print(LT_NONE, "one");
    pifLog_PrintChar('2');
    _drainAll();
    CHECK(sink.count == 0 && sink.parent.drop_count == 0);

    sink.refuse = FALSE;
    _drainAll();
    CHECK(strcmp(sink.all, "one2") == 0);

    pifLog_Clear();
}

static void _testOverflow(void)
{
    CollectSink sink;
    char text[16];
    int i;

    CHECK(pifLog_InitHeap(160));
    _initCollect(&sink);
    CHECK(pifLog_AddSink(&sink.parent));

    // Each piece takes 3 + 5 bytes, so 160 bytes hold the last 20 of 40.
    sink.refuse = TRUE;
    for (i = 0; i < 40; i++) {
        pif_Printf(text, sizeof(text), "L%d|", i + 100);
        pifLog_Print(LT_NONE, text);
    }
    CHECK(sink.parent.drop_count == 20);

    sink.refuse = FALSE;
    _drainAll();
    CHECK(sink.count == 20);
    CHECK(strncmp(sink.all, "L120|", 5) == 0);
    CHECK(strcmp(sink.all + strlen(sink.all) - 5, "L139|") == 0);

    pifLog_Clear();
}

static void _testLateSinkAndChunks(void)
{
    CollectSink sink;
    char text[301];

    memset(text, 'x', 300);
    text[300] = 0;
    text[0] = 'a';
    text[299] = 'z';

    CHECK(pifLog_InitHeap(1024));

    // A long text is cut into pieces, and a sink added later still gets what the buffer holds.
    pifLog_Print(LT_NONE, text);
    _initCollect(&sink);
    CHECK(pifLog_AddSink(&sink.parent));
    _drainAll();
    CHECK(sink.count > 1);
    CHECK(strcmp(sink.all, text) == 0);

    pifLog_Clear();
}

// ---------------------------------------------------------------- the UART and the command line

static PifUart s_uart;
static char s_wire[4096];
static size_t s_wire_len;

static uint16_t _actSend(PifUart *p_uart, uint8_t *p_data, uint16_t size)
{
    (void)p_uart;
    if (size > sizeof(s_wire) - 1 - s_wire_len) size = sizeof(s_wire) - 1 - s_wire_len;
    memcpy(s_wire + s_wire_len, p_data, size);
    s_wire_len += size;
    s_wire[s_wire_len] = 0;
    return size;
}

static void _wireClear(void)
{
    s_wire_len = 0;
    s_wire[0] = 0;
}

static void _loop(int times)
{
    while (times--) pifTaskManager_Loop();
}

static BOOL _attachUart(void)
{
    if (!pifUart_Init(&s_uart, PIF_ID_AUTO, 115200)) return FALSE;
    s_uart.act_send_data = _actSend;
    if (!pifUart_AllocRxBuffer(&s_uart, 64)) return FALSE;
    if (!pifUart_AttachRxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, "RX")) return FALSE;
    if (!pifUart_AttachTxTask(&s_uart, PIF_ID_AUTO, TM_EXTERNAL, 0, "TX")) return FALSE;
    if (!pifLog_AttachUart(&s_uart, 128)) return FALSE;
    _wireClear();
    return TRUE;
}

static void _detachUart(void)
{
    pifLog_Clear();
    pifUart_Clear(&s_uart);
}

static void _testUartBuffered(void)
{
    CHECK(pifLog_InitHeap(1024));

    // Lines printed before the UART is attached reach it once it is.
    pifLog_Print(LT_NONE, "early ");
    CHECK(_attachUart());
    pifLog_Print(LT_NONE, "late");
    CHECK(s_wire_len == 0);
    _loop(20);
    CHECK(strcmp(s_wire, "early late") == 0);

    // A disabled UART skips the lines.
    pifLog_Disable();
    pifLog_Print(LT_NONE, "-hidden");
    _loop(20);
    pifLog_Enable();
    pifLog_Print(LT_NONE, "-shown");
    _loop(20);
    CHECK(strcmp(s_wire, "early late-shown") == 0);

    // pifLog_SendAndExit() sends everything without the task.
    pifLog_Print(LT_NONE, "!bye");
    pifLog_SendAndExit();
    CHECK(strcmp(s_wire + s_wire_len - 4, "!bye") == 0);

    _detachUart();
}

static void _testUartFull(void)
{
    char text[40];
    int i;

    // The transmit buffer of 128 bytes is far smaller than what is printed: nothing is lost, it waits.
    CHECK(pifLog_InitHeap(2048));
    CHECK(_attachUart());
    for (i = 0; i < 30; i++) {
        pif_Printf(text, sizeof(text), "line %d of the long run|", i + 10);
        pifLog_Print(LT_NONE, text);
    }
    _loop(2000);
    CHECK(pifLog_DropCount() == 0);
    CHECK(strstr(s_wire, "line 10 of") == s_wire);
    CHECK(strstr(s_wire, "line 39 of the long run|") != NULL);
    CHECK(s_wire_len == 30 * strlen("line 10 of the long run|"));

    _detachUart();
}

#ifdef PIF_LOG_COMMAND

// A readable sink: the text it got, kept in memory as on a card.
typedef struct
{
    PifLogSink parent;
    char data[2048];
    uint32_t size;
} FileSink;

static BOOL _actFileWrite(PifLogSink *p_sink, PifLogType type, const char *p_prefix, const char *p_text)
{
    FileSink *p_owner = (FileSink*)p_sink;

    (void)type;
    if (p_prefix) {
        strcpy(p_owner->data + p_owner->size, p_prefix);
        p_owner->size += strlen(p_prefix);
    }
    strcpy(p_owner->data + p_owner->size, p_text);
    p_owner->size += strlen(p_text);
    return TRUE;
}

static uint32_t _actFileSize(PifLogSink *p_sink)
{
    return ((FileSink*)p_sink)->size;
}

static uint16_t _actFileRead(PifLogSink *p_sink, uint32_t offset, uint8_t *p_buffer, uint16_t size)
{
    FileSink *p_owner = (FileSink*)p_sink;

    if (offset >= p_owner->size) return 0;
    if (size > p_owner->size - offset) size = p_owner->size - offset;
    memcpy(p_buffer, p_owner->data + offset, size);
    return size;
}

static int _cmdHello(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    pifLog_Print(LT_NONE, "\nhello");
    return PIF_LOG_CMD_NO_ERROR;
}

// More than the transmit buffer of 128 bytes in one go, as the task list of pifLog_CmdPrintTask() is.
static int _cmdLines(int argc, char *argv[])
{
    char text[40];
    int i;

    (void)argc;
    (void)argv;
    for (i = 0; i < 12; i++) {
        pif_Printf(text, sizeof(text), "\nline %d of the long output", i + 10);
        pifLog_Print(LT_NONE, text);
    }
    return PIF_LOG_CMD_NO_ERROR;
}

static const PifLogCmdEntry kCommands[] = {
    { "log", pifLog_CmdLog, "Log outputs", "list | show <name> [lines] | cat <name>" },
    { "hello", _cmdHello, "Says hello", NULL },
    { "lines", _cmdLines, "Prints more than the transmit buffer", NULL },
    { NULL, NULL, NULL, NULL }
};

static void _type(const char *p_text)
{
    pifUart_PutRxData(&s_uart, (uint8_t*)p_text, strlen(p_text));
    _loop(50);
}

static void _testCommandOrder(void)
{
    CHECK(pifLog_InitHeap(1024));
    CHECK(pifLog_UseCommand(32, kCommands, "\n> "));
    CHECK(_attachUart());
    _loop(10);
    _wireClear();

    // What the command prints comes before the prompt, though the log buffer is in between.
    pifLog_Print(LT_NONE, "\nqueued");
    _type("hello\r");
    CHECK(strstr(s_wire, "hello\n\nhello\n> ") != NULL);
    CHECK(strstr(s_wire, "\nqueued") != NULL);

    _detachUart();
}

static void _testCommandLong(void)
{
    CollectSink sink;
    char expect[40];
    char *p_pos = s_wire;
    int i;

    CHECK(pifLog_InitHeap(1024));
    CHECK(pifLog_UseCommand(32, kCommands, "\n> "));
    CHECK(_attachUart());
    _initCollect(&sink);
    CHECK(pifLog_AddSink(&sink.parent));
    _loop(10);
    _wireClear();

    // The lines that find the transmit buffer full wait in the log buffer, and the prompt behind them.
    _type("lines\r");
    _loop(200);
    for (i = 0; i < 12; i++) {
        pif_Printf(expect, sizeof(expect), "\nline %d of the long output", i + 10);
        p_pos = strstr(p_pos, expect);
        CHECK(p_pos != NULL);
        if (!p_pos) break;
    }
    CHECK(strcmp(s_wire + s_wire_len - 3, "\n> ") == 0);
    CHECK(pifLog_DropCount() == 0);
    // Only the UART gets them.
    CHECK(sink.count == 0);

    // The next command goes out at once again.
    _wireClear();
    _type("hello\r");
    CHECK(strstr(s_wire, "hello\n\nhello\n> ") != NULL);

    pifLog_RemoveSink(&sink.parent);
    _detachUart();
}

static void _testCommandShow(void)
{
    FileSink file;
    char text[24];
    int i;

    CHECK(pifLog_InitHeap(1024));
    CHECK(pifLog_UseCommand(32, kCommands, "\n> "));
    memset(&file, 0, sizeof(file));
    file.parent.act_write = _actFileWrite;
    file.parent.type_mask = PIF_LOG_TYPE_ALL;
    file.parent.p_name = "sd";
    file.parent.act_size = _actFileSize;
    file.parent.act_read = _actFileRead;
    CHECK(pifLog_AddSink(&file.parent));
    for (i = 0; i < 30; i++) {
        pif_Printf(text, sizeof(text), "\nrow %d", i + 10);
        pifLog_Print(LT_NONE, text);
    }
    CHECK(_attachUart());
    _loop(500);
    CHECK(strstr(s_wire, "row 39") != NULL);
    _wireClear();

    // The last 3 lines of the readable sink.
    _type("log show sd 3\r");
    _loop(200);
    CHECK(strstr(s_wire, "\nrow 37\nrow 38\nrow 39") != NULL);
    CHECK(strstr(s_wire, "row 36") == NULL);
    CHECK(strcmp(s_wire + s_wire_len - 3, "\n> ") == 0);
    _wireClear();

    // The same of the log buffer.
    _type("log show ram 2\r");
    _loop(200);
    CHECK(strstr(s_wire, "\nrow 38\nrow 39") != NULL);
    CHECK(strstr(s_wire, "row 37") == NULL);
    _wireClear();

    // All of it, through a transmit buffer of 128 bytes.
    _type("log cat sd\r");
    _loop(2000);
    CHECK(strstr(s_wire, "\nrow 10\nrow 11") != NULL);
    CHECK(strstr(s_wire, "\nrow 39") != NULL);
    _wireClear();

    _type("log list\r");
    CHECK(strstr(s_wire, "ram: ") != NULL);
    CHECK(strstr(s_wire, "uart: lost 0") != NULL);
    CHECK(strstr(s_wire, "sd: ") != NULL);
    _wireClear();

    _type("log show usb\r");
    CHECK(strstr(s_wire, "Invalid arguments!") != NULL);
    _type("log show uart\r");
    CHECK(strstr(s_wire, "Invalid arguments!") != NULL);

    _detachUart();
}

static void _testDumpStop(void)
{
    char text[24];
    int i;

    CHECK(pifLog_InitHeap(2048));
    CHECK(pifLog_UseCommand(32, kCommands, "\n> "));
    for (i = 0; i < 60; i++) {
        pif_Printf(text, sizeof(text), "\nrow %d", i + 10);
        pifLog_Print(LT_NONE, text);
    }
    CHECK(_attachUart());
    _loop(2000);
    _wireClear();

    // A key stops the dump, and the lines printed meanwhile follow it.
    pifUart_PutRxData(&s_uart, (uint8_t*)"log cat ram\r", 12);
    _loop(3);
    pifLog_Print(LT_NONE, "\nduring");
    pifUart_PutRxData(&s_uart, (uint8_t*)"x", 1);
    _loop(500);
    CHECK(strstr(s_wire, "^C\n> ") != NULL);
    CHECK(strstr(s_wire, "row 69") == NULL);
    CHECK(strstr(s_wire, "\nduring") > strstr(s_wire, "^C"));

    _detachUart();
}

#endif

typedef struct
{
    const char *p_name;
    void (*test)(void);
} TestCase;

static uint32_t _timer1us(void)
{
    static uint32_t s_us;

    return s_us += 50;
}

static BOOL _portSetup(void)
{
    return pifTaskManager_Init(8, 2);
}

int main(void)
{
    static const TestCase kTests[] = {
        { "prefix", _testPrefix },
        { "type mask", _testTypeMask },
        { "disabled", _testDisabled },
        { "add remove", _testAddRemove },
        { "line size", _testLineSize },
        { "direct refused", _testDirectRefused },
        { "buffered", _testBuffered },
        { "refused waits", _testRefusedWaits },
        { "overflow", _testOverflow },
        { "late sink and chunks", _testLateSinkAndChunks },
        { "uart buffered", _testUartBuffered },
        { "uart full", _testUartFull },
#ifdef PIF_LOG_COMMAND
        { "command order", _testCommandOrder },
        { "command long", _testCommandLong },
        { "command show", _testCommandShow },
        { "dump stop", _testDumpStop },
#endif
    };
    unsigned i, failed = 0;

    if (!pif_Init(_timer1us, _portSetup, NULL)) {
        printf("test_pif_log: setup failed (%d)\n", pif_error);
        return 1;
    }
#ifdef PIF_LOG_COMMAND
    printf("test_pif_log (PIF_LOG_COMMAND)\n");
#else
    printf("test_pif_log\n");
#endif
    for (i = 0; i < sizeof(kTests) / sizeof(kTests[0]); i++) {
        s_failed = FALSE;
        (*kTests[i].test)();
        printf("  %s %s\n", s_failed ? "FAIL" : "ok  ", kTests[i].p_name);
        if (s_failed) failed++;
    }
    printf("%u failed\n", failed);
    return failed ? 1 : 0;
}
