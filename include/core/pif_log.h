// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_log.h
 * @brief Log output over a UART, with an optional command line.
 *
 * The log has three tiers. pifLog_Printf() and the other print functions are the input: they format a line
 * with a time stamp and the log type. The outputs are the UART given to pifLog_AttachUart() and every
 * PifLogSink added with pifLog_AddSink(). Between them sits the log buffer, when the log is started with
 * pifLog_InitHeap() or pifLog_InitStatic():
 *
 * - Without it (pifLog_Init()), a print hands the line to every output at once.
 * - With it, a print only stores the line, and the task of the log hands it to the outputs. Each output
 *   reads the buffer at its own pace. When the buffer is full the oldest lines give way, and an output that
 *   had not read them yet counts them in its drop_count. An output added later gets what the buffer still
 *   holds, so lines printed before the UART was attached still reach it.
 *
 * With pifLog_UseCommand() the UART also takes commands from a table of PifLogCmdEntry, with the built-in
 * commands for the version, the status, the log flags and the outputs (pifLog_CmdLog()). The command line
 * itself, its echo and prompt, and what a command prints with LT_NONE go to the UART at once, so they keep
 * their order whatever is waiting in the buffer. When the output of a command is longer than the transmit
 * buffer of pifLog_AttachUart(), the rest of it and the prompt wait in the log buffer and go out in order.
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
#define PIF_LOG_CMD_NO_ERROR        (0)		///< Done
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
#define PIF_LOG_MSG_WRONG_CRC			"Wrong CRC"			///< "Wrong CRC"
#define PIF_LOG_MSG_BAD_FRAME			"Bad Frame"			///< "Bad Frame"
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
	LT_VCD		= 1,	///< Collect Signal : VCD file
	LT_INFO		= 2,	///< Information
	LT_WARN		= 3,	///< Warning
	LT_ERROR	= 4,	///< Error
	LT_COMM		= 5		///< Communication trace
} PifLogType;

#define PIF_LOG_TYPE_MASK(TYPE)		(1U << (TYPE))	///< Bit of a PifLogType in PifLogSink.type_mask
#define PIF_LOG_TYPE_ALL			0x3FU			///< Every PifLogType

typedef struct StPifLogSink PifLogSink;

/**
 * @brief Takes one piece of the log. It must not wait: return FALSE when there is no room, which drops it.
 * @param p_sink The sink.
 * @param type Type of the line.
 * @param p_prefix What the UART gets before the text: the time line once a minute and "\nSS.mmm X " for
 *                 LT_INFO to LT_COMM. NULL for the other types and for pifLog_PrintChar().
 * @param p_text The text without the prefix.
 */
typedef BOOL (*PifActLogSinkWrite)(PifLogSink *p_sink, PifLogType type, const char *p_prefix, const char *p_text);

/**
 * @brief Tells how many bytes a readable sink holds.
 */
typedef uint32_t (*PifActLogSinkSize)(PifLogSink *p_sink);

/**
 * @brief Reads what a readable sink holds, as the text it was given: prefixes and texts one after another.
 *        It is called from the task of the log, so it may take as long as the medium needs.
 * @param offset Offset from the start of what it holds.
 * @return Bytes read, 0 at the end.
 */
typedef uint16_t (*PifActLogSinkRead)(PifLogSink *p_sink, uint32_t offset, uint8_t *p_buffer, uint16_t size);

/**
 * @struct StPifLogSink
 * @brief An output of the log. Embed it as the first member of a sink object and add it with
 *        pifLog_AddSink(). Sinks get lines whether the log is enabled or not.
 *
 * Without the log buffer act_write is called from the print function, and a refused piece is lost. With
 * the buffer it is called from the task of the log, and a refused piece is offered again later: it is
 * only lost when the buffer has to give it way first.
 *
 * A sink that can read back what it stored, such as a file on an SD card, sets act_size and act_read and
 * a p_name, so the log command can show it on the terminal.
 */
struct StPifLogSink
{
	PifActLogSinkWrite act_write;	///< Writer of the sink
	uint8_t type_mask;				///< PIF_LOG_TYPE_MASK() of the types it gets
	uint32_t drop_count;			///< Pieces lost: refused without the buffer, pushed out of it with it
	const char *p_name;				///< Name in the log command, or NULL
	PifActLogSinkSize act_size;		///< Size of what it holds, or NULL when it cannot be read back
	PifActLogSinkRead act_read;		///< Reader of what it holds, or NULL when it cannot be read back

