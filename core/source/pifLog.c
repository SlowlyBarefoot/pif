#include <stdarg.h>
#include <string.h>

#include "pifLog.h"


#define LOG_FLAG_COUNT	5


typedef struct _PIF_stLog
{
	BOOL bEnable;
	PIF_stRingBuffer *pstBuffer;
	PifComm *pstComm;
    PIF_stRingBuffer *pstTxBuffer;

#ifdef __PIF_LOG_COMMAND__
    char cLastChar;
    uint8_t ucCharIdx;
    BOOL bCmdDone;
    uint8_t ucRxBufferSize;
    char *pcRxBuffer;
	char *apcArgv[PIF_LOG_CMD_MAX_ARGS + 1];
	const PIF_stLogCmdEntry *pstCmdTable[2];
	const char *pcPrompt;
#endif
} PIF_stLog;


PIF_stLogFlag pif_stLogFlag = { .unAll = 0L };

static PIF_stLog s_stLog;

const struct {
	char *acName;
	char *acCommand;
} c_stLogFlags[LOG_FLAG_COUNT] = {
		{ "Performance", "pf" },
		{ "Task", "tk" },
		{ "Collect Signal", "cs" },
		{ "Duty Motor", "dm" },
		{ "Step Motor", "sm" }
};

#ifdef __PIF_LOG_COMMAND__

static int _CmdPrintVersion(int argc, char *argv[]);
static int _CmdSetStatus(int argc, char *argv[]);

const PIF_stLogCmdEntry c_pstCmdTable[] = {
	{ "ver", _CmdPrintVersion, "\nPrint Version" },
	{ "status", _CmdSetStatus, "\nSet Status" },

	{ NULL, NULL, NULL }
};


static int _CmdPrintVersion(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	pifLog_Printf(LT_enNone, "\nPIF Version: %d.%d.%d %s", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH, __DATE__);
	return PIF_LOG_CMD_NO_ERROR;
}

static int _CmdSetStatus(int argc, char *argv[])
{
	BOOL value;
	int i;

	if (argc == 1) {
		for (i = 0; i < LOG_FLAG_COUNT; i++) {
			pifLog_Printf(LT_enNone, "\n  %s(%s): %d", c_stLogFlags[i].acName, c_stLogFlags[i].acCommand, (pif_stLogFlag.unAll >> i) & 1);
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
			if (!strcmp(argv[1], c_stLogFlags[i].acCommand)) {
				if (value) {
					pif_stLogFlag.unAll |= 1L << i;
				}
				else {
					pif_stLogFlag.unAll &= ~(1L << i);
				}
				return PIF_LOG_CMD_NO_ERROR;
			}
		}
		return PIF_LOG_CMD_INVALID_ARG;
	}
	return PIF_LOG_CMD_TOO_FEW_ARGS;
}

static BOOL _GetDebugString(PIF_stLog *pstOwner, PifActCommReceiveData actReceiveData)
{
    BOOL bRes;
    char cTmpChar;
    BOOL bStrGetDoneFlag = FALSE;
    static BOOL bLastCr = FALSE;

	while ((*actReceiveData)(pstOwner->pstComm, (uint8_t *)&cTmpChar)) {
        bRes = 0;
        switch (cTmpChar) {
        case '\b':
            if (pstOwner->ucCharIdx > 0) {
                bRes = pifRingBuffer_PutString(pstOwner->pstTxBuffer, "\b \b");
                if (!bRes) return FALSE;
                pstOwner->ucCharIdx--;
                pstOwner->pcRxBuffer[pstOwner->ucCharIdx] = 0;
            }
            break;

        case '\r':
            bLastCr = TRUE;
            bStrGetDoneFlag = TRUE;
            break;

        case '\n':
            if (bLastCr == TRUE) {
                bLastCr = FALSE;
            }
            else {
                bStrGetDoneFlag = TRUE;
            }
            break;

        case 0x1b:  // ESC-Key pressed
            bRes = pifRingBuffer_PutByte(pstOwner->pstTxBuffer, '\n');
            if (!bRes) return FALSE;
            bStrGetDoneFlag = TRUE;
            break;

        default:
            if (pstOwner->ucCharIdx < pstOwner->ucRxBufferSize - 1) {
                bRes = pifRingBuffer_PutByte(pstOwner->pstTxBuffer, cTmpChar);
                if (!bRes) return FALSE;
                pstOwner->pcRxBuffer[pstOwner->ucCharIdx] = cTmpChar;
                pstOwner->ucCharIdx++;
            }
            break;
        }

        if (bStrGetDoneFlag == TRUE) {
        	pstOwner->pcRxBuffer[pstOwner->ucCharIdx] = 0;
        }
    }
    return bStrGetDoneFlag;
}

