// SPDX-License-Identifier: BSD-3-Clause AND MIT
/**
 * @file pif_basic.h
 * @brief Small BASIC interpreter that runs a program a slice at a time on a task.
 *
 * Based on the BASIC of Jerry Williams Jr (MIT). A program is compiled line by line and run by the interpreter
 * task, at most PIF_BASIC_OPCODE opcodes per release, so the rest of the system keeps running. EXEC calls the C
 * functions given to pifBasic_Init().
 *
 * The language is described in @ref basic_language.
 */

/*****************************************************************************
Copyright 2011 Jerry Williams Jr

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

/*****************************************************************************
    		BASIC INTERPRETER
			JERRY WILLIAMS JR

Thank you for you interest in my little BASIC interpreter.
Since a few people have asked, I've started including the license here in the repository.
I have enjoyed hearing from people who find a use for this, so please do send me an e-mail if you feel so inclined.
The license is the same as MIT with the paragraph removed:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *****************************************************************************/

/**
 * @page basic_language BASIC Language
 *
 * @tableofcontents
 *
 * The language of the BASIC interpreter in interpreter/pif_basic.h. It is the BASIC of Jerry Williams Jr
 * (MIT license), adapted to run on a task: a program is compiled one line per release of the interpreter
 * task, then run PIF_BASIC_OPCODE opcodes per release, so the rest of the system keeps running.
 *
 * This page is written in interpreter/pif_basic.h. Change it there together with the interpreter.
 *
 * @section basic_running Running a program
 *
 * @code{.c}
 * static int _process_led(int count, int* p_params)     // EXEC 1, ...
 * {
 *     // p_params[0] .. p_params[count - 1] are the arguments
 *     return 0;                                           // value of the EXEC expression
 * }
 *
 * static PifBasicProcess s_process[] = { _process_led, NULL };
 *
 * static void _evt_result(PifBasic* p_owner)
 * {
 *     // _result, _parsing_time, _process_time, ... tell how the program ended
 * }
 *
 * pifBasic_Init(s_process, _evt_result);
 * pifBasic_Execute(program_text, 32);                     // 32 opcodes per release
 * @endcode
 *
 * The output of `>` and `FORMAT` and the error messages go to the log (core/pif_log.h).
 *
 * @section basic_statements Statements
 *
 * | Statement | Meaning |
 * | --- | --- |
 * | `# COMMENT ...` | Comment to the end of the line |
 * | `> EXPR` | Prints the value of the expression |
 * | `NAME = EXPR` | Assigns a variable |
 * | `NAME(INDEX) = EXPR` | Assigns an element of an array |
 * | `DIM VAR(LENGTH)` | Makes an array of LENGTH elements |
 * | `LOCAL [VAR [, VAR ...]]` | Declares local variables of a subroutine |
 * | `SUB NAME [PARAM [, PARAM ...]] ... END SUB` | Defines a subroutine |
 * | `NAME [ARG [, ARG ...]]` | Calls a subroutine |
 * | `RETURN [EXPRESSION]` | Returns from a subroutine, with a value |
 * | `IF CONDITION ... [ELSE IF CONDITION ...] [ELSE ...] END IF` | Conditional block |
 * | `IF CONDITION THEN STATEMENT` | Conditional statement on one line |
 * | `WHILE CONDITION ... END WHILE` | Loop while the condition is true |
 * | `FOR VAR=INIT TO FINAL ... END FOR` | Counting loop |
 * | `FORMAT STRING [, EXPRESSION ...]` | Prints a formatted line |
 * | `EXEC PROGRAM [, EXPRESSION ...]` | Calls a C function given to pifBasic_Init() |
 * | `BREAK` | Ends the program |
 * | `BYE` | Ends the program |
 *
 * Notes:
 *
 * - `FORMAT` starts a new line, then prints the string with each `%` replaced by the next argument as an
 *   integer and each `$` by the next argument as a string.
 * - `SUB` may not be nested and must be at the top level.
 * - `LOCAL` and `RETURN` may only be used in a subroutine.
 * - `FOR` stops before `FINAL`: `FOR I=1 TO 3` runs with `I=1` and `I=2`.
 * - Only a statement that does not open or close a block may follow `THEN`.
 * - `RESUME` of the original interpreter is not supported.
 *
 * @section basic_expressions Expressions
 *
 * | Form | Meaning |
 * | --- | --- |
 * | `NUMBER`, `"STRING"`, `NAME`, `(EXPRESSION)` | Value, string, variable, grouping |
 * | `NAME(INDEX)` | Element of an array |
 * | `NAME(ARG, ...)` | Value returned by a subroutine |
 * | `UBOUND(VAR)` | Length of an array |
 * | `*` `/` <code>\\</code> | Multiply, divide, remainder |
 * | `+` `-` | Add, subtract |
 * | `=` `<` `>` `<>` `<=` `=>` | Compare |
 * | `AND` `OR` | Logical and, or |
 *
 * The rows are in order of precedence, highest first. Operators are left-associative.
 *
 * - Values are integers. A boolean is -1 for true and 0 for false.
 * - `AND` and `OR` evaluate both sides.
 * - Strings are meant only for `FORMAT`.
 * - Names are case-insensitive. Array indexes start from 1.
 * - Arrays are allocated by `DIM` and freed when the program ends.
 *
 * @section basic_exec EXEC
 *
 * `EXEC PROGRAM, ARG, ...` calls entry PROGRAM of the table given to pifBasic_Init(), counted from 1. Up to
 * PIF_BASIC_EXEC_SIZE arguments (4 by default) are passed, and each may be an expression. The function gets
 * the number of arguments and their values, and its return value is the value of the `EXEC` expression.
 *
 * @section basic_errors Errors and limits
 *
 * A syntax error or a run-time error ends the program with `_result` FALSE, and the message is logged with
 * its line number. Going past any of the PIF_BASIC_ sizes in pif_conf.h (line length, symbol length,
 * program size, stack, string table, variables, locals) is an error too.
 *
 * @section basic_example Example
 *
 * @code{.unparsed}
 * SUB F X,Y
 *     RETURN X+Y
 * END SUB
 *
 * N = 0
 * FOR I=1 TO 6
 *     N = N + F(I, 10)
 * END FOR
 * FORMAT "SUM: %", N
 *
 * DIM A(3)
 * A(1) = 7
 * FORMAT "LENGTH: %, FIRST: %", UBOUND(A), A(1)
 *
 * IF N > 50 THEN FORMAT "BIG"
 * @endcode
 */

