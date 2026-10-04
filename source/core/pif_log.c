// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

// Logging backend with UART output and optional command-line parser.

typedef struct StPifLog
{
	BOOL enable;
	PifRingBuffer buffer;
	PifUart* p_uart;
    PifRingBuffer* p_tx_buffer;
	uint32_t drop_count;		// Lines and characters the transmit buffer had no room for

#ifdef PIF_LOG_COMMAND
    PifTask* p_task;
    char last_char;
    uint8_t char_idx;
    BOOL cmd_done;
    uint8_t rx_buffer_size;
    char* p_rx_buffer;
	char* p_argv[PIF_LOG_CMD_MAX_ARGS + 1];
	const PifLogCmdEntry* p_cmd_table;
	const char* p_prompt;
	PifEvtLogControlChar evt_control_char;
#endif
} PifLog;


PifLogFlag pif_log_flag = { .all = 0L };

static PifLog s_log;
static uint8_t s_minute = 255;

const struct {
	char* p_name;
	char* p_command;
} c_log_flags[] = {
		{ "Performance", "pf" },
		{ "Task", "tk" },
		{ "Collect Signal", "cs" },
		{ "Duty Motor", "dm" },
		{ "Step Motor", "sm" },

		{ NULL, NULL }
};
const char type_ch[] = { 'I', 'W', 'E', 'C' };


#ifdef PIF_LOG_COMMAND

int pifLog_CmdHelp(int argc, char *argv[])
{
    int i;
    const PifLogCmdEntry* p_entry;

    (void)argc;
    (void)argv;

    pifLog_Print(LT_NONE, "Available commands:\n");
    i = 0;
    while (1) {
        p_entry = &s_log.p_cmd_table[i];
        if (!p_entry->p_name) break;

        pifLog_Printf(LT_NONE, "  %s - %s\n", p_entry->p_name, p_entry->p_help);
    	if (p_entry->p_args) {
            pifLog_Printf(LT_NONE, "\t%s\n", p_entry->p_args);
    	}
        i++;
    }
	return PIF_LOG_CMD_NO_ERROR;
}

int pifLog_CmdPrintVersion(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	pifLog_Printf(LT_NONE, "PIF Version: %d.%d.%d %s\n", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH, __DATE__);
	return PIF_LOG_CMD_NO_ERROR;
}

int pifLog_CmdPrintTask(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	pifTaskManager_Print();
	return PIF_LOG_CMD_NO_ERROR;
}

int pifLog_CmdSetStatus(int argc, char* argv[])
{
	BOOL value;
	int i;

	if (argc == 0) {
		pifLog_Printf(LT_NONE, "Task Load: %u%%\n", pif_performance._task_load);
	   	pifLog_Printf(LT_NONE, "Error: %d\n", pif_error);
	   	pifLog_Printf(LT_NONE, "Flag:\n");
	   	i = 0;
		while (c_log_flags[i].p_name) {
			pifLog_Printf(LT_NONE, "  %s(%s): %d\n", c_log_flags[i].p_name, c_log_flags[i].p_command, (pif_log_flag.all >> i) & 1);
			i++;
		}
		return PIF_LOG_CMD_NO_ERROR;
	}
	else if (argc > 1) {
		switch (argv[1][0]) {
		case '0':
		case 'F':
		case 'f':
			value = FALSE;
			break;

		case '1':
		case 'T':
		case 't':
			value = TRUE;
			break;

		default:
			return PIF_LOG_CMD_INVALID_ARG;
		}
		i = 0;
		while (c_log_flags[i].p_name) {
			if (!strcasecmp(argv[0], c_log_flags[i].p_command)) {
				if (value) {
					pif_log_flag.all |= 1L << i;
				}
				else {
					pif_log_flag.all &= ~(1L << i);
				}
				return PIF_LOG_CMD_NO_ERROR;
			}
			i++;
		}
		return PIF_LOG_CMD_INVALID_ARG;
	}
	return PIF_LOG_CMD_TOO_FEW_ARGS;
}