static int _ProcessDebugCmd(PIF_stLog *pstOwner)
{
    char *pcTmpCmd;
    BOOL bFindArg;
    unsigned int unArgc;
    const PIF_stLogCmdEntry *pstCmdEntry;

    bFindArg = TRUE;
    unArgc = 0;
    pcTmpCmd = pstOwner->pcRxBuffer;

    while (*pcTmpCmd) {
        if (*pcTmpCmd == ' ') {
            *pcTmpCmd = 0;
            bFindArg = TRUE;
        }
        else {
            if (bFindArg) {
                if (unArgc < PIF_LOG_CMD_MAX_ARGS) {
                	pstOwner->apcArgv[unArgc] = pcTmpCmd;
                    unArgc++;
                    bFindArg = FALSE;
                }
                else {
                    return PIF_LOG_CMD_TOO_MANY_ARGS;
                }
            }
        }

        pcTmpCmd++;
    }

    if (unArgc) {
    	for (int i = 0; i < 2; i++) {
            pstCmdEntry = pstOwner->pstCmdTable[i];
            while (pstCmdEntry->pcName) {
                if (!strcmp(pstOwner->apcArgv[0], pstCmdEntry->pcName)) {
                	pifRingBuffer_PutString(pstOwner->pstTxBuffer, (char *)pstCmdEntry->pcHelp);
                    return pstCmdEntry->fnProcessor(unArgc, pstOwner->apcArgv);
                }

                pstCmdEntry++;
            }
    	}
        return PIF_LOG_CMD_BAD_CMD;
    }
    return PIF_LOG_CMD_NO_ERROR;
}

static void _evtParsing(void *pvClient, PifActCommReceiveData actReceiveData)
{
	PIF_stLog *pstOwner = (PIF_stLog *)pvClient;

    if (pstOwner->bCmdDone == FALSE) {
        if (_GetDebugString(pstOwner, actReceiveData)) {
        	pstOwner->bCmdDone = TRUE;
        }
    }
}

#else

static void _evtParsing(void *pvClient, PifActCommReceiveData actReceiveData)
{
    (void)pvClient;
    (void)actReceiveData;
}

#endif

static BOOL _evtSending(void *pvClient, PifActCommSendData actSendData)
{
	PIF_stLog *pstOwner = (PIF_stLog *)pvClient;
	uint16_t usLength;

	if (!pifRingBuffer_IsEmpty(pstOwner->pstTxBuffer)) {
    	usLength = (*actSendData)(pstOwner->pstComm, pifRingBuffer_GetTailPointer(pstOwner->pstTxBuffer, 0),
    			pifRingBuffer_GetLinerSize(pstOwner->pstTxBuffer, 0));
		pifRingBuffer_Remove(pstOwner->pstTxBuffer, usLength);
		return TRUE;
	}
	return FALSE;
}

static void _PrintLog(char *pcString, BOOL bVcd)
{
	if (!bVcd && s_stLog.pstBuffer && pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) {
		pifRingBuffer_PutString(s_stLog.pstBuffer, pcString);
	}

	if (s_stLog.bEnable || bVcd) {
        if (!pifRingBuffer_PutString(s_stLog.pstTxBuffer, pcString)) {
        	pifTaskManager_YieldPeriod(s_stLog.pstComm->_p_task);
            pifRingBuffer_PutString(s_stLog.pstTxBuffer, pcString);
        }
        pifComm_ForceSendData(s_stLog.pstComm);
	}
}

static void _PrintTime()
{
	int nOffset = 0;
    static char acTmpBuf[20];

	acTmpBuf[nOffset++] = '\n';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_datetime.second, 2);
	acTmpBuf[nOffset++] = '.';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_timer1ms, 3);
	acTmpBuf[nOffset++] = ' ';
	acTmpBuf[nOffset++] = 'T';
	acTmpBuf[nOffset++] = ' ';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_datetime.hour, 2);
	acTmpBuf[nOffset++] = ':';
	nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_datetime.minute, 2);
	acTmpBuf[nOffset++] = ' ';

	_PrintLog(acTmpBuf, FALSE);
}

