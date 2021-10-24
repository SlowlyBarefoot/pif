#ifndef PIF_LOG_H
#define PIF_LOG_H


#include "pif_comm.h"
#include "pif_ring_buffer.h"


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

/**
 * @fn pifLog_Init
 * @brief Log 구조체 초기화한다.
 */
void pifLog_Init();

/**
 * @fn pifLog_InitHeap
 * @brief Log 구조체 초기화한다.
 * @param size
 * @return
 */
BOOL pifLog_InitHeap(uint16_t size);

/**
 * @fn pifLog_InitStatic
 * @brief Log 구조체 초기화한다.
 * @param size
 * @param p_buffer
 * @return
 */
BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifLog_Clear
 * @brief Log 구조체를 파기하다.
 */
void pifLog_Clear();

#ifdef __PIF_LOG_COMMAND__

/**
 * @fn pifLog_UseCommad
 * @brief
 * @param p_cmd_table
 * @param p_prompt
 * @return
 */
BOOL pifLog_UseCommand(const PifLogCmdEntry* p_cmd_table, const char* p_prompt);

#endif

/**
 * @fn pifLog_Enable
 * @brief
 */
void pifLog_Enable();

/**
 * @fn pifLog_Disable
 * @brief
 */
void pifLog_Disable();

/**
 * @fn pifLog_IsEmpty
 * @brief
 * @return
 */
BOOL pifLog_IsEmpty();

/**
 * @fn pifLog_Printf
 * @brief
 * @param type
 * @param p_format
 */
void pifLog_Printf(PifLogType type, const char* p_format, ...);

/**
 * @fn pifLog_PrintInBuffer
 * @brief
 */
void pifLog_PrintInBuffer();

/**
 * @fn pifLog_GetCommTask
 * @brief
 * @return
 */
PifTask* pifLog_GetCommTask();

/**
 * @fn pifLog_AttachComm
 * @brief
 * @param p_comm
 * @return
 */
BOOL pifLog_AttachComm(PifComm* p_comm);

#ifdef __PIF_LOG_COMMAND__

/**
 * @fn pifLog_AttachTask
 * @brief Task를 추가한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifLog_AttachTask(PifTaskMode mode, uint16_t period, BOOL start);

#endif

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H