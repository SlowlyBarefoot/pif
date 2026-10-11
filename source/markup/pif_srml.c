// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_srml.c
 * @brief Simple Receipt Markup Language (SRML) parser implementation.
 * @details
 * Implements SRML token parsing, value conversion, alignment, repetition, and
 * line output dispatch through user-provided callback functions.
 */

#include "markup/pif_srml.h"

#include <limits.h>
#include <string.h>


#define SRML_LINE_END_SIZE      3		// "\r\n\0"
#define SRML_NUMBER_SIZE        48		// Sign, 64-bit digits with separators, decimal point and fraction
#define SRML_MAX_FRACTION       9		// 10^9 still fits in a 32-bit unsigned long


/**
 * @brief Checks whether a character can be used as a directive argument.
 * @param ch Character to check.
 * @return TRUE if the character is neither a line end nor the string terminator.
 */
static BOOL _isCommandChar(char ch)
{
    return ch && ch != '\r' && ch != '\n';
}

/**
 * @brief Appends one character to the line buffer, dropping it when the buffer is full.
 * @param p_owner Pointer to the SRML context.
 * @param ch Character to append.
 */
static void _putChar(PifSrml *p_owner, char ch)
{
    if (p_owner->__offset < PIF_SRML_MAX_BUFFER_SIZE - SRML_LINE_END_SIZE) {
        p_owner->__buffer[p_owner->__offset++] = ch;
    }
}

/**
 * @brief Appends the same character several times to the line buffer.
 * @param p_owner Pointer to the SRML context.
 * @param ch Character to append.
 * @param count Number of repetitions.
 */
static void _putChars(PifSrml *p_owner, char ch, int count)
{
    while (count-- > 0) _putChar(p_owner, ch);
}

/**
 * @brief Converts an unsigned integer value to a null-terminated decimal string.
 * @param p_buffer Output buffer for the resulting string.
 * @param value Integer value to convert.
 * @param str_cnt Minimum digit width; left-zero-padded when greater than 0.
 * @param ch_thousand Optional thousands separator; ignored when zero.
 * @return Number of characters written, excluding the null terminator.
 */
static int _numberToString(char *p_buffer, unsigned long value, int str_cnt, char ch_thousand)
{
    char inv_buf[SRML_NUMBER_SIZE];
    int digit_cnt = 0;
    int idx = 0;
    int idx_inv = 0;

    do {
        if (ch_thousand && digit_cnt && digit_cnt % 3 == 0) inv_buf[idx_inv++] = ch_thousand;
        inv_buf[idx_inv++] = (value % 10) + '0';
        digit_cnt++;
        value /= 10;
    } while (value);

    for (; digit_cnt < str_cnt; digit_cnt++) p_buffer[idx++] = '0';
    while (idx_inv) p_buffer[idx++] = inv_buf[--idx_inv];
    p_buffer[idx] = 0;
    return idx;
}

/**
 * @brief Writes a string value into the internal output buffer with alignment.
 * @param p_owner Pointer to the SRML context.
 * @param p_str Source string to write. NULL is treated as an empty string.
 * @param align Alignment mode; defaults to left when not specified.
 * @param str_cnt Field width; when 0, writes without fixed-width padding.
 *                A string longer than the field width is truncated.
 */
static void _convertString(PifSrml *p_owner, const char *p_str, PifSrmlAlign align, int str_cnt)
{
    int i, len, pad_left = 0, pad_right = 0;

    if (!p_str) p_str = "";
    if (align == SRMLA_NONE) align = SRMLA_LEFT;

    len = strlen(p_str);
    if (str_cnt) {
        if (len >= str_cnt) {
            len = str_cnt;
        }
        else {
            switch (align) {
            case SRMLA_CENTER: pad_left = (str_cnt - len) / 2; break;
            case SRMLA_RIGHT:  pad_left = str_cnt - len; break;
            default:           break;
            }
            pad_right = str_cnt - len - pad_left;
        }
    }

    _putChars(p_owner, ' ', pad_left);
    for (i = 0; i < len; i++) _putChar(p_owner, p_str[i]);
    _putChars(p_owner, ' ', pad_right);
}

/**
 * @brief Converts and writes an integer value with optional alignment and width.
 * @param p_owner Pointer to the SRML context.
 * @param value Integer value to format.
 * @param align Alignment mode; defaults to right when not specified.
 * @param str_cnt Field width; when 0, writes without fixed-width padding.
 */
