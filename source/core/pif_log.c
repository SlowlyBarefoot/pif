// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_log.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


// The print functions, the log buffer and the outputs, with an optional command line.

#define PREFIX_LINE_SIZE	10		// "\nSS.mmm X "
#define PREFIX_SIZE			32		// The time line of 16 bytes, the line prefix and the terminator

// A piece of the log buffer: type, prefix length and text length, then the prefix and the text.
#define PIECE_HEADER		3
#define TEXT_CHUNK			(PIF_LOG_LINE_SIZE < 255 ? PIF_LOG_LINE_SIZE : 255)	// A longer text is cut into pieces
#define PIECE_MAX			(PIECE_HEADER + PREFIX_SIZE + TEXT_CHUNK)

// Type of a piece of command output that found no room in the transmit buffer. Only the UART takes it.
#define LT_CONSOLE			((PifLogType)6)

#define DRAIN_BUDGET		16		// Pieces an output gets in one run of the task
#define RETRY_DELAY			1000	// Microseconds before an output that refused is offered again
#define DUMP_CHUNK			64		// Bytes read from a sink at a time
#define SHOW_LINES			20		// Lines of "log show" without a count

// What a run of the task leaves, in order of urgency
#define DRAIN_DONE			0
#define DRAIN_REFUSED		1		// An output had no room: try again after RETRY_DELAY
#define DRAIN_MORE			2		// The budget ran out: run again at once


typedef struct StPifLogPiece
{
	PifLogType type;
	uint16_t length;			// Bytes in the log buffer
	const char *p_prefix;		// NULL when there is none
	const char *p_text;
} PifLogPiece;

#ifdef PIF_LOG_COMMAND

// What "log show" and "log cat" still have to send
typedef struct StPifLogDump
{
	BOOL active;
	PifLogSink *p_sink;			// NULL for the log buffer
	uint32_t pos;				// Offset in the sink, or position in the log buffer
	uint32_t end;				// End offset in the sink, or pieces left in the log buffer
} PifLogDump;

#endif

typedef struct StPifLog
{
	BOOL enable;
	PifUart* p_uart;
    PifRingBuffer* p_tx_buffer;
	PifLogSink uart_sink;		// The UART as an output, first in p_sinks while attached
	PifLogSink *p_sinks;
	PifTask *p_task;
	BOOL console_direct;		// A command runs: its LT_NONE lines go to the UART at once
	BOOL console_queued;		// A line of the command found no room: the rest waits in the log buffer behind it

	// The log buffer. Positions run from 0 to wrap, a multiple of size, so a position modulo size is its
	// index and the distance between two positions is never ambiguous.
	uint8_t *p_buffer;
	BOOL heap;
	uint16_t size;
	uint32_t wrap;
	uint32_t head;				// Where the next piece goes
	uint32_t tail;				// Oldest piece held

#ifdef PIF_LOG_COMMAND
    char last_char;
    uint8_t char_idx;
    BOOL cmd_done;
    uint8_t rx_buffer_size;
    char* p_rx_buffer;
	char* p_argv[PIF_LOG_CMD_MAX_ARGS + 1];
	const PifLogCmdEntry* p_cmd_table;
	const char* p_prompt;
	PifEvtLogControlChar evt_control_char;
	PifLogDump dump;
	BOOL prompt_pending;		// The prompt waits for the dump to end
#endif
} PifLog;


PifLogFlag pif_log_flag = { .all = 0L };

static PifLog s_log;
static uint8_t s_minute = 255;
static char s_piece[PREFIX_SIZE + TEXT_CHUNK + 2];	// The piece an output is being given

static const char type_ch[] = { 'I', 'W', 'E', 'C' };


static uint32_t _dist(uint32_t to, uint32_t from)
{
	return to >= from ? to - from : to + s_log.wrap - from;
}

static uint32_t _advance(uint32_t pos, uint32_t length)
{
	pos += length;
	return pos >= s_log.wrap ? pos - s_log.wrap : pos;
}

static uint8_t _byteAt(uint32_t pos, uint16_t offset)
{
	return s_log.p_buffer[_advance(pos, offset) % s_log.size];
}

static uint16_t _pieceLength(uint32_t pos)
{
	return PIECE_HEADER + _byteAt(pos, 1) + _byteAt(pos, 2);
}

