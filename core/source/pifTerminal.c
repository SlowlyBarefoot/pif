#include <stdarg.h>
#include <string.h>

#include "pifLog.h"
#include "pifTerminal.h"


typedef struct _PIF_stTerminalBase
{
    PIF_stRingBuffer *pstTxBuffer;
    uint8_t ucCharIdx;
    uint8_t ucRxBufferSize;
    char *pcRxBuffer;
    char cLastChar;
	char *apcArgv[PIF_TERM_CMD_MAX_ARGS + 1];
	const PIF_stTermCmdEntry *pstCmdTable;
	const char *pcPrompt;
} PIF_stTerminalBase;


static PIF_stTerminalBase s_stTerminalBase;


#define LOG_FLAG_COUNT	4

const struct {
	char *acName;
	char *acCommand;
} c_stLogFlags[LOG_FLAG_COUNT] = {
		{ "Performance", "perf" },
		{ "Task", "task" },
		{ "Duty Motor", "dmt" },
		{ "Step Motor", "smt" }
};

static BOOL _GetDebugString(PIF_stRingBuffer *pstBuffer)
{
    BOOL bRes;
    char cTmpChar;
    BOOL bStrGetDoneFlag = FALSE;
    static BOOL bLastCr = FALSE;

    if (s_stTerminalBase.pcRxBuffer == NULL) return FALSE;

    if (pifRingBuffer_GetByte(pstBuffer, (uint8_t *)&cTmpChar)) {
        bRes = 0;
        switch (cTmpChar) {
        case '\b':
            if (s_stTerminalBase.ucCharIdx > 0) {
                bRes = pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "\b \b");
                if (!bRes) return FALSE;
                s_stTerminalBase.ucCharIdx--;
                s_stTerminalBase.pcRxBuffer[s_stTerminalBase.ucCharIdx] = 0;
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
            bRes = pifRingBuffer_PutByte(s_stTerminalBase.pstTxBuffer, '\n');
            if (!bRes) return FALSE;
            bStrGetDoneFlag = TRUE;
            break;

        default:
            if (s_stTerminalBase.ucCharIdx < s_stTerminalBase.ucRxBufferSize - 1) {
                bRes = pifRingBuffer_PutByte(s_stTerminalBase.pstTxBuffer, cTmpChar);
                if (!bRes) return FALSE;
                s_stTerminalBase.pcRxBuffer[s_stTerminalBase.ucCharIdx] = cTmpChar;
                s_stTerminalBase.ucCharIdx++;
            }
            break;
        }

        if (bStrGetDoneFlag == TRUE) {
        	s_stTerminalBase.pcRxBuffer[s_stTerminalBase.ucCharIdx] = 0;
        }
    }
    return bStrGetDoneFlag;
}

static int _ProcessDebugCmd(char *pcCmdStr)
{
    char *pcTmpCmd;
    BOOL bFindArg;
    unsigned int unArgc;
    const PIF_stTermCmdEntry *pstCmdEntry;

    bFindArg = TRUE;
    unArgc = 0;
    pcTmpCmd = pcCmdStr;

    while (*pcTmpCmd) {
        if (*pcTmpCmd == ' ') {
            *pcTmpCmd = 0;
            bFindArg = TRUE;
        }
        else {
            if (bFindArg) {
                if (unArgc < PIF_TERM_CMD_MAX_ARGS) {
                	s_stTerminalBase.apcArgv[unArgc] = pcTmpCmd;
                    unArgc++;
                    bFindArg = FALSE;
                }
                else {
                    return PIF_TERM_CMD_TOO_MANY_ARGS;
                }
            }
        }

        pcTmpCmd++;
    }

    if (unArgc) {
        pstCmdEntry = &s_stTerminalBase.pstCmdTable[0];
        while (pstCmdEntry->pcName) {
            if (!strcmp(s_stTerminalBase.apcArgv[0], pstCmdEntry->pcName)) {
            	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, (char *)pstCmdEntry->pcHelp);
                return pstCmdEntry->fnProcessor(unArgc, s_stTerminalBase.apcArgv);
            }

            pstCmdEntry++;
        }
        return PIF_TERM_CMD_BAD_CMD;
    }

    return PIF_TERM_CMD_NO_ERROR;
}

static void _ClearDebugStr()
{
    while (s_stTerminalBase.ucCharIdx) {
    	s_stTerminalBase.pcRxBuffer[s_stTerminalBase.ucCharIdx] = 0;
    	s_stTerminalBase.ucCharIdx--;
    }

    for (int i = 0; i < PIF_TERM_CMD_MAX_ARGS; i++) {
    	s_stTerminalBase.apcArgv[i] = 0;
    }
}

static void _evtParsing(void *pvClient, PIF_stRingBuffer *pstBuffer)
{
	PIF_stTerminalBase *pstBase = (PIF_stTerminalBase *)pvClient;
    int nStatus = PIF_TERM_CMD_NO_ERROR;

    if (_GetDebugString(pstBuffer)) {
        nStatus = _ProcessDebugCmd(pstBase->pcRxBuffer);

        _ClearDebugStr();

        // Handle the case of bad command.
        if (nStatus == PIF_TERM_CMD_BAD_CMD) {
        	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "\nNot defined command!");
        }

        // Handle the case of too many arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_MANY_ARGS) {
        	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "\nToo many arguments for command!");
        }

        // Handle the case of too few arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_FEW_ARGS) {
        	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "\nToo few arguments for command!");
        }

        // Otherwise the command was executed.  Print the error
        // code if one was returned.
        else if (nStatus != PIF_TERM_CMD_NO_ERROR) {
        	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "\nCommand returned error code");
        }

    	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, (char *)pstBase->pcPrompt);
    	pifRingBuffer_PutString(s_stTerminalBase.pstTxBuffer, "> ");
    }
}