/**
 * @fn pifLog_Init
 * @brief Log 구조체 초기화한다.
 */
void pifLog_Init()
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = NULL;
}

/**
 * @fn pifLog_InitHeap
 * @brief Log 구조체 초기화한다.
 * @param usSize
 * @return
 */
BOOL pifLog_InitHeap(uint16_t usSize)
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, usSize);
	if (!s_stLog.pstBuffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifLog_InitStatic
 * @brief Log 구조체 초기화한다.
 * @param usSize
 * @param pcBuffer
 * @return
 */
BOOL pifLog_InitStatic(uint16_t usSize, uint8_t *pucBuffer)
{
	s_stLog.bEnable = TRUE;
	s_stLog.pstBuffer = pifRingBuffer_InitStatic(PIF_ID_AUTO, usSize, pucBuffer);
	if (!s_stLog.pstBuffer) return FALSE;
	return TRUE;
}

/**
 * @fn pifLog_Clear
 * @brief Log 구조체를 파기하다.
 */
void pifLog_Clear()
{
	if (s_stLog.pstBuffer) {
		pifRingBuffer_Exit(s_stLog.pstBuffer);
	}
#ifdef __PIF_LOG_COMMAND__
	if (s_stLog.pcRxBuffer) {
		free(s_stLog.pstBuffer);
		s_stLog.pstBuffer = NULL;
	}
#endif
}

#ifdef __PIF_LOG_COMMAND__

/**
 * @fn pifLog_UseCommad
 * @brief
 * @param pstCmdTable
 * @param pcPrompt
 * @return
 */
BOOL pifLog_UseCommand(const PIF_stLogCmdEntry *pstCmdTable, const char *pcPrompt)
{
    if (!pstCmdTable || !pcPrompt) {
    	pif_error = E_INVALID_PARAM;
		return FALSE;
    }

    s_stLog.pcRxBuffer = calloc(sizeof(char), PIF_LOG_RX_BUFFER_SIZE);
    if (!s_stLog.pcRxBuffer) {
        pif_error = E_OUT_OF_HEAP;
		return FALSE;
    }
    s_stLog.ucRxBufferSize = PIF_LOG_RX_BUFFER_SIZE;

    s_stLog.pstCmdTable[0] = c_pstCmdTable;
    s_stLog.pstCmdTable[1] = pstCmdTable;
    s_stLog.pcPrompt = pcPrompt;
    return TRUE;
}

#endif

/**
 * @fn pifLog_Enable
 * @brief
 */
void pifLog_Enable()
{
	s_stLog.bEnable = TRUE;
}

/**
 * @fn pifLog_Disable
 * @brief
 */
void pifLog_Disable()
{
	s_stLog.bEnable = FALSE;
}

/**
 * @fn pifLog_IsEmpty
 * @brief
 * @return
 */
BOOL pifLog_IsEmpty()
{
	return pifRingBuffer_IsEmpty(s_stLog.pstBuffer);
}

/**
 * @fn pifLog_Printf
 * @brief
 * @param enType
 * @param pcFormat
 */
void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...)
{
	va_list data;
	int nOffset = 0;
    static char acTmpBuf[PIF_LOG_LINE_SIZE];
    static uint8_t nMinute = 255;
    const char cType[] = { 'I', 'W', 'E', 'C' };

    if (enType >= LT_enInfo) {
        if (nMinute != pif_datetime.minute) {
        	_PrintTime();
        	nMinute = pif_datetime.minute;
    	}

    	acTmpBuf[nOffset++] = '\n';
		nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_datetime.second, 2);
    	acTmpBuf[nOffset++] = '.';
		nOffset += pif_DecToString(acTmpBuf + nOffset, (uint32_t)pif_timer1ms, 3);
    	acTmpBuf[nOffset++] = ' ';
    	acTmpBuf[nOffset++] = cType[enType - LT_enInfo];
    	acTmpBuf[nOffset++] = ' ';
    }

	va_start(data, pcFormat);
	pif_PrintFormat(acTmpBuf + nOffset, &data, pcFormat);
	va_end(data);

	_PrintLog(acTmpBuf, enType == LT_enVcd);
}