// Copies the piece at pos into s_piece. Called with interrupts off, since a print may push it out.
static void _copyPiece(uint32_t pos, PifLogPiece *p_piece)
{
	uint8_t prefix_len = _byteAt(pos, 1);
	uint8_t text_len = _byteAt(pos, 2);
	char *p = s_piece;
	uint16_t i;

	p_piece->type = (PifLogType)_byteAt(pos, 0);
	p_piece->length = PIECE_HEADER + prefix_len + text_len;
	for (i = 0; i < prefix_len; i++) *p++ = _byteAt(pos, PIECE_HEADER + i);
	*p++ = 0;
	p_piece->p_prefix = prefix_len ? s_piece : NULL;
	p_piece->p_text = p;
	for (i = 0; i < text_len; i++) *p++ = _byteAt(pos, PIECE_HEADER + prefix_len + i);
	*p = 0;
}

static void _putBytes(uint32_t *p_pos, const void *p_data, uint16_t length)
{
	const uint8_t *p = p_data;

	while (length--) {
		s_log.p_buffer[*p_pos % s_log.size] = *p++;
		*p_pos = _advance(*p_pos, 1);
	}
}

// Stores one piece. The oldest pieces give way when there is no room, and an output that had not got
// one counts it as lost.
static void _storePiece(PifLogType type, const char *p_prefix, uint8_t prefix_len, const char *p_text,
		uint8_t text_len)
{
	uint8_t header[PIECE_HEADER] = { (uint8_t)type, prefix_len, text_len };
	uint16_t length = PIECE_HEADER + prefix_len + text_len;
	PifLogSink *p_sink;
	uint32_t state, pos;

	state = pif_IrqDisable();
	while (s_log.size - _dist(s_log.head, s_log.tail) < length) {
		pos = _advance(s_log.tail, _pieceLength(s_log.tail));
		for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
			if (p_sink->__read_pos != s_log.tail) continue;
			p_sink->__read_pos = pos;
			if (p_sink->type_mask & PIF_LOG_TYPE_MASK(_byteAt(s_log.tail, 0))) p_sink->drop_count++;
		}
#ifdef PIF_LOG_COMMAND
		if (s_log.dump.active && !s_log.dump.p_sink && s_log.dump.pos == s_log.tail) {
			s_log.dump.pos = pos;
			s_log.dump.end--;
		}
#endif
		s_log.tail = pos;
	}

	pos = s_log.head;
	_putBytes(&pos, header, PIECE_HEADER);
	_putBytes(&pos, p_prefix, prefix_len);
	_putBytes(&pos, p_text, text_len);
	s_log.head = pos;
	pif_IrqRestore(state);
}

static void _store(PifLogType type, const char *p_prefix, const char *p_text)
{
	size_t prefix_len = p_prefix ? strlen(p_prefix) : 0;
	size_t text_len = strlen(p_text);
	size_t length;

	do {
		length = text_len > TEXT_CHUNK ? TEXT_CHUNK : text_len;
		_storePiece(type, p_prefix, (uint8_t)prefix_len, p_text, (uint8_t)length);
		p_prefix = NULL;
		prefix_len = 0;
		p_text += length;
		text_len -= length;
	} while (text_len);

	if (s_log.p_task) pifTask_SetTrigger(s_log.p_task, 0);
}

// Hands one output what it can take of the log buffer, up to budget pieces.
static int _drainSink(PifLogSink *p_sink, int budget)
{
	PifLogPiece piece;
	uint32_t state, pos;

	while (budget--) {
		state = pif_IrqDisable();
		pos = p_sink->__read_pos;
		if (pos == s_log.head) {
			pif_IrqRestore(state);
			return DRAIN_DONE;
		}
		_copyPiece(pos, &piece);
		pif_IrqRestore(state);

		if ((p_sink->type_mask & PIF_LOG_TYPE_MASK(piece.type)) &&
				!(*p_sink->act_write)(p_sink, piece.type, piece.p_prefix, piece.p_text)) {
			return DRAIN_REFUSED;
		}

		// Unless a print pushed the piece out meanwhile, which has moved the position already.
		state = pif_IrqDisable();
		if (p_sink->__read_pos == pos) p_sink->__read_pos = _advance(pos, piece.length);
		pif_IrqRestore(state);
	}
	return DRAIN_MORE;
}

