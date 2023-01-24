#include "core/pif_log.h"

#include <stdarg.h>
#include <string.h>


typedef struct StPifLog
{
	BOOL enable;
	PifRingBuffer buffer;
	PifComm* p_comm;
    PifRingBuffer* p_tx_buffer;

#ifdef __PIF_LOG_COMMAND__
    PifTask* p_task;
    char last_char;
    uint8_t char_idx;
    BOOL cmd_done;
    uint8_t rx_buffer_size;
    char* p_rx_buffer;
	char* p_argv[PIF_LOG_CMD_MAX_ARGS + 1];
	const PifLogCmdEntry* p_cmd_table[2];
	const char* p_prompt;
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

#ifdef __PIF_LOG_COMMAND__

static int _cmdHelp(int argc, char *argv[]);
static int _cmdPrintVersion(int argc, char* argv[]);
static int _cmdSetStatus(int argc, char* argv[]);

const PifLogCmdEntry c_cmd_table[] = {
		{ "help", _cmdHelp, "This command" },
		{ "pifversion", _cmdPrintVersion, "Print version" },
		{ "pifstatus", _cmdSetStatus, "Set and print status" },

		{ NULL, NULL, NULL }
};


static int _cmdHelp(int argc, char *argv[])
{
    int i;

    (void)argc;
    (void)argv;

    pifLog_Print(LT_NONE, "Available PIF commands:\n");
    i = 0;
    while (c_cmd_table[i].p_name) {
        pifLog_Printf(LT_NONE, "  %s\t%s\n", c_cmd_table[i].p_name, c_cmd_table[i].p_help);
        i++;
    }

    pifLog_Print(LT_NONE, "\nAvailable user commands:\n");
    i = 0;
    while (s_log.p_cmd_table[1][i].p_name) {
        pifLog_Printf(LT_NONE, "  %s\t%s\n", s_log.p_cmd_table[1][i].p_name, s_log.p_cmd_table[1][i].p_help);
        i++;
    }
	return PIF_LOG_CMD_NO_ERROR;
}

static int _cmdPrintVersion(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	pifLog_Printf(LT_NONE, "PIF Version: %d.%d.%d %s\n", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH, __DATE__);
	return PIF_LOG_CMD_NO_ERROR;
}

static int _cmdSetStatus(int argc, char* argv[])
{
	BOOL value;
	int i;

	if (argc == 1) {
	   	pifLog_Printf(LT_NONE, "Task count: %d\n", pifTaskManager_Count());
	   	pifLog_Printf(LT_NONE, "Error: %d\n", pif_error);
	   	pifLog_Printf(LT_NONE, "Flag:\n");
	   	i = 0;
		while (c_log_flags[i].p_name) {
			pifLog_Printf(LT_NONE, "  %s(%s): %d\n", c_log_flags[i].p_name, c_log_flags[i].p_command, (pif_log_flag.all >> i) & 1);
			i++;
		}
		return PIF_LOG_CMD_NO_ERROR;
	}
	else if (argc > 2) {
		switch (argv[2][0]) {
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
			if (!strcmp(argv[1], c_log_flags[i].p_command)) {
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

static BOOL _getDebugString(PifLog* p_owner, PifActCommReceiveData act_receive_data)
{
    char tmp_char;
    BOOL str_get_done_flag = FALSE;
    static BOOL last_cr = FALSE;

	while ((*act_receive_data)(p_owner->p_comm, (uint8_t*)&tmp_char)) {
        switch (tmp_char) {
        case '\b':
            if (p_owner->char_idx > 0) {
            	while (!pifRingBuffer_PutString(p_owner->p_tx_buffer, "\b \b")) {
            		if (!pifTaskManager_Yield()) break;
            	}
                p_owner->char_idx--;
                p_owner->p_rx_buffer[p_owner->char_idx] = 0;
            }
            break;

        case '\r':
        	last_cr = TRUE;
            str_get_done_flag = TRUE;
            break;

        case '\n':
            if (last_cr == TRUE) {
            	last_cr = FALSE;
            }
            else {
            	str_get_done_flag = TRUE;
            }
            break;

        case 0x1b:  // ESC-Key pressed
            str_get_done_flag = TRUE;
            break;

        default:
            if (p_owner->char_idx < p_owner->rx_buffer_size - 3) {
                while (!pifRingBuffer_PutByte(p_owner->p_tx_buffer, tmp_char)) {
                	if (!pifTaskManager_Yield()) break;
                }
                p_owner->p_rx_buffer[p_owner->char_idx] = tmp_char;
                p_owner->char_idx++;
            }
            break;
        }

        if (str_get_done_flag == TRUE) {
        	p_owner->p_rx_buffer[p_owner->char_idx] = 0;
        	while (!pifRingBuffer_PutByte(p_owner->p_tx_buffer, '\n')) {
        		if (!pifTaskManager_Yield()) break;
        	}
        }
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
    	for (int i = 0; i < 2; i++) {
    		p_cmd_entry = p_owner->p_cmd_table[i];
            while (p_cmd_entry->p_name) {
                if (!strcmp(p_owner->p_argv[0], p_cmd_entry->p_name)) {
                    return p_cmd_entry->processor(argc, p_owner->p_argv);
                }

                p_cmd_entry++;
            }
    	}
        return PIF_LOG_CMD_BAD_CMD;
    }
    return PIF_LOG_CMD_NO_ERROR;
}

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
	PifLog* p_owner = (PifLog*)p_client;

    if (p_owner->cmd_done == FALSE) {
        if (_getDebugString(p_owner, act_receive_data)) {
        	p_owner->cmd_done = TRUE;
        	pifTask_SetImmediate(p_owner->p_task);
        }
    }
}

static uint16_t _doTask(PifTask* p_task)
{
    int status = PIF_LOG_CMD_NO_ERROR;
    char msg[40];

    (void)p_task;

	if (s_log.cmd_done == TRUE) {
		status = _processDebugCmd(&s_log);

	    while (s_log.char_idx) {
	    	s_log.p_rx_buffer[s_log.char_idx] = 0;
	    	s_log.char_idx--;
	    }

	    for (int i = 0; i < PIF_LOG_CMD_MAX_ARGS; i++) {
	    	s_log.p_argv[i] = 0;
	    }

	    // Handle the case of bad command.
	    if (status == PIF_LOG_CMD_BAD_CMD) {
	    	while (!pifRingBuffer_PutString(s_log.p_tx_buffer, "Not defined command!\n")) {
	    		if (!pifTaskManager_Yield()) break;
	    	}
	    }

	    // Handle the case of too many arguments.
	    else if (status == PIF_LOG_CMD_TOO_MANY_ARGS) {
	    	while (!pifRingBuffer_PutString(s_log.p_tx_buffer, "Too many arguments for command!\n")) {
	    		if (!pifTaskManager_Yield()) break;
	    	}
	    }

	    // Handle the case of too few arguments.
	    else if (status == PIF_LOG_CMD_TOO_FEW_ARGS) {
	    	while (!pifRingBuffer_PutString(s_log.p_tx_buffer, "Too few arguments for command!\n")) {
	    		if (!pifTaskManager_Yield()) break;
	    	}
	    }

	    // Otherwise the command was executed.  Print the error
	    // code if one was returned.
	    else if (status != PIF_LOG_CMD_NO_ERROR) {
	    	pif_Printf(msg, "Command returned error code: %d\n", status);
	    	while (!pifRingBuffer_PutString(s_log.p_tx_buffer, msg)) {
	    		if (!pifTaskManager_Yield()) break;
	    	}
	    }

		s_log.cmd_done = FALSE;
	}

	while (!pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)s_log.p_prompt)) {
		if (!pifTaskManager_Yield()) break;
	}

	return 0;
}

#else

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
    (void)p_client;
    (void)act_receive_data;
}

#endif

static BOOL _evtSending(void* p_client, PifActCommSendData act_send_data)
{
	PifLog* p_owner = (PifLog*)p_client;
	uint16_t length;

	if (!pifRingBuffer_IsEmpty(p_owner->p_tx_buffer)) {
		length = (*act_send_data)(p_owner->p_comm, pifRingBuffer_GetTailPointer(p_owner->p_tx_buffer, 0),
    			pifRingBuffer_GetLinerSize(p_owner->p_tx_buffer, 0));
		pifRingBuffer_Remove(p_owner->p_tx_buffer, length);
		return TRUE;
	}
	return FALSE;
}

static void _printLog(char* p_string, BOOL vcd)
{
	if (!vcd && pifRingBuffer_IsBuffer(&s_log.buffer)) {
		pifRingBuffer_PutString(&s_log.buffer, p_string);
	}

	if (s_log.enable || vcd) {
        while (!pifRingBuffer_PutString(s_log.p_tx_buffer, p_string)) {
        	if (!pifTaskManager_Yield()) break;
        }
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

	_printLog(tmp_buf, FALSE);
}

BOOL pifLog_Init()
{
	memset(&s_log, 0, sizeof(PifLog));

	s_log.enable = TRUE;
#ifdef __PIF_LOG_COMMAND__
	s_log.p_task = pifTaskManager_Add(TM_PERIOD_MS, 1, _doTask, &s_log, FALSE);
	if (!s_log.p_task) return FALSE;
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
#ifdef __PIF_LOG_COMMAND__
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

#ifdef __PIF_LOG_COMMAND__

BOOL pifLog_UseCommand(const PifLogCmdEntry* p_cmd_table, const char* p_prompt)
{
    if (!p_cmd_table || !p_prompt) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    s_log.p_rx_buffer = calloc(sizeof(char), PIF_LOG_RX_BUFFER_SIZE);
    if (!s_log.p_rx_buffer) {
        pif_error = E_OUT_OF_HEAP;
		return FALSE;
    }
    s_log.rx_buffer_size = PIF_LOG_RX_BUFFER_SIZE;

    s_log.p_cmd_table[0] = c_cmd_table;
    s_log.p_cmd_table[1] = p_cmd_table;
    s_log.p_prompt = p_prompt;
    return TRUE;
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

void pifLog_Print(PifLogType type, const char* p_string)
{
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
    	tmp_buf[offset] = 0;
    	_printLog(tmp_buf, type == LT_VCD);
    }

	_printLog((char*)p_string, type == LT_VCD);
}

void pifLog_Printf(PifLogType type, const char* p_format, ...)
{
	va_list data;
	int offset = 0;
    char tmp_buf[PIF_LOG_LINE_SIZE + 1];

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
	pif_PrintFormat(tmp_buf + offset, &data, p_format);
	va_end(data);

	_printLog(tmp_buf, type == LT_VCD);
}

void pifLog_PrintInBuffer()
{
	uint16_t length;

	if (!s_log.p_comm->_p_task || !pifRingBuffer_IsBuffer(&s_log.buffer)) return;

	while (!pifRingBuffer_IsEmpty(&s_log.buffer)) {
		while (!pifRingBuffer_IsEmpty(s_log.p_tx_buffer)) {
			if (!pifTaskManager_Yield()) break;
		}
		length = pifRingBuffer_CopyAll(s_log.p_tx_buffer, &s_log.buffer, 0);
		pifRingBuffer_Remove(&s_log.buffer, length);
		pifComm_ForceSendData(s_log.p_comm);
	}
}

PifTask* pifLog_GetCommTask()
{
	return s_log.p_comm->_p_task;
}

BOOL pifLog_AttachComm(PifComm* p_comm)
{
    s_log.p_tx_buffer = pifRingBuffer_CreateHeap(PIF_ID_AUTO, PIF_LOG_TX_BUFFER_SIZE);
    if (!s_log.p_tx_buffer) return FALSE;

	s_log.p_comm = p_comm;
	pifComm_AttachClient(p_comm, &s_log, _evtParsing, _evtSending);
    return TRUE;
}

void pifLog_DetachComm()
{
	pifComm_DetachClient(s_log.p_comm);
	s_log.p_comm = NULL;

	pifRingBuffer_Destroy(&s_log.p_tx_buffer);
}

void pifLog_SendAndExit()
{
	while (pifRingBuffer_GetFillSize(s_log.p_tx_buffer)) {
		s_log.p_comm->_p_task->__evt_loop(s_log.p_comm->_p_task);
	}
}
