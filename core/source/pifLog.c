#include <stdarg.h>
#include <string.h>

#include "pifLog.h"


#define LOG_FLAG_COUNT	5


typedef struct StPifLog
{
	BOOL enable;
	PifRingBuffer* p_buffer;
	PifComm* p_comm;
    PifRingBuffer* p_tx_buffer;

#ifdef __PIF_LOG_COMMAND__
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

const struct {
	char* p_name;
	char* p_command;
} c_log_flags[LOG_FLAG_COUNT] = {
		{ "Performance", "pf" },
		{ "Task", "tk" },
		{ "Collect Signal", "cs" },
		{ "Duty Motor", "dm" },
		{ "Step Motor", "sm" }
};

#ifdef __PIF_LOG_COMMAND__

static int _cmdPrintVersion(int argc, char* argv[]);
static int _cmdSetStatus(int argc, char* argv[]);

const PifLogCmdEntry c_cmd_table[] = {
	{ "ver", _cmdPrintVersion, "\nPrint Version" },
	{ "status", _cmdSetStatus, "\nSet Status" },

	{ NULL, NULL, NULL }
};


static int _cmdPrintVersion(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	pifLog_Printf(LT_NONE, "\nPIF Version: %d.%d.%d %s", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH, __DATE__);
	return PIF_LOG_CMD_NO_ERROR;
}

static int _cmdSetStatus(int argc, char* argv[])
{
	BOOL value;
	int i;

	if (argc == 1) {
		for (i = 0; i < LOG_FLAG_COUNT; i++) {
			pifLog_Printf(LT_NONE, "\n  %s(%s): %d", c_log_flags[i].p_name, c_log_flags[i].p_command, (pif_log_flag.all >> i) & 1);
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
		for (i = 0; i < LOG_FLAG_COUNT; i++) {
			if (!strcmp(argv[1], c_log_flags[i].p_command)) {
				if (value) {
					pif_log_flag.all |= 1L << i;
				}
				else {
					pif_log_flag.all &= ~(1L << i);
				}
				return PIF_LOG_CMD_NO_ERROR;
			}
		}
		return PIF_LOG_CMD_INVALID_ARG;
	}
	return PIF_LOG_CMD_TOO_FEW_ARGS;
}

static BOOL _GetDebugString(PifLog* p_owner, PifActCommReceiveData act_receive_data)
{
    BOOL res;
    char tmp_char;
    BOOL str_get_done_flag = FALSE;
    static BOOL last_cr = FALSE;

	while ((*act_receive_data)(p_owner->p_comm, (uint8_t*)&tmp_char)) {
		res = 0;
        switch (tmp_char) {
        case '\b':
            if (p_owner->char_idx > 0) {
            	res = pifRingBuffer_PutString(p_owner->p_tx_buffer, "\b \b");
                if (!res) return FALSE;
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
        	res = pifRingBuffer_PutByte(p_owner->p_tx_buffer, '\n');
            if (!res) return FALSE;
            str_get_done_flag = TRUE;
            break;

        default:
            if (p_owner->char_idx < p_owner->rx_buffer_size - 1) {
            	res = pifRingBuffer_PutByte(p_owner->p_tx_buffer, tmp_char);
                if (!res) return FALSE;
                p_owner->p_rx_buffer[p_owner->char_idx] = tmp_char;
                p_owner->char_idx++;
            }
            break;
        }

        if (str_get_done_flag == TRUE) {
        	p_owner->p_rx_buffer[p_owner->char_idx] = 0;
        }
    }
    return str_get_done_flag;
}

static int _ProcessDebugCmd(PifLog* p_owner)
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
                	pifRingBuffer_PutString(p_owner->p_tx_buffer, (char *)p_cmd_entry->p_help);
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
        if (_GetDebugString(p_owner, act_receive_data)) {
        	p_owner->cmd_done = TRUE;
        }
    }
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

static void _PrintLog(char* p_string, BOOL vcd)
{
	if (!vcd && s_log.p_buffer && pifRingBuffer_IsBuffer(s_log.p_buffer)) {
		pifRingBuffer_PutString(s_log.p_buffer, p_string);
	}

	if (s_log.enable || vcd) {
        if (!pifRingBuffer_PutString(s_log.p_tx_buffer, p_string)) {
        	pifTaskManager_YieldPeriod(s_log.p_comm->_p_task);
            pifRingBuffer_PutString(s_log.p_tx_buffer, p_string);
        }
        pifComm_ForceSendData(s_log.p_comm);
	}
}

static void _PrintTime()
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

	_PrintLog(tmp_buf, FALSE);
}

/**
 * @fn pifLog_Init
 * @brief Log 구조체 초기화한다.
 */
void pifLog_Init()
{
	s_log.enable = TRUE;
	s_log.p_buffer = NULL;
	s_log.p_tx_buffer = NULL;
#ifdef __PIF_LOG_COMMAND__
	s_log.p_rx_buffer = NULL;
#endif
}

/**
 * @fn pifLog_InitHeap
 * @brief Log 구조체 초기화한다.
 * @param size
 * @return
 */
BOOL pifLog_InitHeap(uint16_t size)
{
	s_log.enable = TRUE;
	s_log.p_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, size);
	if (!s_log.p_buffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifLog_InitStatic
 * @brief Log 구조체 초기화한다.
 * @param size
 * @param p_buffer
 * @return
 */
BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer)
{
	s_log.enable = TRUE;
	s_log.p_buffer = pifRingBuffer_InitStatic(PIF_ID_AUTO, size, p_buffer);
	if (!s_log.p_buffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifLog_Clear
 * @brief Log 구조체를 파기하다.
 */
void pifLog_Clear()
{
	if (s_log.p_buffer) {
		pifRingBuffer_Exit(s_log.p_buffer);
		s_log.p_buffer = NULL;
	}
	if (s_log.p_tx_buffer) {
		pifRingBuffer_Exit(s_log.p_tx_buffer);
		s_log.p_tx_buffer = NULL;
	}
#ifdef __PIF_LOG_COMMAND__
	if (s_log.p_rx_buffer) {
		free(s_log.p_rx_buffer);
		s_log.p_rx_buffer = NULL;
	}
#endif
}

#ifdef __PIF_LOG_COMMAND__

/**
 * @fn pifLog_UseCommad
 * @brief
 * @param p_cmd_table
 * @param p_prompt
 * @return
 */
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

/**
 * @fn pifLog_Enable
 * @brief
 */
void pifLog_Enable()
{
	s_log.enable = TRUE;
}

/**
 * @fn pifLog_Disable
 * @brief
 */
void pifLog_Disable()
{
	s_log.enable = FALSE;
}

/**
 * @fn pifLog_IsEmpty
 * @brief
 * @return
 */
BOOL pifLog_IsEmpty()
{
	return pifRingBuffer_IsEmpty(s_log.p_buffer);
}

/**
 * @fn pifLog_Printf
 * @brief
 * @param type
 * @param p_format
 */
void pifLog_Printf(PifLogType type, const char* p_format, ...)
{
	va_list data;
	int offset = 0;
    static char tmp_buf[PIF_LOG_LINE_SIZE];
    static uint8_t minute = 255;
    const char type_ch[] = { 'I', 'W', 'E', 'C' };

    if (type >= LT_INFO) {
        if (minute != pif_datetime.minute) {
        	_PrintTime();
        	minute = pif_datetime.minute;
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

	_PrintLog(tmp_buf, type == LT_VCD);
}

/**
 * @fn pifLog_PrintInBuffer
 * @brief
 */
void pifLog_PrintInBuffer()
{
	uint16_t length;

	if (!s_log.p_comm->_p_task || !pifRingBuffer_IsBuffer(s_log.p_buffer)) return;

	while (!pifRingBuffer_IsEmpty(s_log.p_buffer)) {
		while (!pifRingBuffer_IsEmpty(s_log.p_tx_buffer)) {
			pifTaskManager_YieldPeriod(s_log.p_comm->_p_task);
		}
		length = pifRingBuffer_CopyAll(s_log.p_tx_buffer, s_log.p_buffer, 0);
		pifRingBuffer_Remove(s_log.p_buffer, length);
		pifComm_ForceSendData(s_log.p_comm);
	}
}

/**
 * @fn pifLog_GetCommTask
 * @brief
 * @return
 */
PifTask* pifLog_GetCommTask()
{
	return s_log.p_comm->_p_task;
}

/**
 * @fn pifLog_AttachComm
 * @brief
 * @param p_comm
 * @return
 */
BOOL pifLog_AttachComm(PifComm* p_comm)
{
    s_log.p_tx_buffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_LOG_TX_BUFFER_SIZE);
    if (!s_log.p_tx_buffer) return FALSE;

	s_log.p_comm = p_comm;
	pifComm_AttachClient(p_comm, &s_log);
	p_comm->evt_parsing = _evtParsing;
	p_comm->evt_sending = _evtSending;
    return TRUE;
}

#ifdef __PIF_LOG_COMMAND__

static uint16_t _DoTask(PifTask* p_task)
{
    int status = PIF_LOG_CMD_NO_ERROR;

    (void)p_task;

	if (s_log.cmd_done == TRUE) {
		status = _ProcessDebugCmd(&s_log);

	    while (s_log.char_idx) {
	    	s_log.p_rx_buffer[s_log.char_idx] = 0;
	    	s_log.char_idx--;
	    }

	    for (int i = 0; i < PIF_LOG_CMD_MAX_ARGS; i++) {
	    	s_log.p_argv[i] = 0;
	    }

	    // Handle the case of bad command.
	    if (status == PIF_LOG_CMD_BAD_CMD) {
	    	pifRingBuffer_PutString(s_log.p_tx_buffer, "\nNot defined command!");
	    }

	    // Handle the case of too many arguments.
	    else if (status == PIF_LOG_CMD_TOO_MANY_ARGS) {
	    	pifRingBuffer_PutString(s_log.p_tx_buffer, "\nToo many arguments for command!");
	    }

	    // Handle the case of too few arguments.
	    else if (status == PIF_LOG_CMD_TOO_FEW_ARGS) {
	    	pifRingBuffer_PutString(s_log.p_tx_buffer, "\nToo few arguments for command!");
	    }

	    // Otherwise the command was executed.  Print the error
	    // code if one was returned.
	    else if (status != PIF_LOG_CMD_NO_ERROR) {
	    	pifRingBuffer_PutString(s_log.p_tx_buffer, "\nCommand returned error code");
	    }

		pifRingBuffer_PutString(s_log.p_tx_buffer, (char *)s_log.p_prompt);
		pifRingBuffer_PutString(s_log.p_tx_buffer, "> ");

		s_log.cmd_done = FALSE;
	}

	return 0;
}

/**
 * @fn pifLog_AttachTask
 * @brief Task를 추가한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifLog_AttachTask(PifTaskMode mode, uint16_t period, BOOL start)
{
	return pifTaskManager_Add(mode, period, _DoTask, &s_log, start);
}

#endif