static void _echoString(PifLog* p_owner, const char* p_string, uint8_t length)
{
	while (length--) pifRingBuffer_PutByte(p_owner->p_tx_buffer, *p_string++);
}

static void _redrawLine(PifLog* p_owner)
{
	pifRingBuffer_PutString(p_owner->p_tx_buffer, (char *)p_owner->p_prompt);
	_echoString(p_owner, p_owner->p_rx_buffer, p_owner->char_idx);
}

static void _insertChar(PifLog* p_owner, char ch)
{
	// The first word is the command name, so a line never starts with a space.
	// Three bytes stay free for the completion space, the terminator and a margin.
	if (ch == ' ' && !p_owner->char_idx) return;
	if (p_owner->char_idx >= p_owner->rx_buffer_size - 3) return;

	p_owner->p_rx_buffer[p_owner->char_idx++] = ch;
	pifRingBuffer_PutByte(p_owner->p_tx_buffer, ch);
}

static void _eraseChar(PifLog* p_owner)
{
	if (!p_owner->char_idx) return;

	p_owner->p_rx_buffer[--p_owner->char_idx] = 0;
	pifRingBuffer_PutString(p_owner->p_tx_buffer, "\b \b");		// Step back, blank the cell, step back again
}

static void _clearScreen(PifLog* p_owner)
{
	pifRingBuffer_PutString(p_owner->p_tx_buffer, "\033[2J\033[H");	// ANSI: erase display, cursor home
	_redrawLine(p_owner);
}

/**
 * @fn _completeCommand
 * @brief Completes the command name typed so far against the command table.
 *
 * Every entry whose name starts with the typed text (ignoring case) is a candidate.
 * The line grows to the longest prefix all candidates share. A single candidate is
 * completed in full and followed by a space. With several candidates, or with an
 * empty line, the candidates are listed on their own line and the line is redrawn.
 * Completion only applies while the first word is typed.
 * @param p_owner Pointer to the owner instance.
 * @return None.
 */
static void _completeCommand(PifLog* p_owner)
{
	const PifLogCmdEntry* p_entry;
	const char* p_first = NULL;
	uint8_t typed = p_owner->char_idx;
	uint8_t shared = 0;
	uint8_t count = 0;
	uint8_t limit = p_owner->rx_buffer_size - 3;
	uint8_t n;

	if (memchr(p_owner->p_rx_buffer, ' ', typed)) return;

	for (p_entry = p_owner->p_cmd_table; p_entry->p_name; p_entry++) {
		if (strncasecmp(p_entry->p_name, p_owner->p_rx_buffer, typed)) continue;

		if (!p_first) {
			p_first = p_entry->p_name;
			shared = strlen(p_first);
		}
		else {
			n = typed;
			while (n < shared && tolower((unsigned char)p_entry->p_name[n]) == tolower((unsigned char)p_first[n])) n++;
			shared = n;
		}
		count++;
	}
	if (!count) return;

	if (shared > limit) shared = limit;
	if (shared > typed) {
		memcpy(p_owner->p_rx_buffer + typed, p_first + typed, shared - typed);
		p_owner->char_idx = shared;
	}

	if (count == 1) {
		if (p_owner->char_idx < limit) p_owner->p_rx_buffer[p_owner->char_idx++] = ' ';
		_echoString(p_owner, p_owner->p_rx_buffer + typed, p_owner->char_idx - typed);
		return;
	}

	if (shared > typed && typed) {
		_echoString(p_owner, p_owner->p_rx_buffer + typed, p_owner->char_idx - typed);
		return;
	}

	pifRingBuffer_PutString(p_owner->p_tx_buffer, "\r\n");
	for (p_entry = p_owner->p_cmd_table; p_entry->p_name; p_entry++) {
		if (strncasecmp(p_entry->p_name, p_owner->p_rx_buffer, p_owner->char_idx)) continue;
		pifRingBuffer_PutString(p_owner->p_tx_buffer, (char *)p_entry->p_name);
		pifRingBuffer_PutString(p_owner->p_tx_buffer, "  ");
	}
	pifRingBuffer_PutString(p_owner->p_tx_buffer, "\r\n");
	_redrawLine(p_owner);
}

