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


typedef enum EnPifLogType
{
	LT_NONE		= 0,
	LT_VCD		= 1,			// Collect Signal : VCD file
	LT_INFO		= 2,
	LT_WARN		= 3,
	LT_ERROR	= 4,
	LT_COMM		= 5
} PifLogType;

typedef int (*PifLogCmd)(int argc, char* argv[]);

/**
 * @class StPifLogCmdEntry
 * @brief
 */
typedef struct StPifLogCmdEntry
{
    //! A pointer to a string containing the name of the command.
    const char* p_name;

    //! A function pointer to the implementation of the command.
    PifLogCmd processor;

    //! A pointer to a string of brief help text for the command.
    const char* p_help;
} PifLogCmdEntry;

/**
 * @struct StPifLogFlag
 * @brief 항목별 Log 출력 여부
 */
typedef union StPifLogFlag
{
	uint32_t all;
	struct {
		uint32_t performance		: 1;
		uint32_t task				: 1;
		uint32_t collect_signal		: 1;
		uint32_t duty_motor			: 1;
		uint32_t step_motor			: 1;
	} bt;
} PifLogFlag;


extern PifLogFlag pif_log_flag;


#ifdef __cplusplus
extern "C" {
#endif

void pifLog_Init();
BOOL pifLog_InitHeap(uint16_t size);
BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer);
void pifLog_Clear();

#ifdef __PIF_LOG_COMMAND__
BOOL pifLog_UseCommand(const PifLogCmdEntry* p_cmd_table, const char* p_prompt);
#endif

void pifLog_Enable();
void pifLog_Disable();

BOOL pifLog_IsEmpty();

void pifLog_Printf(PifLogType type, const char* p_format, ...);

void pifLog_PrintInBuffer();

PifTask* pifLog_GetCommTask();

// Attach Function
BOOL pifLog_AttachComm(PifComm* p_comm);

#ifdef __PIF_LOG_COMMAND__
// Task Function
PifTask* pifLog_AttachTask(PifTaskMode mode, uint16_t period, BOOL start);
#endif

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