static int _drain(int budget)
{
	PifLogSink *p_sink;
	int status = DRAIN_DONE, result;

	if (!s_log.p_buffer) return DRAIN_DONE;

	for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
#ifdef PIF_LOG_COMMAND
		// The terminal belongs to the dump until it ends. The lines wait in the log buffer.
		if (p_sink == &s_log.uart_sink && s_log.dump.active) continue;
#endif
		result = _drainSink(p_sink, budget);
		if (result > status) status = result;
	}
	return status;
}

static BOOL _uartWrite(PifLogSink *p_sink, PifLogType type, const char *p_prefix, const char *p_text)
{
	size_t prefix_len = p_prefix ? strlen(p_prefix) : 0;
	size_t text_len = strlen(p_text);

	(void)p_sink;

	if (!s_log.p_uart || (!s_log.enable && type != LT_VCD)) return TRUE;

	// The piece goes whole or not at all. Waiting for room would mean holding the CPU until the UART
	// task had drained the buffer, which lets a log line decide how long every other task waits.
	if (pifRingBuffer_GetRemainSize(s_log.p_tx_buffer) < prefix_len + text_len) {
		pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
		return FALSE;
	}
	if (prefix_len) pifRingBuffer_PutData(s_log.p_tx_buffer, (uint8_t*)p_prefix, prefix_len);
	pifRingBuffer_PutData(s_log.p_tx_buffer, (uint8_t*)p_text, text_len);
	pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
	return TRUE;
}

#ifdef PIF_LOG_COMMAND

static const struct {
	const char* p_name;
	const char* p_command;
} c_log_flags[] = {
		{ "Performance", "pf" },
		{ "Task", "tk" },
		{ "Collect Signal", "cs" },
		{ "Duty Motor", "dm" },
		{ "Step Motor", "sm" },

		{ NULL, NULL }
};

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
			if (!pif_StrCaseCmp(argv[0], c_log_flags[i].p_command)) {
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
		if (pif_StrNCaseCmp(p_entry->p_name, p_owner->p_rx_buffer, typed)) continue;

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
		if (pif_StrNCaseCmp(p_entry->p_name, p_owner->p_rx_buffer, p_owner->char_idx)) continue;
		pifRingBuffer_PutString(p_owner->p_tx_buffer, (char *)p_entry->p_name);
		pifRingBuffer_PutString(p_owner->p_tx_buffer, "  ");
	}
	pifRingBuffer_PutString(p_owner->p_tx_buffer, "\r\n");
	_redrawLine(p_owner);
}

static BOOL _getDebugString(PifLog* p_owner, PifActTransportReceive act_receive_data)
{
    char tmp_char;
    BOOL str_get_done_flag = FALSE;
    uint8_t enter = 0;
    static uint8_t pre_enter = 0;

	while ((*act_receive_data)(&p_owner->p_uart->parent, (uint8_t*)&tmp_char, 1)) {
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
			if (!pif_StrCaseCmp(p_owner->p_argv[0], p_cmd_entry->p_name)) {
				return p_cmd_entry->processor(argc - 1, &p_owner->p_argv[1]);
            }

			p_cmd_entry++;
    	}
        return PIF_LOG_CMD_BAD_CMD;
    }
    return PIF_LOG_CMD_NO_ERROR;
}

// Text that ends the output of a command goes behind what of it waits in the log buffer.
static void _putConsole(const char *p_text)
{
	if (s_log.console_queued) _store(LT_CONSOLE, NULL, p_text);
	else pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)p_text);
}

static void _putPrompt()
{
	_putConsole(s_log.p_prompt);
	pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
}

static void _endDump(const char *p_message)
{
	s_log.dump.active = FALSE;
	if (!s_log.p_uart) return;

	if (p_message) pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)p_message);
	if (s_log.prompt_pending) {
		s_log.prompt_pending = FALSE;
		_putPrompt();
	}
	s_log.console_queued = FALSE;
}