static BOOL _getDebugString(PifLog* p_owner, PifActUartReceiveData act_receive_data)
{
    char tmp_char;
    BOOL str_get_done_flag = FALSE;
    uint8_t enter = 0;
    static uint8_t pre_enter = 0;

	while ((*act_receive_data)(p_owner->p_uart, (uint8_t*)&tmp_char, 1)) {
		if (tmp_char >= 32 && tmp_char <= 126) {
			_insertChar(p_owner, tmp_char);
		}
		else {
			switch (tmp_char) {
			case '\b':		// 0x08 / Backspace / CTRL-H
			case 0x7F:		// Delete
				_eraseChar(p_owner);
				break;

			case '\t':		// 0x09 / Horizontal Tab / CTRL-I
				_completeCommand(p_owner);
				break;

			case '\n':		// 0x0A / Line Feed / CTRL-J
				enter = 1;
				break;

			case '\r':		// 0x0D / Carriage Return / CTRL-M
				enter = 2;
				break;

			case 0x0C:		// Form Feed, New Page / CTRL-L
				_clearScreen(p_owner);
				break;

			default:
				if (p_owner->evt_control_char) (*p_owner->evt_control_char)(tmp_char);
            	break;
			}
        }

		if (enter) {
			if (p_owner->char_idx) {
				str_get_done_flag = TRUE;
				pre_enter = enter;
			}
			else if (!pre_enter || enter == pre_enter) {
				pifRingBuffer_PutString(p_owner->p_tx_buffer, (char *)p_owner->p_prompt);
				pre_enter = enter;
			}
			enter = 0;
		}

        if (str_get_done_flag == TRUE) {
        	// Drop the spaces at the end of the line
            while (p_owner->char_idx > 0 && p_owner->p_rx_buffer[p_owner->char_idx - 1] == ' ') {
            	p_owner->char_idx--;
            }
            if (p_owner->char_idx) {
				p_owner->p_rx_buffer[p_owner->char_idx] = 0;
				pifRingBuffer_PutByte(p_owner->p_tx_buffer, '\n');
	        	break;
            }
            else {
            	str_get_done_flag = FALSE;
        	}
        }
    }
	if (!pifRingBuffer_IsEmpty(p_owner->p_tx_buffer)) {
		pifTask_SetTrigger(p_owner->p_uart->_p_tx_task, 0);
	}
    return str_get_done_flag;
}

static int _processDebugCmd(PifLog* p_owner)
{
    char* p_tmp_cmd;
    BOOL find_arg;
    unsigned int argc;
    const PifLogCmdEntry* p_cmd_entry;

    find_arg = TRUE;
    argc = 0;
    p_tmp_cmd = p_owner->p_rx_buffer;

    while (*p_tmp_cmd) {
        if (*p_tmp_cmd == ' ') {
            *p_tmp_cmd = 0;
            find_arg = TRUE;
        }
        else {
            if (find_arg) {
                if (argc < PIF_LOG_CMD_MAX_ARGS) {
                	p_owner->p_argv[argc] = p_tmp_cmd;
                	argc++;
                    find_arg = FALSE;
                }
                else {
                    return PIF_LOG_CMD_TOO_MANY_ARGS;
                }
            }
        }

        p_tmp_cmd++;
    }

    if (argc) {
		p_cmd_entry = p_owner->p_cmd_table;
		while (p_cmd_entry->p_name) {
			if (!strcasecmp(p_owner->p_argv[0], p_cmd_entry->p_name)) {
				return p_cmd_entry->processor(argc - 1, &p_owner->p_argv[1]);
            }

			p_cmd_entry++;
    	}
        return PIF_LOG_CMD_BAD_CMD;
    }
    return PIF_LOG_CMD_NO_ERROR;
}

