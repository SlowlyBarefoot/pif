// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_srml.h
 * @brief Simple Receipt Markup Language: lines of text, such as a receipt, made from a template and callbacks.
 *
 * The syntax is described in @ref srml_syntax.
 */

/**
 * @page srml_syntax SRML Syntax
 *
 * @tableofcontents
 *
 * SRML (Simple Receipt Markup Language) of markup/pif_srml.h turns a template into lines of text, such as the
 * lines of a receipt. The template is plain text with commands that insert values, repeat a line for each
 * item of a list and print a line only when a condition holds. The values come from callbacks, so the same
 * template works with any data.
 *
 * This page is written in markup/pif_srml.h. Change it there together with the parser.
 *
 * @section srml_using Using it
 *
 * @code{.c}
 * static PifSrmlType _process_data(PifSrml* p_owner, char command, void** p_data)
 * {
 *     switch (command) {
 *     case 'c': *p_data = company; return SRMLT_STRING;                         // const char *
 *     case 'i': *p_data = item_name[p_owner->loop_idx]; return SRMLT_STRING;
 *     case 'q': *p_data = &item_qty[p_owner->loop_idx]; return SRMLT_INT;       // long *
 *     case 't': *p_data = &total; return SRMLT_REAL;                            // double *
 *     default: return SRMLT_NONE;
 *     }
 * }
 *
 * static BOOL _process_loop(PifSrml* p_owner, char command)
 * {
 *     return p_owner->loop_idx >= item_count;     // TRUE: no more items
 * }
 *
 * static BOOL _process_if(PifSrml* p_owner, char command)
 * {
 *     return show_footer;                          // TRUE: print the line
 * }
 *
 * static void _print_line(char* message, int length)
 * {
 *     // Send the line to the printer
 * }
 *
 * pifSrml_Init(&srml, _process_data, _process_loop, _process_if, _print_line);
 * pifSrml_Parsing(&srml, receipt_template);
 * @endcode
 *
 * ch_decimal and ch_thousand of PifSrml set the decimal point and the thousands separator of numbers.
 *
 * @section srml_notation Notation
 *
 * The tables below use this notation. It is not part of the template.
 *
 * <table>
 * <tr><th>Notation</th><th>Meaning</th></tr>
 * <tr><td><code>{}</code></td><td>At least one character must be present</td></tr>
 * <tr><td><code>[]</code></td><td>May be left out</td></tr>
 * <tr><td><code>|</code></td><td>One of the items it separates</td></tr>
 * <tr><td><code>[&lt;|=|&gt;]</code></td><td>Alignment: <code>&lt;</code> left, <code>=</code> center,
 * <code>&gt;</code> right. Strings default to left, numbers to right.</td></tr>
 * <tr><td><code>[n|n.m]</code></td><td>n is the field width and m the digits after the decimal point, at most 9.
 * Without n the value is printed where the line is and the alignment is ignored. A value longer than n is cut to
 * n characters. A real number without m is rounded to an integer.</td></tr>
 * <tr><td><code>cmd</code></td><td>One lowercase letter. Uppercase letters may work too, depending on the
 * callbacks.</td></tr>
 * <tr><td><code>char</code></td><td>Any printable character</td></tr>
 * </table>
 *
 * @section srml_commands Commands
 *
 * <table>
 * <tr><th>Command</th><th>Meaning</th></tr>
 * <tr><td><code>[cmd</code></td><td>In the first column: repeats the line. The loop callback is called with cmd
 * when the loop starts and after each printed line, and returns TRUE when there are no more items. When it
 * returns TRUE at the start, the line is not printed at all. loop_idx counts the items from 0.</td></tr>
 * <tr><td><code>?cmd</code></td><td>In the first column, or right after <code>[cmd</code>: the if callback
 * decides whether the line is printed. Inside a loop it is asked for each repetition.</td></tr>
 * <tr><td><code>\%[&lt;|=|&gt;][n|n.m]cmd</code></td><td>Inserts the value the data callback gives for
 * cmd</td></tr>
 * <tr><td><code>\%\%</code></td><td>Prints <code>\%</code></td></tr>
 * <tr><td><code>\#{n}char</code></td><td>Prints char n times</td></tr>
 * <tr><td><code>\#\#</code></td><td>Prints <code>\#</code></td></tr>
 * </table>
 *
 * Empty lines are printed as they are, except lines skipped by `?` or by a loop with no items. A line longer
 * than PIF_SRML_MAX_BUFFER_SIZE - 3 characters is cut.
 *
 * @section srml_example Example
 *
 * The template of a receipt, with the commands of the data callback above and a few more:
 *
 * @code{.unparsed}
 * %=32c
 * Date: %d
 *
 * ITEM NAME          QTY    AMOUNT
 * #32-
 * [P%18i %>3q %9.2u
 * #32-
 * Total %26.2t
 * ?a%=32c
 * @endcode
 *
 * - `%=32c` centers the company name in 32 columns.
 * - `#32-` draws a line of 32 dashes.
 * - `[P...` repeats the item line once per product, with the name in 18 columns, the quantity right-aligned
 *   in 3 and the amount in 9 columns with 2 decimals.
 * - `?a...` prints the last line only when the if callback answers TRUE for `a`.
 */