static BOOL _evtSending(void *pvClient, PIF_stRingBuffer *pstBuffer)
{
	PIF_stTerminalBase *pstBase = (PIF_stTerminalBase *)pvClient;
	uint16_t usLength;

	if (!pifRingBuffer_IsEmpty(pstBase->pstTxBuffer)) {
		usLength = pifRingBuffer_CopyAll(pstBuffer, pstBase->pstTxBuffer, 0);
		pifRingBuffer_Remove(pstBase->pstTxBuffer, usLength);
		return TRUE;
	}
	return FALSE;
}

/**
 * @fn pifTerminal_Init
 * @brief
 * @param pstCmdTable
 * @param pcPrompt
 * @return
 */
BOOL pifTerminal_Init(const PIF_stTermCmdEntry *pstCmdTable, const char *pcPrompt)
{
    if (!pstCmdTable || !pcPrompt) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    s_stTerminalBase.pcRxBuffer = calloc(sizeof(char), PIF_TERMINAL_RX_BUFFER_SIZE);
    if (!s_stTerminalBase.pcRxBuffer) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }
    s_stTerminalBase.ucRxBufferSize = PIF_TERMINAL_RX_BUFFER_SIZE;

    s_stTerminalBase.pstTxBuffer = pifRingBuffer_InitHeap(PIF_ID_AUTO, PIF_TERMINAL_TX_BUFFER_SIZE);
    if (!s_stTerminalBase.pstTxBuffer) goto fail;

    s_stTerminalBase.pstCmdTable = pstCmdTable;
    s_stTerminalBase.pcPrompt = pcPrompt;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "%u Terminal:Init(R:%u T:%u) EC:%d", PIF_TERMINAL_RX_BUFFER_SIZE, PIF_TERMINAL_TX_BUFFER_SIZE, pif_enError);
	return FALSE;
}

/**
 * @fn pifTerminal_ResizeRxBuffer
 * @brief
 * @param usRxSize
 * @return
 */
BOOL pifTerminal_ResizeRxBuffer(uint16_t usRxSize)
{
    if (!usRxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	if (s_stTerminalBase.pcRxBuffer) {
		free(s_stTerminalBase.pcRxBuffer);
		s_stTerminalBase.ucRxBufferSize = 0;
	}

	s_stTerminalBase.pcRxBuffer = calloc(sizeof(char), usRxSize);
    if (!s_stTerminalBase.pcRxBuffer) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }
    s_stTerminalBase.ucRxBufferSize = usRxSize;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Terminal:ResizeRxBuffer(S:%u) EC:%d", usRxSize, pif_enError);
	return FALSE;
}

/**
 * @fn pifTerminal_ResizeTxBuffer
 * @brief
 * @param usTxSize
 * @return
 */
BOOL pifTerminal_ResizeTxBuffer(uint16_t usTxSize)
{
    if (!usTxSize) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

    if (!pifRingBuffer_ResizeHeap(s_stTerminalBase.pstTxBuffer, usTxSize)) goto fail;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Terminal:ResizeTxBuffer(S:%u) EC:%d", usTxSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifTerminal_GetTxBuffer
 * @brief
 * @return
 */
PIF_stRingBuffer *pifTerminal_GetTxBuffer()
{
	 return s_stTerminalBase.pstTxBuffer;
}

/**
 * @fn pifTerminal_AttachComm
 * @brief
 * @param pstComm
 */
void pifTerminal_AttachComm(PIF_stComm *pstComm)
{
	pifComm_AttachClient(pstComm, &s_stTerminalBase);
	pifComm_AttachEvent(pstComm, _evtParsing, _evtSending, NULL);
}

/**
 * @fn pifTerminal_PrintVersion
 * @brief
 * @param argc
 * @param argv
 * @return
 */
int pifTerminal_PrintVersion(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	pifLog_Printf(LT_enNone, "\nPIF Version: %d.%d.%d %s", PIF_VERSION_MAJOR, PIF_VERSION_MINOR, PIF_VERSION_PATCH, __DATE__);
	return PIF_TERM_CMD_NO_ERROR;
}

/**
 * @fn pifTerminal_SetStatus
 * @brief
 * @param argc
 * @param argv
 * @return
 */
int pifTerminal_SetStatus(int argc, char *argv[])
{
	BOOL value;
	int i;

	if (argc == 1) {
		for (i = 0; i < LOG_FLAG_COUNT; i++) {
			pifLog_Printf(LT_enNone, "\n  %s(%s): %d", c_stLogFlags[i].acName, c_stLogFlags[i].acCommand, (pif_stLogFlag.unFlags >> i) & 1);
		}
		return PIF_TERM_CMD_NO_ERROR;
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
			return PIF_TERM_CMD_INVALID_ARG;
		}
		for (i = 0; i < LOG_FLAG_COUNT; i++) {
			if (!strcmp(argv[1], c_stLogFlags[i].acCommand)) {
				if (value) {
					pif_stLogFlag.unFlags |= 1L << i;
				}
				else {
					pif_stLogFlag.unFlags &= ~(1L << i);
				}
				return PIF_TERM_CMD_NO_ERROR;
			}
		}
		return PIF_TERM_CMD_INVALID_ARG;
	}
	return PIF_TERM_CMD_TOO_FEW_ARGS;
}