/**
 * @fn pifLog_PrintInBuffer
 * @brief
 */
void pifLog_PrintInBuffer()
{
	uint16_t usLength;

	if (!s_stLog.pstComm->_p_task || !pifRingBuffer_IsBuffer(s_stLog.pstBuffer)) return;

	while (!pifRingBuffer_IsEmpty(s_stLog.pstBuffer)) {
		while (!pifRingBuffer_IsEmpty(s_stLog.pstTxBuffer)) {
			pifTaskManager_YieldPeriod(s_stLog.pstComm->_p_task);
		}
		usLength = pifRingBuffer_CopyAll(s_stLog.pstTxBuffer, s_stLog.pstBuffer, 0);
		pifRingBuffer_Remove(s_stLog.pstBuffer, usLength);
		pifComm_ForceSendData(s_stLog.pstComm);
	}
}

/**
 * @fn pifLog_AttachComm
 * @brief
 * @param pstComm
 * @return
 */
PifTask *pifLog_GetCommTask()
{
	return s_stLog.pstComm->_p_task;
}

/**
 * @fn pifLog_AttachComm
 * @brief
 * @param pstComm
 * @return
 */
BOOL pifLog_AttachComm(PifComm *pstComm)
{
    s_stLog.pstTxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_LOG_TX_BUFFER_SIZE);
    if (!s_stLog.pstTxBuffer) return FALSE;

	s_stLog.pstComm = pstComm;
	pifComm_AttachClient(pstComm, &s_stLog);
	pstComm->evt_parsing = _evtParsing;
	pstComm->evt_sending = _evtSending;
    return TRUE;
}

#ifdef __PIF_LOG_COMMAND__

static uint16_t _DoTask(PifTask *pstTask)
{
    int nStatus = PIF_LOG_CMD_NO_ERROR;

    (void)pstTask;

	if (s_stLog.bCmdDone == TRUE) {
	    nStatus = _ProcessDebugCmd(&s_stLog);

	    while (s_stLog.ucCharIdx) {
	    	s_stLog.pcRxBuffer[s_stLog.ucCharIdx] = 0;
	    	s_stLog.ucCharIdx--;
	    }

	    for (int i = 0; i < PIF_LOG_CMD_MAX_ARGS; i++) {
	    	s_stLog.apcArgv[i] = 0;
	    }

	    // Handle the case of bad command.
	    if (nStatus == PIF_LOG_CMD_BAD_CMD) {
	    	pifRingBuffer_PutString(s_stLog.pstTxBuffer, "\nNot defined command!");
	    }

	    // Handle the case of too many arguments.
	    else if (nStatus == PIF_LOG_CMD_TOO_MANY_ARGS) {
	    	pifRingBuffer_PutString(s_stLog.pstTxBuffer, "\nToo many arguments for command!");
	    }

	    // Handle the case of too few arguments.
	    else if (nStatus == PIF_LOG_CMD_TOO_FEW_ARGS) {
	    	pifRingBuffer_PutString(s_stLog.pstTxBuffer, "\nToo few arguments for command!");
	    }

	    // Otherwise the command was executed.  Print the error
	    // code if one was returned.
	    else if (nStatus != PIF_LOG_CMD_NO_ERROR) {
	    	pifRingBuffer_PutString(s_stLog.pstTxBuffer, "\nCommand returned error code");
	    }

		pifRingBuffer_PutString(s_stLog.pstTxBuffer, (char *)s_stLog.pcPrompt);
		pifRingBuffer_PutString(s_stLog.pstTxBuffer, "> ");

		s_stLog.bCmdDone = FALSE;
	}

	return 0;
}

/**
 * @fn pifLog_AttachTask
 * @brief Task를 추가한다.
 * @param enMode Task의 Mode를 설정한다.
 * @param usPeriod Mode에 따라 주기의 단위가 변경된다.
 * @param bStart 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask *pifLog_AttachTask(PifTaskMode enMode, uint16_t usPeriod, BOOL bStart)
{
	return pifTaskManager_Add(enMode, usPeriod, _DoTask, &s_stLog, bStart);
}

#endif
