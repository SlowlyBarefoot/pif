#ifndef PIF_TERMINAL_H
#define PIF_TERMINAL_H


#include "pifComm.h"
#include "pifRingBuffer.h"


#ifndef PIF_TERMINAL_RX_BUFFER_SIZE
#define PIF_TERMINAL_RX_BUFFER_SIZE		64
#endif
#ifndef PIF_TERMINAL_TX_BUFFER_SIZE
#define PIF_TERMINAL_TX_BUFFER_SIZE		128
#endif

#define PIF_TERM_CMD_MAX_ARGS           8

#define PIF_TERM_CMD_NO_ERROR           (0)
#define PIF_TERM_CMD_BAD_CMD            (-1)
#define PIF_TERM_CMD_TOO_MANY_ARGS      (-2)
#define PIF_TERM_CMD_TOO_FEW_ARGS   	(-3)
#define PIF_TERM_CMD_INVALID_ARG		(-4)


typedef int (*PIF_fnTermCmd)(int argc, char *argv[]);

/**
 * @class _PIF_stTermCmdEntry
 * @brief
 */
typedef struct _PIF_stTermCmdEntry
{
    //! A pointer to a string containing the name of the command.
    const char *pcName;

    //! A function pointer to the implementation of the command.
    PIF_fnTermCmd fnProcessor;

    //! A pointer to a string of brief help text for the command.
    const char *pcHelp;
} PIF_stTermCmdEntry;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifTerminal_Init(const PIF_stTermCmdEntry *pstCmdTable, const char *pcPrompt);

BOOL pifTerminal_ResizeRxBuffer(uint16_t usRxSize);
BOOL pifTerminal_ResizeTxBuffer(uint16_t usTxSize);

PIF_stRingBuffer *pifTerminal_GetTxBuffer();

void pifTerminal_AttachComm(PIF_stComm *pstComm);

int pifTerminal_PrintVersion(int argc, char *argv[]);
int pifTerminal_SetStatus(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TERMINAL_H