#ifndef PIF_SRML_H
#define PIF_SRML_H


#include "core/pif.h"


#ifndef PIF_SRML_MAX_BUFFER_SIZE
	#define PIF_SRML_MAX_BUFFER_SIZE     64	///< Size of the line buffer
#endif


/**
 * @brief Alignment of a value in its field.
 */
typedef enum EnPifSrmlAlign
{
    SRMLA_NONE    	  = 0,	///< Default: strings left, numbers right
	SRMLA_LEFT,	///< Left
	SRMLA_CENTER,	///< Center
	SRMLA_RIGHT	///< Right
} PifSrmlAlign;

/**
 * @brief Type of the value FSrmlProcessData gives.
 */
typedef enum EnPifSrmlType
{
	SRMLT_NONE    	  = 0,	///< No value: the command is unknown
	SRMLT_STRING,	///< A const char *
	SRMLT_INT,	///< A long *
	SRMLT_REAL	///< A double *
} PifSrmlType;

struct StPifSrml;
typedef struct StPifSrml PifSrml;

/**
 * @brief Maps a conversion command to its value.
 * @details Set *p_data to a "const char *" for SRMLT_STRING, a "long *" for SRMLT_INT,
 *          or a "double *" for SRMLT_REAL. Inside a loop, use p_owner->loop_idx as the item index.
 */
typedef PifSrmlType (*FSrmlProcessData)(PifSrml *p_owner, char command, void **p_data);
/**
 * @brief Steps a loop of the given command. Returns TRUE when there are no more items.
 */
typedef BOOL (*FSrmlProcessLoop)(PifSrml *p_owner, char command);
/**
 * @brief Tells whether a line with ?command is printed.
 */
typedef BOOL (*FSrmlProcessIf)(PifSrml *p_owner, char command);
/**
 * @brief Outputs one formatted line of length characters.
 */
typedef void (*FSrmlPrintLine)(char *message, int length);

/**
 * @struct StPifSrml
 * @brief Runtime context and callback registry for SRML parsing.
 */
typedef struct StPifSrml
{
	// Public Member Variable
	char ch_decimal;	///< Decimal point of real numbers
	char ch_thousand;	///< Thousands separator of numbers, 0 for none
    int loop_idx;	///< Item index of the current loop, from 0

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
 * @brief Parses an SRML format string and emits formatted lines through callbacks.
 * @param p_owner Pointer to an initialized SRML context.
 * @param p_format Null-terminated SRML template string to parse.
 */
void pifSrml_Parsing(PifSrml *p_owner, const char *p_format);

#ifdef __cplusplus
}
#endif


#endif	// PIF_SRML_H
