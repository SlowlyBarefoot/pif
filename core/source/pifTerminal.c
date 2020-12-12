#include <stdarg.h>
#include <string.h>

#include "pifLog.h"
#include "pifTerminal.h"


typedef struct _PIF_stTerminalBase
{
    PIF_stRingBuffer stTxBuffer;
    uint8_t ucCharIdx;
    uint8_t ucRxBufferSize;
    char *pcRxBuffer;
    char cLastChar;
	char *apcArgv[PIF_TERM_CMD_MAX_ARGS + 1];
	const PIF_stTermCmdEntry *pstCmdTable;
	const char *pcPrompt;
} PIF_stTerminalBase;


static PIF_stTerminalBase s_stTerminalBase;


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
                bRes = pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "\b \b");
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
            bRes = pifRingBuffer_PutByte(&s_stTerminalBase.stTxBuffer, '\n');
            if (!bRes) return FALSE;
            bStrGetDoneFlag = TRUE;
            break;

        default:
            if (s_stTerminalBase.ucCharIdx < s_stTerminalBase.ucRxBufferSize - 1) {
                bRes = pifRingBuffer_PutByte(&s_stTerminalBase.stTxBuffer, cTmpChar);
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
            	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, (char *)pstCmdEntry->pcHelp);
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
        	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "Not defined command!\n");
        }

        // Handle the case of too many arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_MANY_ARGS) {
        	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "Too many arguments for command processor!\n");
        }

        // Handle the case of too few arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_FEW_ARGS) {
        	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "Too few arguments for command processor!\n");
        }

        // Otherwise the command was executed.  Print the error
        // code if one was returned.
        else if (nStatus != PIF_TERM_CMD_NO_ERROR) {
        	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "Command returned error code\n");
        }

    	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, (char *)pstBase->pcPrompt);
    	pifRingBuffer_PutString(&s_stTerminalBase.stTxBuffer, "> ");
    }
}

static BOOL _evtSending(void *pvClient, PIF_stRingBuffer *pstBuffer)
{
	PIF_stTerminalBase *pstBase = (PIF_stTerminalBase *)pvClient;
	uint16_t usLength;

	if (!pifRingBuffer_IsEmpty(&pstBase->stTxBuffer)) {
		usLength = pifRingBuffer_CopyAll(pstBuffer, &pstBase->stTxBuffer, 0);
		pifRingBuffer_Remove(&pstBase->stTxBuffer, usLength);
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

    if (!pifRingBuffer_InitAlloc(&s_stTerminalBase.stTxBuffer, PIF_TERMINAL_TX_BUFFER_SIZE)) goto fail;

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

	pifRingBuffer_Exit(&s_stTerminalBase.stTxBuffer);
    if (!pifRingBuffer_InitAlloc(&s_stTerminalBase.stTxBuffer, usTxSize)) goto fail;
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
	 return &s_stTerminalBase.stTxBuffer;
}

/**
 * @fn pifTerminal_AttachComm
 * @brief
 * @param pstComm
 */
void pifTerminal_AttachComm(PIF_stComm *pstComm)
{
	pifComm_AttachClient(pstComm, &s_stTerminalBase);
	pifComm_AttachEvtParsing(pstComm, _evtParsing);
	pifComm_AttachEvtSending(pstComm, _evtSending);
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

	if (argc == 1) {
		pifLog_Printf(LT_enNone, "\n  Performance: %d", pif_stLogFlag.btPerformance);
		pifLog_Printf(LT_enNone, "\n  Task: %d", pif_stLogFlag.btTask);
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
		if (!strcmp(argv[1], "perform")) {
			pif_stLogFlag.btPerformance = value;
			return PIF_TERM_CMD_NO_ERROR;
		}
		else if (!strcmp(argv[1], "task")) {
			pif_stLogFlag.btTask = value;
			return PIF_TERM_CMD_NO_ERROR;
		}
		return PIF_TERM_CMD_INVALID_ARG;
	}
	return PIF_TERM_CMD_TOO_FEW_ARGS;
}