// Sends as much of the dump as the transmit buffer takes.
static int _dumpStep()
{
	PifLogDump *p_dump = &s_log.dump;
	PifLogPiece piece;
	uint8_t buffer[DUMP_CHUNK];
	uint16_t room, length;
	size_t prefix_len;
	uint32_t state, pos;
	int budget = DRAIN_BUDGET;

	if (!p_dump->active) return DRAIN_DONE;
	if (!s_log.p_uart) {
		p_dump->active = FALSE;
		return DRAIN_DONE;
	}

	while (budget--) {
		room = pifRingBuffer_GetRemainSize(s_log.p_tx_buffer);
		if (p_dump->p_sink) {
			length = p_dump->end - p_dump->pos > DUMP_CHUNK ? DUMP_CHUNK : p_dump->end - p_dump->pos;
			if (length > room) length = room;
			if (!length) {
				if (p_dump->pos < p_dump->end) return DRAIN_REFUSED;
				_endDump("\n");
				return DRAIN_DONE;
			}
			length = (*p_dump->p_sink->act_read)(p_dump->p_sink, p_dump->pos, buffer, length);
			if (!length) {
				_endDump("\n");
				return DRAIN_DONE;
			}
			pifRingBuffer_PutData(s_log.p_tx_buffer, buffer, length);
			p_dump->pos += length;
		}
		else {
			state = pif_IrqDisable();
			pos = p_dump->pos;
			if (!p_dump->end) {
				pif_IrqRestore(state);
				_endDump("\n");
				return DRAIN_DONE;
			}
			_copyPiece(pos, &piece);
			pif_IrqRestore(state);

			// The output of an earlier command is not part of the log.
			if (piece.type != LT_CONSOLE) {
				prefix_len = piece.p_prefix ? strlen(piece.p_prefix) : 0;
				if (room < prefix_len + strlen(piece.p_text)) return DRAIN_REFUSED;
				if (prefix_len) pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)piece.p_prefix);
				pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)piece.p_text);
			}

			state = pif_IrqDisable();
			if (p_dump->pos == pos) {
				p_dump->pos = _advance(pos, piece.length);
				p_dump->end--;
			}
			pif_IrqRestore(state);
		}
		pifTask_SetTrigger(s_log.p_uart->_p_tx_task, 0);
	}
	return DRAIN_MORE;
}

// Offset in a readable sink of the last lines. Lines start with their newline.
static uint32_t _findLastLines(PifLogSink *p_sink, uint32_t size, uint32_t lines)
{
	uint8_t buffer[DUMP_CHUNK];
	uint32_t pos = size;
	uint16_t length;

	while (pos) {
		length = pos > DUMP_CHUNK ? DUMP_CHUNK : pos;
		pos -= length;
		length = (*p_sink->act_read)(p_sink, pos, buffer, length);
		while (length--) {
			if (buffer[length] == '\n' && !--lines) return pos + length;
		}
	}
	return 0;
}

static BOOL _isLineStart(uint32_t pos)
{
	// A line starts with its newline, in the prefix when it has one.
	return (_byteAt(pos, 1) || _byteAt(pos, 2)) && _byteAt(pos, PIECE_HEADER) == '\n';
}

// Sets the dump of the log buffer to its last lines, or to all of it with lines 0.
static void _startBufferDump(uint32_t lines)
{
	uint32_t state, pos, total = 0, skip;

	state = pif_IrqDisable();
	for (pos = s_log.tail; pos != s_log.head; pos = _advance(pos, _pieceLength(pos))) {
		if (_isLineStart(pos)) total++;
	}
	skip = lines && total > lines ? total - lines : 0;

	pos = s_log.tail;
	while (pos != s_log.head) {
		if (_isLineStart(pos)) {
			if (!skip) break;
			skip--;
		}
		pos = _advance(pos, _pieceLength(pos));
	}
	s_log.dump.pos = pos;
	s_log.dump.end = 0;
	for (; pos != s_log.head; pos = _advance(pos, _pieceLength(pos))) s_log.dump.end++;
	s_log.dump.p_sink = NULL;
	s_log.dump.active = TRUE;
	pif_IrqRestore(state);
}