static void _convertInteger(PifSrml *p_owner, long value, PifSrmlAlign align, int str_cnt)
{
    char num_str[SRML_NUMBER_SIZE];
    unsigned long abs_value;
    int idx = 0;

    if (align == SRMLA_NONE) align = SRMLA_RIGHT;

    if (value < 0) {
        num_str[idx++] = '-';
        abs_value = 0UL - (unsigned long)value;		// Also valid for LONG_MIN
    }
    else {
        abs_value = value;
    }

    _numberToString(num_str + idx, abs_value, 0, p_owner->ch_thousand);
    _convertString(p_owner, num_str, align, str_cnt);
}

/**
 * @brief Converts and writes a floating-point value with fraction formatting.
 * @param p_owner Pointer to the SRML context.
 * @param value Real value to format.
 * @param align Alignment mode; defaults to right when not specified.
 * @param str_cnt Field width; when 0, writes without fixed-width padding.
 * @param fraction Number of fractional digits to print (at most SRML_MAX_FRACTION).
 *                 The value is rounded to this number of digits.
 */
static void _convertReal(PifSrml *p_owner, double value, PifSrmlAlign align, int str_cnt, int fraction)
{
    char num_str[SRML_NUMBER_SIZE];
    int i, idx = 0;
    BOOL negative = FALSE;
    unsigned long divisor = 1, int_part, frac_part;

    if (align == SRMLA_NONE) align = SRMLA_RIGHT;
    if (fraction > SRML_MAX_FRACTION) fraction = SRML_MAX_FRACTION;

    if (value < 0.0) {
        negative = TRUE;
        value = -value;
    }

    for (i = 0; i < fraction; i++) divisor *= 10;

    if (value < (double)ULONG_MAX) {
        int_part = (unsigned long)value;
        frac_part = (unsigned long)((value - int_part) * divisor + 0.5);
        if (frac_part >= divisor) {
            int_part++;
            frac_part -= divisor;
        }
    }
    else {
        int_part = ULONG_MAX;
        frac_part = 0;
    }

    if (negative && (int_part || frac_part)) num_str[idx++] = '-';
    idx += _numberToString(num_str + idx, int_part, 0, p_owner->ch_thousand);

    if (fraction) {
        num_str[idx++] = p_owner->ch_decimal;
        _numberToString(num_str + idx, frac_part, fraction, 0);
    }
    _convertString(p_owner, num_str, align, str_cnt);
}

/**
 * @brief Parses and processes a repetition directive that starts with '#'.
 * @details On return, the format pointer points to the first character after the directive.
 * @param p_owner Pointer to the SRML context.
 */
static void _srmlRepeat(PifSrml *p_owner)
{
    int count = 0;
    BOOL has_count = FALSE;

    p_owner->__p_format++;
    while (*p_owner->__p_format >= '0' && *p_owner->__p_format <= '9') {
        if (count < PIF_SRML_MAX_BUFFER_SIZE) count = count * 10 + *p_owner->__p_format - '0';
        has_count = TRUE;
        p_owner->__p_format++;
    }

    if (!_isCommandChar(*p_owner->__p_format)) return;

    if (!has_count && *p_owner->__p_format == '#') _putChar(p_owner, '#');
    else _putChars(p_owner, *p_owner->__p_format, count);
    p_owner->__p_format++;
}

/**
 * @brief Parses and processes a conversion directive that starts with '%'.
 * @details On return, the format pointer points to the first character after the directive.
 * @param p_owner Pointer to the SRML context.
 */
static void _srmlConvert(PifSrml *p_owner)
{
    int num_str = 0, fraction = 0;
    BOOL point = FALSE;
    PifSrmlAlign align = SRMLA_NONE;
    char command;
    void *p_data = NULL;

    for (p_owner->__p_format++; ; p_owner->__p_format++) {
        command = *p_owner->__p_format;
        if (command == '<') align = SRMLA_LEFT;
        else if (command == '=') align = SRMLA_CENTER;
        else if (command == '>') align = SRMLA_RIGHT;
        else if (command == '.') point = TRUE;
        else if (command >= '0' && command <= '9') {
            if (point) {
                if (fraction <= SRML_MAX_FRACTION) fraction = fraction * 10 + command - '0';
            }
            else {
                if (num_str < PIF_SRML_MAX_BUFFER_SIZE) num_str = num_str * 10 + command - '0';
            }
        }
        else break;
    }

    if (!_isCommandChar(command)) return;
    p_owner->__p_format++;

    if (command == '%') {
        _putChar(p_owner, '%');
        return;
    }

    switch ((*p_owner->__f_process_data)(p_owner, command, &p_data)) {
    case SRMLT_STRING:
        _convertString(p_owner, (const char *)p_data, align, num_str);
        break;

    case SRMLT_INT:
        if (p_data) _convertInteger(p_owner, *(long *)p_data, align, num_str);
        break;

    case SRMLT_REAL:
        if (p_data) _convertReal(p_owner, *(double *)p_data, align, num_str, fraction);
        break;

    default:
        break;
    }
}