static BOOL _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
	PifLog* p_owner = (PifLog*)p_client;

    if (p_owner->cmd_done == FALSE) {
        if (_getDebugString(p_owner, act_receive_data)) {
        	p_owner->cmd_done = TRUE;
        	pifTask_SetTrigger(p_owner->p_task, 0);
        }
    }
    return p_owner->cmd_done;
}

static uint32_t _doTask(PifTask* p_task)
{
    int status = PIF_LOG_CMD_NO_ERROR;
    char msg[40];

    (void)p_task;

	status = _processDebugCmd(&s_log);

	while (s_log.char_idx) {
		s_log.p_rx_buffer[s_log.char_idx] = 0;
		s_log.char_idx--;
	}

	for (int i = 0; i < PIF_LOG_CMD_MAX_ARGS; i++) {
		s_log.p_argv[i] = 0;
	}

	switch (status) {
	case PIF_LOG_CMD_BAD_CMD:
		// Handle the case of bad command.
		pifRingBuffer_PutString(s_log.p_tx_buffer, "\nNot defined command!");
		break;

	case PIF_LOG_CMD_TOO_MANY_ARGS:
		// Handle the case of too many arguments.
		pifRingBuffer_PutString(s_log.p_tx_buffer, "\nToo many arguments!");
		break;

	case PIF_LOG_CMD_TOO_FEW_ARGS:
		// Handle the case of too few arguments.
		pifRingBuffer_PutString(s_log.p_tx_buffer, "\nToo few arguments!");
		break;

	case PIF_LOG_CMD_INVALID_ARG:
		pifRingBuffer_PutString(s_log.p_tx_buffer, "\nInvalid arguments!");
		break;

	default:
		// Otherwise the command was executed.  Print the error
		// code if one was returned.
		if (status < PIF_LOG_CMD_NO_ERROR && status > PIF_LOG_CMD_USER_ERROR) {
			pif_Printf(msg, sizeof(msg), "\nCommand returned error code: %d", status);
			pifRingBuffer_PutString(s_log.p_tx_buffer, msg);
		}
		break;
	}

	pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)s_log.p_prompt);
	pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);

	s_log.cmd_done = FALSE;
	return 0;
}

#else

static BOOL _evtParsing(void* p_client, PifActUartReceiveData act_receive_data)
{
    (void)p_client;
    (void)act_receive_data;

    return FALSE;
}

#endif

static uint16_t _evtSending(void* p_client, PifActUartSendData act_send_data)
{
	PifLog* p_owner = (PifLog*)p_client;
	uint16_t length = 0;

	if (!pifRingBuffer_IsEmpty(p_owner->p_tx_buffer)) {
		length = (*act_send_data)(p_owner->p_uart, pifRingBuffer_GetTailPointer(p_owner->p_tx_buffer, 0),
    			pifRingBuffer_GetLinerSize(p_owner->p_tx_buffer, 0));
		pifRingBuffer_Remove(p_owner->p_tx_buffer, length);
	}
	return length > 0 ? length : !pifRingBuffer_IsEmpty(p_owner->p_tx_buffer);
}

static void _printLog(char* p_string, BOOL vcd)
{
	if (!vcd && pifRingBuffer_IsBuffer(&s_log.buffer)) {
		pifRingBuffer_PutString(&s_log.buffer, p_string);
	}

	if (s_log.p_uart && (s_log.enable || vcd)) {
		// The line is dropped when the transmit buffer has no room for it. Waiting for room would
		// mean holding the CPU until the UART task had drained the buffer, which lets a log line
		// decide how long every other task waits. pifLog_DropCount() is where the loss shows up
		// instead, and the buffer is still handed to the UART task so the next line has room.
        if (!pifRingBuffer_PutString(s_log.p_tx_buffer, p_string)) s_log.drop_count++;
        pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
	}
}

