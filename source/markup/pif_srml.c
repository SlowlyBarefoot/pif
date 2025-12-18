/**
 * @file pif_srml.c
 * @brief Simple Receipt Markup Langeage
 * @details
*/

#include "markup/pif_srml.h"

#include <stdio.h>
#include <string.h>


static int _numberToString(char *p_buffer, long value, int str_cnt, char ch_thousand)
{
    char inv_buf[16];
    int exp_cnt = 0;
    int zero_str_cnt = 0;
    int idx = 0;
    long idx_inv = 0;
    long tmp_val;

    tmp_val = value;
    if (tmp_val != 0) {
        while (tmp_val) {
            exp_cnt++;
            if (tmp_val >= 10) {
                inv_buf[idx_inv++] = (tmp_val % 10) + '0';
            }
            else {
                inv_buf[idx_inv++] = tmp_val + '0';
                break;
            }
            tmp_val = tmp_val / 10;
            if (ch_thousand && !str_cnt && exp_cnt % 3 == 0) inv_buf[idx_inv++] = ch_thousand;
        }

        if ((str_cnt != 0) && (exp_cnt < str_cnt)) {
            zero_str_cnt = str_cnt - exp_cnt;
            while (zero_str_cnt) {
                p_buffer[idx++] = '0';
                zero_str_cnt--;
            }
        }
        p_buffer[idx] = 0;
        while (idx_inv) {
            idx_inv--;
            p_buffer[idx++] = inv_buf[idx_inv];
        }
    }
    else {
        zero_str_cnt = str_cnt;
        do {
            p_buffer[idx++] = '0';
            if (zero_str_cnt > 0) zero_str_cnt--;
        } while (zero_str_cnt);
    }
    p_buffer[idx] = 0;
    return idx;
}

static void _convertString(PifSrml *p_owner, char *p_str, PifSrmlAlign align, int str_cnt)
{
    char form[16], *p_buffer = p_owner->__buffer + p_owner->__offset;
    int i, len, offset;

    if (align == SRMLA_NONE) align = SRMLA_LEFT;

    if (str_cnt) {
        switch (align) {
        case SRMLA_CENTER:
            len = strlen(p_str);
            if (len < str_cnt) {
                sprintf(form, "%%%ds", (str_cnt - len) / 2 + len);
                offset = sprintf(p_buffer, form, p_str);
                for (i = 0; i < str_cnt - offset; i++) p_buffer[offset + i] = ' ';
            }
            break;

        case SRMLA_RIGHT:
            sprintf(form, "%%%ds", str_cnt);
            sprintf(p_buffer, form, p_str);
            break;

        default:
            offset = sprintf(p_buffer, p_str);
            for (i = 0; i < str_cnt - offset; i++) p_buffer[offset + i] = ' ';
            break;
        }
        p_owner->__offset += str_cnt;
    }
    else {
    	p_owner->__offset += sprintf(p_buffer, p_str);
    }
}

static void _convertInteger(PifSrml *p_owner, long value, PifSrmlAlign align, int str_cnt)
{
    char num_str[16];
    int idx = 0;

    if (align == SRMLA_NONE) align = SRMLA_RIGHT;

    if (value < 0) {
    	num_str[idx++] = '-';
        value *= -1;
    }

    _numberToString(num_str, value, 0, p_owner->ch_thousand);
    _convertString(p_owner, num_str, align, str_cnt);
}

static void _convertReal(PifSrml *p_owner, double value, PifSrmlAlign align, int str_cnt, int fraction)
{
    char num_str[32];
    int i, idx = 0;
    long num;

    if (align == SRMLA_NONE) align = SRMLA_RIGHT;

    if (value < 0.0) {
        num_str[idx++] = '-';
        value *= -1.0;
    }

    num = (long)value;
    idx += _numberToString(num_str + idx, num, 0, p_owner->ch_thousand);

    if (fraction) {
        num_str[idx++] = p_owner->ch_decimal;

        value -= num;
        for (i = 0; i < fraction; i++) value *= 10;

        idx += _numberToString(num_str + idx, (long)(value + 0.5), 0, 0);
    }
    _convertString(p_owner, num_str, align, str_cnt);
}