/**
 * @brief Terminates the current line buffer and passes it to the print callback.
 * @param p_owner Pointer to the SRML context.
 */
static void _srmlPrintLine(PifSrml *p_owner)
{
    p_owner->__buffer[p_owner->__offset++] = '\r';
    p_owner->__buffer[p_owner->__offset++] = '\n';
    p_owner->__buffer[p_owner->__offset] = 0;
    (*p_owner->__f_print_line)(p_owner->__buffer, p_owner->__offset);
    p_owner->__offset = 0;
}

BOOL pifSrml_Init(PifSrml *p_owner, FSrmlProcessData f_process_data, FSrmlProcessLoop f_process_loop, FSrmlProcessIf f_process_if, FSrmlPrintLine f_print_line)
{
	if (!p_owner || !f_process_data || !f_print_line) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSrml));

    p_owner->__f_process_data = f_process_data;
    p_owner->__f_process_loop = f_process_loop;
    p_owner->__f_process_if = f_process_if;
    p_owner->__f_print_line = f_print_line;
    p_owner->ch_decimal = '.';
    return TRUE;
}

void pifSrml_Parsing(PifSrml *p_owner, const char *p_format)
{
    BOOL first = TRUE, ignore = FALSE;
    char ch, loop_command = 0;
    const char *loop_fp = p_format;
    const char *p_line = p_format;

    p_owner->__p_format = p_format;
    p_owner->__offset = 0;
    p_owner->loop_idx = 0;

    for (;;) {
        ch = *p_owner->__p_format;
        switch (ch) {
        case '[':
            if (first && !loop_command && p_owner->__f_process_loop && _isCommandChar(p_owner->__p_format[1])) {
                loop_command = p_owner->__p_format[1];
                p_owner->__p_format += 2;
                loop_fp = p_owner->__p_format;
                p_owner->loop_idx = 0;
                if ((*p_owner->__f_process_loop)(p_owner, loop_command)) {
                    // No items: skip this line entirely.
                    loop_command = 0;
                    ignore = TRUE;
                }
                // 'first' stays TRUE so that a '?' condition can follow the loop marker.
                break;
            }
            if (!ignore) _putChar(p_owner, ch);
            p_owner->__p_format++;
            first = FALSE;
            break;

        case '?':
            if (first && p_owner->__f_process_if && _isCommandChar(p_owner->__p_format[1])) {
                if (!ignore) ignore = !(*p_owner->__f_process_if)(p_owner, p_owner->__p_format[1]);
                p_owner->__p_format += 2;
            }
            else {
                if (!ignore) _putChar(p_owner, ch);
                p_owner->__p_format++;
            }
            first = FALSE;
            break;

        case '#':
            if (ignore) p_owner->__p_format++;
            else _srmlRepeat(p_owner);
            first = FALSE;
            break;

        case '%':
            if (ignore) p_owner->__p_format++;
            else _srmlConvert(p_owner);
            first = FALSE;
            break;

        case 0:
        case '\n':
        case '\r':
            // A template ending with a line break has no extra empty line at the end.
            if (ch == 0 && p_owner->__p_format == p_line) return;

            if (!ignore) _srmlPrintLine(p_owner);
            p_owner->__offset = 0;

            if (ch == '\r' && p_owner->__p_format[1] == '\n') p_owner->__p_format += 2;
            else if (ch) p_owner->__p_format++;

            if (loop_command) {
                p_owner->loop_idx++;
                if ((*p_owner->__f_process_loop)(p_owner, loop_command)) loop_command = 0;
                else p_owner->__p_format = loop_fp;
            }

            p_line = p_owner->__p_format;
            first = TRUE;
            ignore = FALSE;
            break;

        default:
            if (!ignore) _putChar(p_owner, ch);
            p_owner->__p_format++;
            first = FALSE;
            break;
        }
    }
}
