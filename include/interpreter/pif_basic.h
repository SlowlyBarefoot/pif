/*****************************************************************************
    		BASIC INTERPRETER
			JERRY WILLIAMS JR

Thank you for you interest in my little BASIC interpreter.
Since a few people have asked, I've started including the license here in the repository.
I have enjoyed hearing from people who find a use for this, so please do send me an e-mail if you feel so inclined.
The license is the same as MIT with the paragraph removed:
	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.


STATEMENTS:
	#	COMMENT ...
	>	EXPR
	DIM	VAR(LENGTH)
	LOCAL	[VAR [, VAR ...]]
	SUB	NAME [PARAM [, PARAM ...]] ... END SUB
	NAME	[ARG [, ARG ...]]
	RETURN	[EXPRESSION]	
	IF	CONDITION ... [ELSE IF CONDITION ...] [ELSE ...] END IF
	IF	CONDITION THEN STATEMENT
	WHILE	CONDITION ... END WHILE
	FOR	VAR=INIT TO FINAL ... END FOR
	FORMAT	STRING [, EXPRESSION ...]
    EXEC  PROGRAM [, EXPRESSION ...]
	RESUME
	BREAK
	BYE
	
	* "%" IN "FORMAT" PRINTS THE NEXT ARGUMENT AS AN INTEGER
	* "SUB" MAY NOT BE NESTED
	* "LOCAL" MAY ONLY BE USED IN A SUBROUTINE

EXPRESSIONS:
	NUMBER, "", NAME, NAME(INDEX), NAME(ARG,...), UBOUND(VAR), (EXPRESSION)
	[EXPRESSION {*|/|\}] EXPRESSION
	[EXPRESSION {+|-}] EXPRESSION
	[EXPRESSION {=|<|>|<>|<=|=>}] EXPRESSION
	[EXPRESSION {AND|OR}] EXPRESSION
	
	* BOOLEAN VALUES ARE -1 FOR TRUE AND 0 FOR FALSE
	* "AND" AND "OR" ARE NOT SHORT-CIRCUITING
	* OPERATORS ARE LEFT-ASSOCIATIVE
	* STRINGS SHOULD ONLY BE USED FOR "FORMAT"

GENERAL NOTES:
	* VARIABLES ARE CASE-INSENSITIVE
	* ARRAY INDEXES START FROM 1
	


			IMPLEMENTATION

	* ALL BLOCK STRUCTURES LEAVE THEIR KEYWORD AT THE TOP
		OF THE C-STACK TO BE CHECKED AGAINST "END"
	* "TEMP" KEEPS TRACK OF HOW MANY CELLS ARE ON THE
		RUN-TIME STACK. "RETURN" DROPS THESE BEFORE
		EXITING THE FUNCTION.
	* "LOCAL" CANNOT CHANGE THE MODE OF A VARIABLE; A
		VARIABLE MUST BE NORMAL, AN ARRAY, OR SUBROUTINE
		IN ALL CONTEXTS.
	* "INITBASIC" IS NOT MEANT TO RESET THE INTERPRETER

COMPILING AND INTERPRETING:
	IF GIVEN A FILE, WE BEGIN IN COMPILED MODE
	IF NO FILE IS GIVEN, WE BEGIN IN INTERPRETED MODE
	"CPC" IS THE "PC" OF THE NEXT INSTRUCTION TO COMPILE
	"IPC" IS THE BEGINNING OF TEMPORARY INTERPRETED CODE
	"OPC" IS THE PC TO RETURN TO AFTER INTERPRETED CODE IS RUN
	A STATMENT IS COMPILED IN EITHER MODE
	AFTER A STATEMENT IS COMPILED, IT IS RUN IN I-MODE BY:
		SETTING "PC" TO "IPC"
		EMITTING A "STOP_"
		RUNNING THE DRIVER
		RESTORING THE "PC" AND "CPC"
	COMPLEX STATEMENTS LIKE IF SET ENTER COMPILED MODE
	* "OPC" IS NULL IF "BREAK" IS RUN FROM COMPILED CODE
	* "RESUME" WITH A VALUE FOR EXPRESSIONS NOT STATEMENTS

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
	* LOCALS MAY ONLY BE USED INSIDE OF A SUBROUTINE
	* TRYING TO USE THEM OUTSIDE WILL NOT REPORT AN ERROR

WHILE:	C-STACK: {TEST-FALSE-BRANCH, TOP}

FOR:
	C-STACK: {TEST-FALSE-BRANCH, I, TOP}
	STACK:	{HI}
	* "FOR_" AND "NEXT_" INSTRUCTIONS DROP THE HI VALUE
		FROM THE STACK ON FAILURE AND INCREMENT

DIMENSIONS:
	THE VALUE OF A DIM IS A POINTER
	THE LENGTH OF THE DIM IS STORED IN THE FIRST CELL

EXEC:
    PROGRAM: The index of the external function, starting with number 1.
    EXPRESSION: Up to four parameters of an external function are default values 
        and can be changed by PIF_BASIC_EXEC_SIZE. It can also be expressed with a formula.
 *****************************************************************************/

#ifndef PIF_BASIC_H
#define PIF_BASIC_H


#include "core/pif.h"

#ifndef PIF_BASIC_LINE_SIZE
	#define PIF_BASIC_LINE_SIZE	80		/* The max size of a line in a program */
#endif
#ifndef PIF_BASIC_SYMBOL
	#define PIF_BASIC_SYMBOL	16		/* Maximum number of characters in the symbol : 16 */
#endif
#ifndef PIF_BASIC_PROGRAM
	#define PIF_BASIC_PROGRAM	2048	/* PROGRAM SIZE */
#endif
#ifndef PIF_BASIC_STACK
	#define PIF_BASIC_STACK		64		/* STACK SIZE */
#endif
#ifndef PIF_BASIC_STRING
	#define PIF_BASIC_STRING	1024	/* STRING TABLE SIZE */
#endif
#ifndef PIF_BASIC_VARIABLE
	#define PIF_BASIC_VARIABLE	128		/* VARIABLE COUNT */
#endif
#ifndef PIF_BASIC_LOCAL
	#define PIF_BASIC_LOCAL		8		/* LOCAL COUNT */
#endif
#ifndef PIF_BASIC_EXEC_SIZE
	#define PIF_BASIC_EXEC_SIZE	4		/* Maximum number of EXEC command parameters */
#endif
#ifndef PIF_BASIC_OPCODE
	#define PIF_BASIC_OPCODE	32		/* Count of opcodes processed at one time */
#endif


struct StPifBasic;
typedef struct StPifBasic PifBasic;

typedef int (*PifBasicProcess)(int count, int* p_params);

typedef void (*PifEvtBasicResult)(PifBasic* p_owner);


struct StPifBasic
{
	// Read-only Member Variable
    PifTask* _p_task;
    uint32_t _parsing_time;
    uint32_t _process_time;
    BOOL _result;
    uint16_t _program_size;
    int _varable_count;
    int _string_count;
    int _stack_count;

	// Private Member Variable
    char* __p_program;
    uint32_t __start_time;
    PifBasicProcess* __p_process;
    int __process_size;

	// Private Event Function
    PifEvtBasicResult __evt_result;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBasic_Init
 * @brief
 * @param p_process
 * @param evt_result
 * @return
 */
BOOL pifBasic_Init(PifBasicProcess* p_process, PifEvtBasicResult evt_result);

/**
 * @fn pifBasic_Execute
 * @brief
 * @param p_program
 */
void pifBasic_Execute(char* p_program);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BASIC_H
