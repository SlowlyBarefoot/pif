#include <stdarg.h>
#include <string.h>

#include "pifLog.h"
#include "pifTerminal.h"


static PIF_stTerminal s_stTerminal;


static BOOL _GetDebugString(PIF_stRingBuffer *pstBuffer)
{
    BOOL bRes;
    char cTmpChar;
    BOOL bStrGetDoneFlag = FALSE;
    static BOOL bLastCr = FALSE;

    if (s_stTerminal.__pcRxBuffer == NULL) return FALSE;

    if (pifRingBuffer_Pop(pstBuffer, (uint8_t *)&cTmpChar)) {
        bRes = 0;
        switch (cTmpChar) {
        case '\b':
            if (s_stTerminal.__ucCharIdx > 0) {
                bRes = pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "\b \b");
                if (!bRes) return FALSE;
                s_stTerminal.__ucCharIdx--;
                s_stTerminal.__pcRxBuffer[s_stTerminal.__ucCharIdx] = 0;
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
            bRes = pifRingBuffer_PushByte(&s_stTerminal.stTxBuffer, '\n');
            if (!bRes) return FALSE;
            bStrGetDoneFlag = TRUE;
            break;

        default:
            if (s_stTerminal.__ucCharIdx < s_stTerminal.__ucRxBufferSize - 1) {
                bRes = pifRingBuffer_PushByte(&s_stTerminal.stTxBuffer, cTmpChar);
                if (!bRes) return FALSE;
                s_stTerminal.__pcRxBuffer[s_stTerminal.__ucCharIdx] = cTmpChar;
                s_stTerminal.__ucCharIdx++;
            }
            break;
        }

        if (bStrGetDoneFlag == TRUE) {
        	s_stTerminal.__pcRxBuffer[s_stTerminal.__ucCharIdx] = 0;
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
                	s_stTerminal.__apcArgv[unArgc] = pcTmpCmd;
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
        pstCmdEntry = &s_stTerminal.__pstCmdTable[0];
        while (pstCmdEntry->pcName) {
            if (!strcmp(s_stTerminal.__apcArgv[0], pstCmdEntry->pcName)) {
            	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, (char *)pstCmdEntry->pcHelp);
                return pstCmdEntry->fnProcessor(unArgc, s_stTerminal.__apcArgv);
            }

            pstCmdEntry++;
        }
        return PIF_TERM_CMD_BAD_CMD;
    }

    return PIF_TERM_CMD_NO_ERROR;
}

static void _ClearDebugStr()
{
    while (s_stTerminal.__ucCharIdx) {
    	s_stTerminal.__pcRxBuffer[s_stTerminal.__ucCharIdx] = 0;
    	s_stTerminal.__ucCharIdx--;
    }

    for (int i = 0; i < PIF_TERM_CMD_MAX_ARGS; i++) {
    	s_stTerminal.__apcArgv[i] = 0;
    }
}

static void _Parsing(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stTerminal *pstOwner = (PIF_stTerminal *)pvOwner;
    int nStatus = PIF_TERM_CMD_NO_ERROR;

    if (_GetDebugString(pstBuffer)) {
        nStatus = _ProcessDebugCmd(pstOwner->__pcRxBuffer);

        _ClearDebugStr();

        // Handle the case of bad command.
        if (nStatus == PIF_TERM_CMD_BAD_CMD) {
        	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "Not defined command!\n");
        }

        // Handle the case of too many arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_MANY_ARGS) {
        	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "Too many arguments for command processor!\n");
        }

        // Handle the case of too few arguments.
        else if (nStatus == PIF_TERM_CMD_TOO_FEW_ARGS) {
        	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "Too few arguments for command processor!\n");
        }

        // Otherwise the command was executed.  Print the error
        // code if one was returned.
        else if (nStatus != PIF_TERM_CMD_NO_ERROR) {
        	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "Command returned error code\n");
        }

    	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, (char *)pstOwner->__pcPrompt);
    	pifRingBuffer_PushString(&s_stTerminal.stTxBuffer, "> ");
    }
}

static BOOL _Sending(void *pvOwner, PIF_stRingBuffer *pstBuffer)
{
	PIF_stTerminal *pstOwner = (PIF_stTerminal *)pvOwner;
	uint16_t usLength;

	if (!pifRingBuffer_IsEmpty(&pstOwner->stTxBuffer)) {
		usLength = pifRingBuffer_CopyAll(pstBuffer, &pstOwner->stTxBuffer);
		pifRingBuffer_Remove(&pstOwner->stTxBuffer, usLength);
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
PIF_stTerminal *pifTerminal_Init(const PIF_stTermCmdEntry *pstCmdTable, const char *pcPrompt)
{
    if (!pstCmdTable || !pcPrompt) {
    	pif_enError = E_enInvalidParam;
    	goto fail;
    }

	s_stTerminal.__pcRxBuffer = calloc(sizeof(char), PIF_TERMINAL_RX_BUFFER_SIZE);
    if (!s_stTerminal.__pcRxBuffer) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }
    s_stTerminal.__ucRxBufferSize = PIF_TERMINAL_RX_BUFFER_SIZE;

    if (!pifRingBuffer_InitAlloc(&s_stTerminal.stTxBuffer, PIF_TERMINAL_TX_BUFFER_SIZE)) goto fail;

    s_stTerminal.__pstCmdTable = pstCmdTable;
    s_stTerminal.__pcPrompt = pcPrompt;
    return &s_stTerminal;

fail:
	pifLog_Printf(LT_enError, "%u Terminal:Init(R:%u T:%u) EC:%d", PIF_TERMINAL_RX_BUFFER_SIZE, PIF_TERMINAL_TX_BUFFER_SIZE, pif_enError);
	return NULL;
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

	if (s_stTerminal.__pcRxBuffer) {
		free(s_stTerminal.__pcRxBuffer);
	    s_stTerminal.__ucRxBufferSize = 0;
	}

	s_stTerminal.__pcRxBuffer = calloc(sizeof(char), usRxSize);
    if (!s_stTerminal.__pcRxBuffer) {
        pif_enError = E_enOutOfHeap;
        goto fail;
    }
    s_stTerminal.__ucRxBufferSize = usRxSize;
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

	pifRingBuffer_Exit(&s_stTerminal.stTxBuffer);
    if (!pifRingBuffer_InitAlloc(&s_stTerminal.stTxBuffer, usTxSize)) goto fail;
    return TRUE;

fail:
	pifLog_Printf(LT_enError, "Terminal:ResizeTxBuffer(S:%u) EC:%d", usTxSize, pif_enError);
    return FALSE;
}

/**
 * @fn pifTerminal_AttachFunction
 * @brief
 * @param pstComm
 */
void pifTerminal_AttachComm(PIF_stComm *pstComm)
{
	pstComm->__pvProcessor = &s_stTerminal;
	pstComm->__fnParing = _Parsing;
	pstComm->__fnSending = _Sending;
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
