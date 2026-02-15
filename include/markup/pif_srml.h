/**
 * @file pif_srml.h
 * @brief Simple Receipt Markup Langeage
 * @details
 * 
 * Metacharacter	Explanation
 * ----------------------------------------------------------------------------------------------------------------------------
 *     {}			At least one character must be present.
 *     []			Items that can be omitted.
 *      |			Select one of the items separated by this character.
 *   [<|=|>]		Align. '<' left alignment, '=' center alignment, '>' right alignment, if not present, default alignment.
 *   				The default alignment of strings is left alignment. The default alignment of numbers is right-aligned.
 *   [n|n.m]		n is the number of digits. m is the number of digits after the decimal point.
 * 					If omitted, it is output from the current position, sort items are ignored, and if it is a real number, 
 * 					decimals are rounded and displayed as integers.
 *	  'cmd'			Use one lowercase alphabet letter. However, depending on the function implementation, 
 * 					uppercase letters are also possible.
 *    'char'		All printable characters can be used.
 * 
 *       Command		Explanation
 * ----------------------------------------------------------------------------------------------------------------------------
 *       ['cmd'			If there is a '[' character in the first line, the line is repeated. 
 * 						The number of repetitions and whether to print is also determined by 'cmd'.
 *       ?'cmd'			When this character is present in the first space, 
 * 						whether or not to print is determined according to the conditions specified from A to Z.
 * %[<|=|>][n|n.m]'cmd'	Convert to the value corresponding to 'cmd'.
 *         %%			Prints '%'.
 *     #{n}'char'		Repeat the 'char' character n times.
 * 		   ##			Prints '@'.
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

typedef PifSrmlType (*FSrmlProcessData)(PifSrml *p_owner, char command, void **p_data, int loop_idx);
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
