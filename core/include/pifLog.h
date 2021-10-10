#ifndef PIF_LOG_H
#define PIF_LOG_H


#include "pifComm.h"
#include "pifRingBuffer.h"


#define PIF_LOG_CMD_MAX_ARGS        8

#define PIF_LOG_CMD_NO_ERROR        (0)
#define PIF_LOG_CMD_BAD_CMD         (-1)
#define PIF_LOG_CMD_TOO_MANY_ARGS   (-2)
#define PIF_LOG_CMD_TOO_FEW_ARGS   	(-3)
#define PIF_LOG_CMD_INVALID_ARG		(-4)


typedef enum _PIF_enLogType
{
	LT_enNone	= 0,
	LT_enVcd	= 1,			// Collect Signal : VCD file
	LT_enInfo	= 2,
	LT_enWarn	= 3,
	LT_enError	= 4,
	LT_enComm	= 5
} PIF_enLogType;

typedef int (*PIF_fnLogCmd)(int argc, char *argv[]);

/**
 * @class _PIF_stLogCmdEntry
 * @brief
 */
typedef struct _PIF_stLogCmdEntry
{
    //! A pointer to a string containing the name of the command.
    const char *pcName;

    //! A function pointer to the implementation of the command.
    PIF_fnLogCmd fnProcessor;

    //! A pointer to a string of brief help text for the command.
    const char *pcHelp;
} PIF_stLogCmdEntry;

/**
 * @struct _PIF_stLogFlag
 * @brief 항목별 Log 출력 여부
 */
typedef union _PIF_stLogFlag
{
	uint32_t unAll;
	struct {
		uint32_t Performance		: 1;
		uint32_t Task				: 1;
		uint32_t CollectSignal		: 1;
		uint32_t DutyMotor			: 1;
		uint32_t StepMotor			: 1;
	} bt;
} PIF_stLogFlag;


extern PIF_stLogFlag pif_stLogFlag;


#ifdef __cplusplus
extern "C" {
#endif

void pifLog_Init();
BOOL pifLog_InitHeap(uint16_t usSize);
BOOL pifLog_InitStatic(uint16_t usSize, uint8_t *pucBuffer);
void pifLog_Clear();

#ifdef __PIF_LOG_COMMAND__
BOOL pifLog_UseCommand(const PIF_stLogCmdEntry *pstCmdTable, const char *pcPrompt);
#endif

void pifLog_Enable();
void pifLog_Disable();

BOOL pifLog_IsEmpty();

void pifLog_Printf(PIF_enLogType enType, const char *pcFormat, ...);

void pifLog_PrintInBuffer();

PifTask *pifLog_GetCommTask();

// Attach Function
BOOL pifLog_AttachComm(PifComm *pstComm);

#ifdef __PIF_LOG_COMMAND__
// Task Function
PifTask *pifLog_AttachTask(PifTaskMode enMode, uint16_t usPeriod, BOOL bStart);
#endif

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