int pifLog_CmdLog(int argc, char *argv[])
{
	PifLogSink *p_sink;
	uint32_t lines = SHOW_LINES;
	BOOL cat;
	char *p_end;

	if (argc == 0 || !pif_StrCaseCmp(argv[0], "list")) {
		if (s_log.p_buffer) {
			pifLog_Printf(LT_NONE, "  ram: %lu/%u bytes\n", (unsigned long)_dist(s_log.head, s_log.tail), s_log.size);
		}
		for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
			if (p_sink->act_size && p_sink->act_read) {
				pifLog_Printf(LT_NONE, "  %s: %lu bytes, lost %lu\n", p_sink->p_name ? p_sink->p_name : "-",
						(unsigned long)(*p_sink->act_size)(p_sink), (unsigned long)p_sink->drop_count);
			}
			else {
				pifLog_Printf(LT_NONE, "  %s: lost %lu\n", p_sink->p_name ? p_sink->p_name : "-",
						(unsigned long)p_sink->drop_count);
			}
		}
		return PIF_LOG_CMD_NO_ERROR;
	}

	cat = !pif_StrCaseCmp(argv[0], "cat");
	if (!cat && pif_StrCaseCmp(argv[0], "show")) return PIF_LOG_CMD_INVALID_ARG;
	if (argc < 2) return PIF_LOG_CMD_TOO_FEW_ARGS;
	if (argc > (cat ? 2 : 3)) return PIF_LOG_CMD_TOO_MANY_ARGS;
	if (argc == 3) {
		lines = strtoul(argv[2], &p_end, 10);
		if (*p_end || !lines) return PIF_LOG_CMD_INVALID_ARG;
	}
	if (cat) lines = 0;

	if (!pif_StrCaseCmp(argv[1], "ram")) {
		if (!s_log.p_buffer) return PIF_LOG_CMD_INVALID_ARG;
		_startBufferDump(lines);
	}
	else {
		for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
			if (p_sink->p_name && !pif_StrCaseCmp(p_sink->p_name, argv[1])) break;
		}
		if (!p_sink || !p_sink->act_size || !p_sink->act_read) return PIF_LOG_CMD_INVALID_ARG;

		s_log.dump.p_sink = p_sink;
		s_log.dump.end = (*p_sink->act_size)(p_sink);
		s_log.dump.pos = lines ? _findLastLines(p_sink, s_log.dump.end, lines) : 0;
		s_log.dump.active = TRUE;
	}
	if (s_log.p_task) pifTask_SetTrigger(s_log.p_task, 0);
	return PIF_LOG_CMD_NO_ERROR;
}

static BOOL _evtParsing(void* p_client, PifActTransportReceive act_receive_data)
{
	PifLog* p_owner = (PifLog*)p_client;
	char ch;

	// A key stops a dump, and is not taken as input.
	if (p_owner->dump.active) {
		if ((*act_receive_data)(&p_owner->p_uart->parent, (uint8_t*)&ch, 1)) {
			while ((*act_receive_data)(&p_owner->p_uart->parent, (uint8_t*)&ch, 1));
			_endDump("\n^C");
		}
		return FALSE;
	}

    if (p_owner->cmd_done == FALSE) {
        if (_getDebugString(p_owner, act_receive_data)) {
        	p_owner->cmd_done = TRUE;
        	pifTask_SetTrigger(p_owner->p_task, 0);
        }
    }
    return p_owner->cmd_done;
}

static void _runCommand(PifTask *p_task)
{
    int i, status = PIF_LOG_CMD_NO_ERROR;
    char msg[40];

	// A command typed on the terminal, such as the task statistics, is not part of the load being
	// measured, and neither is the lateness it causes the other tasks.
	pifTask_IgnoreStatistics(p_task);

	// What the command prints goes to the terminal at once, ahead of the prompt.
	s_log.console_direct = TRUE;
	status = _processDebugCmd(&s_log);
	s_log.console_direct = FALSE;

	while (s_log.char_idx) {
		s_log.p_rx_buffer[s_log.char_idx] = 0;
		s_log.char_idx--;
	}

	for (i = 0; i < PIF_LOG_CMD_MAX_ARGS; i++) {
		s_log.p_argv[i] = 0;
	}

	// The UART may have been detached while the command was waiting for this task.
	if (!s_log.p_uart) {
		s_log.cmd_done = FALSE;
		s_log.dump.active = FALSE;
		s_log.console_queued = FALSE;
		return;
	}

	switch (status) {
	case PIF_LOG_CMD_BAD_CMD:
		// Handle the case of bad command.
		_putConsole("\nNot defined command!");
		break;

	case PIF_LOG_CMD_TOO_MANY_ARGS:
		// Handle the case of too many arguments.
		_putConsole("\nToo many arguments!");
		break;

	case PIF_LOG_CMD_TOO_FEW_ARGS:
		// Handle the case of too few arguments.
		_putConsole("\nToo few arguments!");
		break;

	case PIF_LOG_CMD_INVALID_ARG:
		_putConsole("\nInvalid arguments!");
		break;

	default:
		// Otherwise the command was executed.  Print the error
		// code if one was returned, user defined ones included.
		if (status < PIF_LOG_CMD_NO_ERROR) {
			pif_Printf(msg, sizeof(msg), "\nCommand returned error code: %d", status);
			_putConsole(msg);
		}
		break;
	}

	if (s_log.dump.active) {
		s_log.prompt_pending = TRUE;
	}
	else {
		_putPrompt();
		s_log.console_queued = FALSE;
	}
	s_log.cmd_done = FALSE;
}