	// Private
	PifLogSink *__p_next;
	uint32_t __read_pos;			///< Next piece of the log buffer it gets
};

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
 * @brief Starts the log without the log buffer: a print hands the line to every output at once. With
 *        PIF_LOG_COMMAND it also adds the task of the log, for the command line.
 * @return TRUE on success, FALSE when the task could not be added.
 */
BOOL pifLog_Init();

/**
 * @brief Starts the log with a log buffer from the heap, and the task that hands its lines to the outputs.
 * @param size Size of the log buffer in bytes.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_InitHeap(uint16_t size);

/**
 * @brief Starts the log with a log buffer of the caller's, and the task that hands its lines to the
 *        outputs.
 * @param size Size of p_buffer in bytes.
 * @param p_buffer Buffer of size bytes. It must stay valid while the log is used.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_InitStatic(uint16_t size, uint8_t* p_buffer);

/**
 * @brief Stops the log: detaches the UART, removes the sinks and frees the buffers and the task.
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
 * @brief Stops lines going out to the UART, except collect signal (LT_VCD) lines. The lines are still
 *        stored and still go to the sinks.
 */
void pifLog_Disable();

/**
 * @brief Tells whether every output has got everything in the log buffer.
 * @return TRUE when nothing waits for an output, and always without the log buffer.
 */
BOOL pifLog_IsEmpty();

/**
 * @brief Writes one character, without a time stamp.
 *        The character is dropped when the transmit buffer is full, as a line is.
 * @param ch Single character value to output.
 */
void pifLog_PrintChar(char ch);

/**
 * @brief Returns how many lines and characters the UART lost: without the log buffer those its
 *        transmit buffer had no room for, with it those pushed out of the log buffer before the UART got
 *        them. Logging never waits for room, so this is the only place the loss is visible: a count that
 *        keeps growing means a buffer or the baud rate is too small for how much is being logged.
 * @return Number of lines and characters lost since the last reset.
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
 * @brief Hands the outputs what they can take of the log buffer right now, as the task of the log does.
 *        Kept for the code written before the task did it.
 * @return TRUE while the UART still has something to get from the log buffer.
 */
BOOL pifLog_PrintInBuffer();

/**
 * @brief Sends the log through a UART, as its only client. With the command line on, the prompt is sent
 *        at once. With the log buffer, the UART gets what the buffer still holds first.
 * @param p_uart UART to send through.
 * @param size Size of the transmit buffer of the log in bytes, allocated from the heap.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_AttachUart(PifUart* p_uart, uint16_t size);

/**
 * @brief Hands the log buffer to the outputs and runs the UART transmit task until everything for the
 *        UART is sent, holding the CPU. For use before a reset or a halt.
 */
void pifLog_SendAndExit();

/**
 * @brief Detaches the log from its UART and frees the transmit buffer.
 */
void pifLog_DetachUart();

/**
 * @brief Adds an output. Call it after pifLog_Init(), and not while a line may be printed from an interrupt.
 *        With the log buffer, the sink gets what the buffer still holds first. Without it (pifLog_Init()),
 *        act_write is called where the line is printed, maybe in an interrupt, so it must be short: a slow
 *        sink such as an SD card needs the log buffer.
 * @param p_sink Sink with act_write and type_mask set. It must stay valid until removed.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLog_AddSink(PifLogSink *p_sink);

/**
 * @brief Removes an output added with pifLog_AddSink(). pifLog_Clear() removes them all.
 * @param p_sink Sink to remove.
 */
void pifLog_RemoveSink(PifLogSink *p_sink);

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

#ifdef PIF_LOG_COMMAND

/**
 * @brief Built-in command for the outputs of the log. Add it to the table as
 *        { "log", pifLog_CmdLog, "Log outputs", "list | show <name> [lines] | cat <name>" }.
 *        - list: the log buffer ("ram") and the sinks, with what they hold and lost.
 *        - show: the last lines (20 by default) of the log buffer or of a readable sink.
 *        - cat: all of it.
 *        show and cat go on in the task of the log as the UART takes them, and a key stops them.
 * @param argc Number of command arguments.
 * @param argv Array of command argument strings.
 * @return PIF_LOG_CMD_NO_ERROR, or an error code for a bad argument.
 */
int pifLog_CmdLog(int argc, char *argv[]);

#endif

#ifdef __cplusplus
}
#endif


#endif	// PIF_LOG_H