/*****************************************************************************
			IMPLEMENTATION NOTES (for the interpreter itself, not part of the manual)

	* ALL BLOCK STRUCTURES LEAVE THEIR KEYWORD AT THE TOP
		OF THE C-STACK TO BE CHECKED AGAINST "END"
	* "TEMP" KEEPS TRACK OF HOW MANY CELLS ARE ON THE
		RUN-TIME STACK. "RETURN" DROPS THESE BEFORE
		EXITING THE FUNCTION.
	* "LOCAL" CANNOT CHANGE THE MODE OF A VARIABLE; A
		VARIABLE MUST BE NORMAL, AN ARRAY, OR SUBROUTINE
		IN ALL CONTEXTS.
	* "INITBASIC" RESETS THE INTERPRETER FOR EVERY PROGRAM

COMPILING AND RUNNING:
	THE WHOLE PROGRAM IS COMPILED FIRST, ONE LINE PER TASK RELEASE,
	AND THEN RUN, "PIF_BASIC_OPCODE" OPCODES PER TASK RELEASE.
	"CPC" IS THE "PC" OF THE NEXT INSTRUCTION TO COMPILE

SUBROUTINES:
	C-STACK: {JMP}
	STACK: {LINK,SAVED...}
	SUBROUTINE DESCRIPTOR:
		TOTAL LOCALS
		PARAM COUNT
		INDEXES OF PARAMS
	VALUE OF A SUBROUTINE IS IT'S PC
	CALL TAKES VARIABLE INDEX
	* NO DISTINCTION BETWEEN FUNCTIONS AND SUBROUTINES

WHILE:	C-STACK: {TEST-FALSE-BRANCH, TOP}

FOR:
	C-STACK: {TEST-FALSE-BRANCH, I, TOP}
	STACK:	{HI}
	* "FOR_" AND "NEXT_" INSTRUCTIONS DROP THE HI VALUE
		FROM THE STACK ON FAILURE AND INCREMENT

DIMENSIONS:
	THE VALUE OF A DIM IS A POINTER
	THE LENGTH OF THE DIM IS STORED IN THE FIRST CELL
 *****************************************************************************/