#else

static BOOL _evtParsing(void* p_client, PifActTransportReceive act_receive_data)
{
    (void)p_client;
    (void)act_receive_data;

    return FALSE;
}

#endif

static uint32_t _doTask(PifTask *p_task)
{
	int status = DRAIN_DONE, result;

#ifdef PIF_LOG_COMMAND
	if (s_log.cmd_done) _runCommand(p_task);
	status = _dumpStep();
#endif
	result = _drain(DRAIN_BUDGET);
	if (result > status) status = result;

	if (status == DRAIN_MORE) pifTask_SetTrigger(p_task, 0);
	else if (status == DRAIN_REFUSED) pifTask_SetTrigger(p_task, RETRY_DELAY);
	return 0;
}

static BOOL _addTask()
{
	if (s_log.p_task) return TRUE;

	s_log.p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_EXTERNAL, 0, _doTask, &s_log, FALSE);
	if (!s_log.p_task) return FALSE;
	s_log.p_task->name = "Log";
	return TRUE;
}

static uint16_t _evtSending(void* p_client, PifActTransportSend act_send_data)
{
	PifLog* p_owner = (PifLog*)p_client;
	uint16_t length = 0;
	uint16_t size;
	uint8_t* p_data;

	if (!pifRingBuffer_IsEmpty(p_owner->p_tx_buffer)) {
		p_data = pifRingBuffer_GetReadPointer(p_owner->p_tx_buffer, 0, &size);
		length = (*act_send_data)(&p_owner->p_uart->parent, p_data, size);
		pifRingBuffer_Remove(p_owner->p_tx_buffer, length);
	}
	return length > 0 ? length : !pifRingBuffer_IsEmpty(p_owner->p_tx_buffer);
}

static void _printLog(PifLogType type, const char *p_prefix, const char *p_text)
{
	PifLogSink *p_sink;

	if (p_prefix && !*p_prefix) p_prefix = NULL;

	if (s_log.console_direct && type == LT_NONE) {
		if (!s_log.p_uart) return;
		if (!s_log.console_queued && _uartWrite(&s_log.uart_sink, type, p_prefix, p_text)) return;
		// No room: the line and every one after it until the prompt wait in the log buffer, so that the
		// output of a command longer than the transmit buffer still comes out whole and in order.
		if (s_log.p_buffer) {
			s_log.console_queued = TRUE;
			_store(LT_CONSOLE, p_prefix, p_text);
		}
		else {
			s_log.uart_sink.drop_count++;
		}
		return;
	}

	// Collect signal lines are too many to wait in the log buffer.
	if (s_log.p_buffer && type != LT_VCD) {
		_store(type, p_prefix, p_text);
		return;
	}

	for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
		if (!(p_sink->type_mask & PIF_LOG_TYPE_MASK(type))) continue;
		if (!(*p_sink->act_write)(p_sink, type, p_prefix, p_text)) p_sink->drop_count++;
	}
}

