// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_log.h
 * @brief Log output over a UART, with an optional command line.
 *
 * pifLog_Printf() formats a line with a time stamp and the log type and queues it for the UART. With
 * pifLog_UseCommand() the same UART takes commands from a table of PifLogCmdEntry, with the built-in commands
 * for the version, the status and the log flags.
 */
#ifndef PIF_LOG_H
#define PIF_LOG_H


#include "communication/pif_uart.h"


#ifndef PIF_LOG_LINE_SIZE
#define PIF_LOG_LINE_SIZE			80	///< Longest line pifLog_Printf() formats, in bytes
#endif

#define PIF_LOG_CMD_MAX_ARGS        8	///< Most arguments of a command, the command name included

/** @name Command results
 *  Values a command processor returns. A command may return PIF_LOG_CMD_USER_ERROR or lower for errors
 *  of its own.
 *  @{
 */
#define PIF_LOG_CMD_NO_ERROR        (0)	///< Done
#define PIF_LOG_CMD_BAD_CMD         (-1)	///< No such command
#define PIF_LOG_CMD_TOO_MANY_ARGS   (-2)	///< More than PIF_LOG_CMD_MAX_ARGS arguments
#define PIF_LOG_CMD_TOO_FEW_ARGS   	(-3)	///< Not enough arguments
#define PIF_LOG_CMD_INVALID_ARG		(-4)	///< An argument is not valid
#define PIF_LOG_CMD_USER_ERROR		(-10)	///< User defined error
/** @} */

/** @name Messages of dropped packets
 *  Wording shared by the protocol modules when they log a packet they drop. Pasted into the format
 *  string, so an unused one costs nothing and the same one used by several modules is stored once.
 *  @{
 */
#define PIF_LOG_MSG_WRONG_CRC			"Wrong CRC"	///< "Wrong CRC"
#define PIF_LOG_MSG_BAD_FRAME			"Bad Frame"	///< "Bad Frame"
#define PIF_LOG_MSG_UNKNOWN_FUNCTION	"Unknown Function"	///< "Unknown Function"
#define PIF_LOG_MSG_INVALID_RESPONSE	"Invalid Response"	///< "Invalid Response"
#define PIF_LOG_MSG_INVALID_PACKET_NO	"Invalid Packet No"	///< "Invalid Packet No"
#define PIF_LOG_MSG_OUT_OF_SEQUENCE		"Out of Sequence"	///< "Out of Sequence"
/** @} */


/**
 * @brief Type of a log line. INFO, WARN, ERROR and COMM lines get a time stamp and a type letter.
 */
typedef enum EnPifLogType
{
	LT_NONE		= 0,	///< Plain text without a prefix
	LT_VCD		= 1,			///< Collect Signal : VCD file
	LT_INFO		= 2,	///< Information
	LT_WARN		= 3,	///< Warning
	LT_ERROR	= 4,	///< Error
	LT_COMM		= 5	///< Communication trace
} PifLogType;

/**
 * @brief Gets a control character the command line does not handle itself.
 */
typedef void (*PifEvtLogControlChar)(char ch);

/**
 * @class StPifLogCmdEntry
 * @brief One command of the command line, in the table given to pifLog_UseCommand().
 */
typedef struct StPifLogCmdEntry
{
    //! A pointer to a string containing the name of the command.
    const char* p_name;

    //! A function pointer to the implementation of the command.
    int (*processor)(int argc, char* argv[]);

    //! A pointer to a string of brief help text for the command.
    const char* p_help;

    //! A pointer to a string of brief help text for the arguments.
    const char* p_args;
} PifLogCmdEntry;

/**
 * @struct StPifLogFlag
 * @brief Switches of the periodic and debug logs. Each can be changed from the command line.
 */
typedef union StPifLogFlag
{
	uint32_t all;	///< All switches as one word
	struct {
		uint8_t performance			: 1;	///< Load and loop time of the main loop, every second
		uint8_t task				: 1;	///< Task statistics
		uint8_t collect_signal		: 1;	///< Collect signal messages
		uint8_t duty_motor			: 1;	///< Duty changes of the duty motors
		uint8_t step_motor			: 1;	///< State changes of the step motors
	} bt;	///< Switches one by one
} PifLogFlag;


extern PifLogFlag pif_log_flag;	///< Log switches, all OFF at start


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts the log. With PIF_LOG_COMMAND it also adds the task of the command line. Lines are not
 *        retained.
 * @return TRUE on success, FALSE when the task could not be added.
 */
BOOL pifLog_Init();