static void _srmlRepeat(PifSrml *p_owner)
{
    int i, num_str;

    num_str = 0;

NEXT_STR:
	p_owner->__p_format++;
    switch (*p_owner->__p_format) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
		num_str *= 10;
		num_str += *p_owner->__p_format - '0';
        goto NEXT_STR;

    case '#':
    	p_owner->__buffer[p_owner->__offset++] = '#';
        break;

    default:
    	for (i = 0; i < num_str; i++) p_owner->__buffer[p_owner->__offset++] = *p_owner->__p_format;
        break;
    }
}

static void _srmlConvert(PifSrml *p_owner)
{
    int num_str, fraction, point;
    PifSrmlAlign align;
    void *p_data;

    num_str = 0;
    fraction = 0;
    align = SRMLA_NONE;
    point = 0;

NEXT_STR:
	p_owner->__p_format++;
    switch (*p_owner->__p_format) {
    case '<':
        align = SRMLA_LEFT;
        goto NEXT_STR;

    case '=':
        align = SRMLA_CENTER;
        goto NEXT_STR;

    case '>':
        align = SRMLA_RIGHT;
        goto NEXT_STR;

    case '.':
        point = 1;
        goto NEXT_STR;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (point) {
            fraction *= 10;
            fraction += *p_owner->__p_format - '0';
        }
        else {
            num_str *= 10;
            num_str += *p_owner->__p_format - '0';
        }
        goto NEXT_STR;

    case '%':
    	p_owner->__buffer[p_owner->__offset++] = '%';
        break;

    default:
		switch ((*p_owner->__f_process_data)(p_owner, *p_owner->__p_format, &p_data, p_owner->loop_idx)) {
		case SRMLT_STRING:
			_convertString(p_owner, p_data, align, num_str);
			break;

		case SRMLT_INT:
			_convertInteger(p_owner, *(long*)p_data, align, num_str);
			break;

		case SRMLT_REAL:
			_convertReal(p_owner, *(double*)p_data, align, num_str, fraction);
			break;

		default:
			break;
    	}
    	break;
    }
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
	int first = 1, ignore = 0;
    char loop_command;
    const char *loop_fp;

    p_owner->__p_format = p_format;
    p_owner->__offset = 0;
    p_owner->loop_idx = 0;
    loop_command = 0;
    loop_fp = p_owner->__p_format;
    while (*p_owner->__p_format) {
        switch (*p_owner->__p_format) {
        case '[':
			if (p_owner->__f_process_loop && first) {
				p_owner->__p_format++;
				p_owner->loop_idx = 0;
				loop_command = *p_owner->__p_format;
				(*p_owner->__f_process_loop)(p_owner, loop_command);
				loop_fp = p_owner->__p_format + 1;
			}
			else if (!ignore) {
				p_owner->__buffer[p_owner->__offset++] = *p_owner->__p_format;
        	}
            p_owner->__p_format++;
        	first = 0;
            break;

        case '?':
            if (p_owner->__f_process_if && first) {
            	p_owner->__p_format++;
				ignore = !(*p_owner->__f_process_if)(p_owner, *p_owner->__p_format);
            }
            else if (!ignore) {
            	p_owner->__buffer[p_owner->__offset++] = *p_owner->__p_format;
            }
            p_owner->__p_format++;
        	first = 0;
            break;

        case '#':
        	if (!ignore) _srmlRepeat(p_owner);
            p_owner->__p_format++;
        	first = 0;
            break;

        case '%':
        	if (!ignore) _srmlConvert(p_owner);
            p_owner->__p_format++;
        	first = 0;
            break;

        case '\n':
        case '\r':
            if (p_owner->__offset) {
            	p_owner->__buffer[p_owner->__offset++] = '\r';
            	p_owner->__buffer[p_owner->__offset++] = '\n';
            	p_owner->__buffer[p_owner->__offset] = 0;
                (*p_owner->__f_print_line)(p_owner->__buffer, p_owner->__offset);
            }
            p_owner->__offset = 0;
            p_owner->__p_format++;
            if (loop_command) {
            	p_owner->loop_idx++;
                if (p_owner->__f_process_loop) {
                	if ((*p_owner->__f_process_loop)(p_owner, loop_command)) loop_command = 0;
                	else p_owner->__p_format = loop_fp;
                }
            }
        	first = 1;
        	ignore = 0;
            break;

        default:
        	if (!ignore) {
				p_owner->__buffer[p_owner->__offset++] = *p_owner->__p_format;
        	}
			p_owner->__p_format++;
        	first = 0;
            break;
        }
    }
}