// Writes into p_buf what goes before the text of a line of this type, and returns its length.
static int _makePrefix(PifLogType type, char *p_buf)
{
	int offset = 0;

	p_buf[0] = 0;
	if (type < LT_INFO || type > LT_COMM) return 0;

	if (s_minute != pif_datetime.minute) {
		p_buf[offset++] = '\n';
		offset += pif_DecToString(p_buf + offset, (uint32_t)pif_datetime.second, 2);
		p_buf[offset++] = '.';
		offset += pif_DecToString(p_buf + offset, (uint32_t)pif_timer1ms, 3);
		p_buf[offset++] = ' ';
		p_buf[offset++] = 'T';
		p_buf[offset++] = ' ';
		offset += pif_DecToString(p_buf + offset, (uint32_t)pif_datetime.hour, 2);
		p_buf[offset++] = ':';
		offset += pif_DecToString(p_buf + offset, (uint32_t)pif_datetime.minute, 2);
		p_buf[offset++] = ' ';
		s_minute = pif_datetime.minute;
	}

	p_buf[offset++] = '\n';
	offset += pif_DecToString(p_buf + offset, (uint32_t)pif_datetime.second, 2);
	p_buf[offset++] = '.';
	offset += pif_DecToString(p_buf + offset, (uint32_t)pif_timer1ms, 3);
	p_buf[offset++] = ' ';
	p_buf[offset++] = type_ch[type - LT_INFO];
	p_buf[offset++] = ' ';
	p_buf[offset] = 0;
	return offset;
}

BOOL pifLog_Init()
{
	memset(&s_log, 0, sizeof(PifLog));

	s_log.enable = TRUE;
#ifdef PIF_LOG_COMMAND
	if (!_addTask()) return FALSE;
#endif
   	return TRUE;
}

static BOOL _initBuffer(uint16_t size, uint8_t *p_buffer, BOOL heap)
{
	// A piece of the longest kind has to fit.
	if (size < PIECE_MAX || (!heap && !p_buffer)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!pifLog_Init()) return FALSE;

	if (heap) {
		p_buffer = malloc(size);
		if (!p_buffer) {
			pif_error = E_OUT_OF_HEAP;
			pifLog_Clear();
			return FALSE;
		}
	}
	s_log.p_buffer = p_buffer;
	s_log.heap = heap;
	s_log.size = size;
	s_log.wrap = size * (0x80000000UL / size);

	if (!_addTask()) {
		pifLog_Clear();
		return FALSE;
	}
	return TRUE;
}

BOOL pifLog_InitHeap(uint16_t size)
{
	return _initBuffer(size, NULL, TRUE);
}

BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer)
{
	return _initBuffer(size, p_buffer, FALSE);
}

void pifLog_Clear()
{
	// The UART client is detached too, so neither the UART task nor a later print reaches the
	// transmit buffer after it is freed.
	pifLog_DetachUart();
	s_log.p_sinks = NULL;
	if (s_log.p_task) {
		pifTaskManager_Remove(s_log.p_task);
		s_log.p_task = NULL;
	}
	if (s_log.p_buffer && s_log.heap) free(s_log.p_buffer);
	s_log.p_buffer = NULL;
#ifdef PIF_LOG_COMMAND
	s_log.dump.active = FALSE;
	if (s_log.p_rx_buffer) {
		free(s_log.p_rx_buffer);
		s_log.p_rx_buffer = NULL;
	}
#endif
}

#ifdef PIF_LOG_COMMAND

BOOL pifLog_UseCommand(uint8_t size, const PifLogCmdEntry* p_cmd_table, const char* p_prompt)
{
    // Three bytes are kept free for the completion space, the terminator and a margin,
    // so a smaller buffer would leave no room for a single character.
    if (size < 4 || !p_cmd_table || !p_prompt) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    if (s_log.p_rx_buffer) free(s_log.p_rx_buffer);
    s_log.char_idx = 0;
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
	PifLogSink *p_sink;

	if (!s_log.p_buffer) return TRUE;
	for (p_sink = s_log.p_sinks; p_sink; p_sink = p_sink->__p_next) {
		if (p_sink->__read_pos != s_log.head) return FALSE;
	}
	return TRUE;
}

void pifLog_PrintChar(char ch)
{
	char text[2] = { ch, 0 };

	_printLog(LT_NONE, NULL, text);
}

uint32_t pifLog_DropCount()
{
	return s_log.uart_sink.drop_count;
}

void pifLog_ResetDropCount()
{
	s_log.uart_sink.drop_count = 0UL;
}

void pifLog_Print(PifLogType type, const char* p_string)
{
	char prefix[PREFIX_SIZE];

	_makePrefix(type, prefix);
	_printLog(type, prefix, p_string);
}