#ifndef PIF_BASIC_H
#define PIF_BASIC_H


#include "core/pif.h"

#ifndef PIF_BASIC_LINE_SIZE
	#define PIF_BASIC_LINE_SIZE	80		///< Longest line of a program
#endif
#ifndef PIF_BASIC_SYMBOL
	#define PIF_BASIC_SYMBOL	16		///< Longest name of a symbol
#endif
#ifndef PIF_BASIC_PROGRAM
	#define PIF_BASIC_PROGRAM	2048	///< Bytes of compiled program
#endif
#ifndef PIF_BASIC_STACK
	#define PIF_BASIC_STACK		64		///< Words of the stack
#endif
#ifndef PIF_BASIC_STRING
	#define PIF_BASIC_STRING	1024	///< Bytes of the string table
#endif
#ifndef PIF_BASIC_VARIABLE
	#define PIF_BASIC_VARIABLE	128		///< Most variables
#endif
#ifndef PIF_BASIC_LOCAL
	#define PIF_BASIC_LOCAL		8		///< Most local variables of a SUB
#endif
#ifndef PIF_BASIC_EXEC_SIZE
	#define PIF_BASIC_EXEC_SIZE	4		///< Most parameters of an EXEC call
#endif
#ifndef PIF_BASIC_OPCODE
	#define PIF_BASIC_OPCODE	32		///< Opcodes run per release of the task
#endif


struct StPifBasic;
typedef struct StPifBasic PifBasic;

/**
 * @enum EnPifBasicState
 * @brief Where the interpreter has got to between the releases of its task. A program used to be
 *        parsed and run to the end inside one release; now each release does at most
 *        PIF_BASIC_OPCODE opcodes or one source line and asks for another, so that the rest of
 *        the system keeps running while a program does.
 */
typedef enum EnPifBasicState
{
	BIS_IDLE			= 0,	///< No program is running
	BIS_START			= 1,	///< A program was handed over and nothing has been done with it yet
	BIS_LINE			= 2,	///< Reading and compiling the next source line
	BIS_RUN				= 3		///< Running the compiled program
} PifBasicState;

/**
 * @brief C function called by EXEC n.
 *
 * It gets the count arguments in p_params and returns the value of the EXEC expression.
 */
typedef int (*PifBasicProcess)(int count, int* p_params);

/**
 * @brief Called when a program stops, with the results in the read-only members.
 */
typedef void (*PifEvtBasicResult)(PifBasic* p_owner);


/**
 * @brief State and results of the interpreter. There is one interpreter.
 */
struct StPifBasic
{
	// Read-only Member Variable
    PifTask* _p_task;	///< Interpreter task
    uint32_t _parsing_time;	///< Time in ms spent compiling the program
    uint32_t _process_time;	///< Time in ms spent running the program
    BOOL _result;	///< TRUE when the program ran to its end without an error
    uint16_t _program_size;	///< Size of the compiled program
    int _variable_count;		///< Number of variables used
    int _string_count;			///< Bytes of the string table used
    int _stack_count;			///< Deepest use of the stack

	// Private Member Variable
    char* __p_program;
    char* __p_current;			///< Where the parsing of the program has got to
    PifBasicState __state;
    uint32_t __start_time;
    PifBasicProcess* __p_process;
    int __opcode;
    int __process_size;

	// Private Event Function
    PifEvtBasicResult __evt_result;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the BASIC interpreter and creates its task context.
 * @param p_process Optional external process function table, terminated by NULL.
 * @param evt_result Optional completion callback invoked after execution stops.
 * @return TRUE if initialization succeeds; otherwise FALSE.
 */
BOOL pifBasic_Init(PifBasicProcess* p_process, PifEvtBasicResult evt_result);

/**
 * @brief Schedules parsing, compilation, and execution of a BASIC program string. A program
 *        that is still running is dropped and this one starts in its place.
 * @param p_program Null-terminated BASIC source text. It is read while the program is being
 *                  compiled, so it must stay valid until the result callback is called.
 * @param opcode Opcode budget per scheduler slice; default is used when the value is non-positive.
 * @return TRUE if the program was scheduled; FALSE if pifBasic_Init() has not succeeded or
 *         p_program is NULL.
 */
BOOL pifBasic_Execute(char* p_program, int opcode);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BASIC_H