/**
 * @brief Starts the log as pifLog_Init() does, and also retains every line in a buffer from the heap, to
 *        be sent later with pifLog_PrintInBuffer().
 * @param size Size of the retained buffer in bytes.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_InitHeap(uint16_t size);

/**
 * @brief Starts the log as pifLog_Init() does, and also retains every line in a buffer of the caller's,
 *        to be sent later with pifLog_PrintInBuffer().
 * @param size Size of p_buffer in bytes.
 * @param p_buffer Buffer of size bytes. It must stay valid while the log is used.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer);

/**
 * @brief Stops the log: detaches the UART and frees the buffers and the command line task.
 */
void pifLog_Clear();

#ifdef PIF_LOG_COMMAND

/**
 * @brief Turns on the command line on the UART of the log. Only with PIF_LOG_COMMAND.
 * @param size Size of the line buffer, the longest command line plus 3. At least 4.
 * @param p_cmd_table Commands, ending with an entry whose p_name is NULL. The table must
 *                    stay valid.
 * @param p_prompt Prompt shown before each command line. It must stay valid.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_UseCommand(uint8_t size, const PifLogCmdEntry* p_cmd_table, const char* p_prompt);

/**
 * @brief Sets the callback that gets the control characters the command line does not handle itself.
 * @param evt_control_char Callback, or NULL.
 */
void pifLog_AttachEvent(PifEvtLogControlChar evt_control_char);

#endif

/**
 * @brief Lets lines go out to the UART again. The log starts enabled.
 */
void pifLog_Enable();

/**
 * @brief Stops lines going out to the UART, except collect signal (LT_VCD) lines. They are still
 *        retained in the buffer.
 */
void pifLog_Disable();

/**
 * @brief Tells whether the retained buffer is empty.
 * @return TRUE when it holds nothing to send.
 */
BOOL pifLog_IsEmpty();

/**
 * @brief Writes one character, without a time stamp.
 *        The character is dropped when the transmit buffer is full, as a line is.
 * @param ch Single character value to output.
 */
void pifLog_PrintChar(char ch);

/**
 * @brief Returns how many lines and characters have been dropped because the transmit buffer had
 *        no room for them. Logging never waits for room, so this is the only place the loss is
 *        visible: a count that keeps growing means the buffer or the baud rate is too small for
 *        how much is being logged.
 * @return Number of lines and characters dropped since the last reset.
 */
uint32_t pifLog_DropCount();

/**
 * @brief Clears the count reported by pifLog_DropCount().
 */
void pifLog_ResetDropCount();

/**
 * @brief Writes a string, with a time stamp and the type letter for LT_INFO, LT_WARN, LT_ERROR and
 *        LT_COMM.
 * @param type Type of the line.
 * @param p_string Text to write.
 */
void pifLog_Print(PifLogType type, const char* p_string);

/**
 * @brief Formats and writes a line, with a time stamp and the type letter for LT_INFO, LT_WARN, LT_ERROR
 *        and LT_COMM. The format is that of pif_PrintFormat() and the line is cut at PIF_LOG_LINE_SIZE.
 * @param type Type of the line.
 * @param p_format Format string, followed by its arguments.
 */
void pifLog_Printf(PifLogType type, const char* p_format, ...);

/**
 * @brief Hands as much of the retained log buffer to the UART as its transmit buffer will take
 *        right now. Emptying the rest needs the UART task to run, so this does not do it all in
 *        one call: keep calling it from a task while it returns TRUE.
 * @return TRUE while the retained buffer still holds something to send.
 */
BOOL pifLog_PrintInBuffer();

/**
 * @brief Sends the log through a UART, as its only client. With the command line on, the prompt is sent
 *        at once.
 * @param p_uart UART to send through.
 * @param size Size of the transmit buffer of the log in bytes, allocated from the heap.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_AttachUart(PifUart* p_uart, uint16_t size);

/**
 * @brief Runs the UART transmit task until everything the log queued is sent, holding the CPU. For use
 *        before a reset or a halt.
 */
void pifLog_SendAndExit();

/**
 * @brief Detaches the log from its UART and frees the transmit buffer.
 */
void pifLog_DetachUart();

/**
 * @brief Built-in command that lists the commands with their help text.
 * @param argc Number of arguments.
 * @param argv Arguments.
 * @return PIF_LOG_CMD_NO_ERROR.
 */
int pifLog_CmdHelp(int argc, char *argv[]);

/**
 * @param argc Number of command arguments.
 * @param argv Array of command argument strings.
 * @return PIF_LOG_CMD_NO_ERROR.
 */
int pifLog_CmdPrintVersion(int argc, char* argv[]);

/**
 * @param argc Number of command arguments.
 * @param argv Array of command argument strings.
 * @return PIF_LOG_CMD_NO_ERROR.
 */
int pifLog_CmdPrintTask(int argc, char* argv[]);

/**
 * @param argc Number of command arguments.
 * @param argv Array of command argument strings.
 * @return PIF_LOG_CMD_NO_ERROR, or an error code for a bad argument.
 */
int pifLog_CmdSetStatus(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