static void _printTime()
{
	int offset = 0;
    static char tmp_buf[20];

    tmp_buf[offset++] = '\n';
	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_datetime.second, 2);
	tmp_buf[offset++] = '.';
	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_timer1ms, 3);
	tmp_buf[offset++] = ' ';
	tmp_buf[offset++] = 'T';
	tmp_buf[offset++] = ' ';
	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_datetime.hour, 2);
	tmp_buf[offset++] = ':';
	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_datetime.minute, 2);
	tmp_buf[offset++] = ' ';
	tmp_buf[offset] = 0;

	_printLog(tmp_buf, FALSE);
}

BOOL pifLog_Init()
{
	memset(&s_log, 0, sizeof(PifLog));

	s_log.enable = TRUE;
#ifdef PIF_LOG_COMMAND
	s_log.p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, &s_log, FALSE);
	if (!s_log.p_task) return FALSE;
	s_log.p_task->name = "Log";
#endif
   	return TRUE;
}

BOOL pifLog_InitHeap(uint16_t size)
{
	if (!pifLog_Init()) return FALSE;
	if (!pifRingBuffer_InitHeap(&s_log.buffer, PIF_ID_AUTO, size)) {
		pifLog_Clear();
		return FALSE;
	}
	return TRUE;
}

BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer)
{
	if (!pifLog_Init()) return FALSE;
	if (!pifRingBuffer_InitStatic(&s_log.buffer, PIF_ID_AUTO, size, p_buffer)) {
		pifLog_Clear();
		return FALSE;
	}
	return TRUE;
}

void pifLog_Clear()
{
	pifRingBuffer_Clear(&s_log.buffer);
	if (s_log.p_tx_buffer) pifRingBuffer_Destroy(&s_log.p_tx_buffer);
#ifdef PIF_LOG_COMMAND
	if (s_log.p_task) {
		pifTaskManager_Remove(s_log.p_task);
		s_log.p_task = NULL;
	}
	if (s_log.p_rx_buffer) {
		free(s_log.p_rx_buffer);
		s_log.p_rx_buffer = NULL;
	}
#endif
}

#ifdef PIF_LOG_COMMAND

BOOL pifLog_UseCommand(uint8_t size, const PifLogCmdEntry* p_cmd_table, const char* p_prompt)
{
    if (!size || !p_cmd_table || !p_prompt) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    s_log.p_rx_buffer = calloc(size, sizeof(char));
    if (!s_log.p_rx_buffer) {
        pif_error = E_OUT_OF_HEAP;
		return FALSE;
    }
    s_log.rx_buffer_size = size;

    s_log.p_cmd_table = p_cmd_table;
    s_log.p_prompt = p_prompt;
    return TRUE;
}

void pifLog_AttachEvent(PifEvtLogControlChar evt_control_char)
{
	s_log.evt_control_char = evt_control_char;
}

#endif

void pifLog_Enable()
{
	s_log.enable = TRUE;
}

void pifLog_Disable()
{
	s_log.enable = FALSE;
}

BOOL pifLog_IsEmpty()
{
	return pifRingBuffer_IsEmpty(&s_log.buffer);
}

void pifLog_PrintChar(char ch)
{
	if (pifRingBuffer_IsBuffer(&s_log.buffer)) {
		pifRingBuffer_PutByte(&s_log.buffer, ch);
	}

	if (s_log.p_uart && s_log.enable) {
		// Dropped when the transmit buffer is full, as a whole line is in _printLog().
        if (!pifRingBuffer_PutByte(s_log.p_tx_buffer, ch)) s_log.drop_count++;
        pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
	}
}