void pifLog_Printf(PifLogType type, const char* p_format, ...)
{
	va_list data;
	char prefix[PREFIX_SIZE];
    char tmp_buf[PIF_LOG_LINE_SIZE];

	// The line, the prefix included, is still cut at PIF_LOG_LINE_SIZE.
	int size = _makePrefix(type, prefix) ? PIF_LOG_LINE_SIZE - PREFIX_LINE_SIZE : PIF_LOG_LINE_SIZE;

	va_start(data, p_format);
	pif_PrintFormat(tmp_buf, size, &data, p_format);
	va_end(data);

	_printLog(type, prefix, tmp_buf);
}

BOOL pifLog_PrintInBuffer()
{
	_drain(DRAIN_BUDGET);
	return s_log.p_buffer && s_log.p_uart && s_log.uart_sink.__read_pos != s_log.head;
}

BOOL pifLog_AttachUart(PifUart* p_uart, uint16_t size)
{
	uint32_t state;

    if (!p_uart || !size) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    pifLog_DetachUart();
    s_log.p_tx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, size);
    if (!s_log.p_tx_buffer) return FALSE;

	s_log.p_uart = p_uart;
	pifTransport_AttachClient(&p_uart->parent, &s_log, _evtParsing, _evtSending, NULL);

	// The UART is the first output, and gets what the log buffer still holds.
	s_log.uart_sink.act_write = _uartWrite;
	s_log.uart_sink.type_mask = PIF_LOG_TYPE_ALL | PIF_LOG_TYPE_MASK(LT_CONSOLE);
	s_log.uart_sink.p_name = "uart";
	state = pif_IrqDisable();
	s_log.uart_sink.__read_pos = s_log.tail;
	s_log.uart_sink.__p_next = s_log.p_sinks;
	s_log.p_sinks = &s_log.uart_sink;
	pif_IrqRestore(state);
	if (s_log.p_task && !pifLog_IsEmpty()) pifTask_SetTrigger(s_log.p_task, 0);

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

	pifLog_RemoveSink(&s_log.uart_sink);
	pifTransport_DetachClient(&s_log.p_uart->parent);
	s_log.p_uart = NULL;

	pifRingBuffer_Destroy(&s_log.p_tx_buffer);
	s_log.console_queued = FALSE;
}

BOOL pifLog_AddSink(PifLogSink *p_sink)
{
	PifLogSink **pp;
	uint32_t state;

	if (!p_sink || !p_sink->act_write) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Appended, so sinks get each piece in the order they were added. Adding one twice does nothing.
	for (pp = &s_log.p_sinks; *pp; pp = &(*pp)->__p_next) {
		if (*pp == p_sink) return TRUE;
	}
	p_sink->__p_next = NULL;
	p_sink->drop_count = 0UL;
	state = pif_IrqDisable();
	p_sink->__read_pos = s_log.tail;
	*pp = p_sink;
	pif_IrqRestore(state);
	if (s_log.p_task && s_log.tail != s_log.head) pifTask_SetTrigger(s_log.p_task, 0);
	return TRUE;
}

void pifLog_RemoveSink(PifLogSink *p_sink)
{
	PifLogSink **pp;
	uint32_t state;

#ifdef PIF_LOG_COMMAND
	if (s_log.dump.active && s_log.dump.p_sink == p_sink) s_log.dump.active = FALSE;
#endif
	for (pp = &s_log.p_sinks; *pp; pp = &(*pp)->__p_next) {
		if (*pp == p_sink) {
			state = pif_IrqDisable();
			*pp = p_sink->__p_next;
			pif_IrqRestore(state);
			p_sink->__p_next = NULL;
			return;
		}
	}
}

void pifLog_SendAndExit()
{
	// Blocks until the UART has sent everything for it, so it must still be able to send.
	if (!s_log.p_uart || !s_log.p_uart->_p_tx_task) return;

	do {
		_drain(DRAIN_BUDGET);
		while (pifRingBuffer_GetFillSize(s_log.p_tx_buffer)) {
			s_log.p_uart->_p_tx_task->__evt_loop(s_log.p_uart->_p_tx_task);
		}
	} while (s_log.p_buffer && s_log.uart_sink.__read_pos != s_log.head);
}
