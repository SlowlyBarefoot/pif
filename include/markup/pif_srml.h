// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_srml.h
 * @brief Simple Receipt Markup Language
 * @details
 *
 * Metacharacter	Explanation
 * ----------------------------------------------------------------------------------------------------------------------------
 *     {}			At least one character must be present.
 *     []			Items that can be omitted.
 *      |			Select one of the items separated by this character.
 *   [<|=|>]		Align. '<' left alignment, '=' center alignment, '>' right alignment, if not present, default alignment.
 *   				The default alignment of strings is left alignment. The default alignment of numbers is right-aligned.
 *   [n|n.m]		n is the field width. m is the number of digits after the decimal point (at most 9).
 * 					If n is omitted, the value is output from the current position and the alignment is ignored.
 * 					A value longer than n is truncated to n characters.
 * 					If m is omitted for a real number, it is rounded and displayed as an integer.
 *	  'cmd'			Use one lowercase alphabet letter. However, depending on the function implementation,
 * 					uppercase letters are also possible.
 *    'char'		All printable characters can be used.
 *
 *       Command		Explanation
 * ----------------------------------------------------------------------------------------------------------------------------
 *       ['cmd'			If this is in the first column, the line is repeated.
 * 						The loop callback is called with 'cmd' when the loop starts and after each printed line,
 * 						and returns TRUE when there are no more items. If it returns TRUE at the start,
 * 						the line is not printed at all.
 *       ?'cmd'			If this is in the first column, or right after ['cmd', whether or not to print the line
 * 						is determined by the if callback. Inside a loop, it is evaluated for each repetition.
 * %[<|=|>][n|n.m]'cmd'	Convert to the value corresponding to 'cmd'.
 *         %%			Prints '%'.
 *     #{n}'char'		Repeat the 'char' character n times.
 * 		   ##			Prints '#'.
 *
 * Empty lines are printed as they are, except for lines skipped by '?' or by a loop with no items.
 * A line longer than PIF_SRML_MAX_BUFFER_SIZE - 3 characters is truncated.
*/

#ifndef PIF_SRML_H
#define PIF_SRML_H


#include "core/pif.h"


#ifndef PIF_SRML_MAX_BUFFER_SIZE
	#define PIF_SRML_MAX_BUFFER_SIZE     64
#endif


typedef enum EnPifSrmlAlign
{
    SRMLA_NONE    	  = 0,
	SRMLA_LEFT,
	SRMLA_CENTER,
	SRMLA_RIGHT
} PifSrmlAlign;

typedef enum EnPifSrmlType
{
	SRMLT_NONE    	  = 0,
	SRMLT_STRING,
	SRMLT_INT,
	SRMLT_REAL
} PifSrmlType;

struct StPifSrml;
typedef struct StPifSrml PifSrml;

/**
 * @brief Maps a conversion command to its value.
 * @details Set *p_data to a "const char *" for SRMLT_STRING, a "long *" for SRMLT_INT,
 *          or a "double *" for SRMLT_REAL. Inside a loop, use p_owner->loop_idx as the item index.
 */
typedef PifSrmlType (*FSrmlProcessData)(PifSrml *p_owner, char command, void **p_data);
typedef BOOL (*FSrmlProcessLoop)(PifSrml *p_owner, char command);
typedef BOOL (*FSrmlProcessIf)(PifSrml *p_owner, char command);
typedef void (*FSrmlPrintLine)(char *message, int length);

/**
 * @struct StPifSrml
 * @brief Runtime context and callback registry for SRML parsing.
 */
typedef struct StPifSrml
{
	// Public Member Variable
	char ch_decimal;
	char ch_thousand;
    int loop_idx;

	// Read-only Member Variable

	// Private Member Variable
    char __buffer[PIF_SRML_MAX_BUFFER_SIZE];
    const char *__p_format;
    int __offset;

	// Private Member Function
    FSrmlProcessData __f_process_data;
    FSrmlProcessLoop __f_process_loop;
    FSrmlProcessIf __f_process_if;
    FSrmlPrintLine __f_print_line;
} PifSrml;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSrml_Init
 * @brief Initializes an SRML context with callback handlers and default options.
 * @param p_owner Pointer to the SRML context to initialize.
 * @param f_process_data Callback that maps SRML command characters to data values.
 * @param f_process_loop Optional callback that controls loop start and continuation.
 * @param f_process_if Optional callback that evaluates conditional line output.
 * @param f_print_line Callback that receives each completed output line.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifSrml_Init(PifSrml *p_owner, FSrmlProcessData f_process_data, FSrmlProcessLoop f_process_loop, FSrmlProcessIf f_process_if, FSrmlPrintLine f_print_line);

/**
 * @fn pifSrml_Parsing
 * @brief Parses an SRML format string and emits formatted lines through callbacks.
 * @param p_owner Pointer to an initialized SRML context.
 * @param p_format Null-terminated SRML template string to parse.
 */
void pifSrml_Parsing(PifSrml *p_owner, const char *p_format);

#ifdef __cplusplus
}
#endif


#endif	// PIF_SRML_H