uint32_t pifLog_DropCount()
{
	return s_log.drop_count;
}

void pifLog_ResetDropCount()
{
	s_log.drop_count = 0UL;
}

void pifLog_Print(PifLogType type, const char* p_string)
{
	int offset = 0;
    char tmp_buf[12];

    if (type >= LT_INFO) {
        if (s_minute != pif_datetime.minute) {
        	_printTime();
        	s_minute = pif_datetime.minute;
    	}

        tmp_buf[offset++] = '\n';
    	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_datetime.second, 2);
    	tmp_buf[offset++] = '.';
    	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_timer1ms, 3);
    	tmp_buf[offset++] = ' ';
    	tmp_buf[offset++] = type_ch[type - LT_INFO];
    	tmp_buf[offset++] = ' ';
    	tmp_buf[offset] = 0;
    	_printLog(tmp_buf, type == LT_VCD);
    }

	_printLog((char*)p_string, type == LT_VCD);
}

void pifLog_Printf(PifLogType type, const char* p_format, ...)
{
	va_list data;
	int offset = 0;
    char tmp_buf[PIF_LOG_LINE_SIZE];

    if (type >= LT_INFO) {
        if (s_minute != pif_datetime.minute) {
        	_printTime();
        	s_minute = pif_datetime.minute;
    	}

        tmp_buf[offset++] = '\n';
    	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_datetime.second, 2);
    	tmp_buf[offset++] = '.';
    	offset += pif_DecToString(tmp_buf + offset, (uint32_t)pif_timer1ms, 3);
    	tmp_buf[offset++] = ' ';
    	tmp_buf[offset++] = type_ch[type - LT_INFO];
    	tmp_buf[offset++] = ' ';
    }

	va_start(data, p_format);
	pif_PrintFormat(tmp_buf + offset, PIF_LOG_LINE_SIZE - offset, &data, p_format);
	va_end(data);

	_printLog(tmp_buf, type == LT_VCD);
}

BOOL pifLog_PrintInBuffer()
{
	uint16_t length;

	if (!s_log.p_uart || !s_log.p_uart->_p_tx_task || !pifRingBuffer_IsBuffer(&s_log.buffer)) return FALSE;
	if (pifRingBuffer_IsEmpty(&s_log.buffer)) return FALSE;

	// As much of the retained buffer as the transmit buffer will take right now. Emptying the rest
	// needs the UART task to run, so the caller is told to come back for the remainder instead of
	// the CPU being held until it has.
	length = pifRingBuffer_CopyAll(s_log.p_tx_buffer, &s_log.buffer, 0);
	if (length) {
		pifRingBuffer_Remove(&s_log.buffer, length);
		pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
	}
	return !pifRingBuffer_IsEmpty(&s_log.buffer);
}

BOOL pifLog_AttachUart(PifUart* p_uart, uint16_t size)
{
    if (!p_uart || !size) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    s_log.p_tx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, size);
    if (!s_log.p_tx_buffer) return FALSE;

	s_log.p_uart = p_uart;
	pifUart_AttachClient(p_uart, &s_log, _evtParsing, _evtSending);

#ifdef PIF_LOG_COMMAND
	if (s_log.p_prompt) {
        pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)s_log.p_prompt);
	    pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
    }
#endif
    return TRUE;
}

void pifLog_DetachUart()
{
    if (!s_log.p_uart) return;

	pifUart_DetachClient(s_log.p_uart);
	s_log.p_uart = NULL;

	pifRingBuffer_Destroy(&s_log.p_tx_buffer);
	s_log.p_tx_buffer = NULL;
}

void pifLog_SendAndExit()
{
	if (!s_log.p_uart) return;

	while (pifRingBuffer_GetFillSize(s_log.p_tx_buffer)) {
		s_log.p_uart->_p_tx_task->__evt_loop(s_log.p_uart->_p_tx_task);
	}
}
