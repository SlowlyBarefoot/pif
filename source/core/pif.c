// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif.h"
#include "core/pif_marker.h"
#ifndef	PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "core/pif_task_manager.h"

#include <string.h>

// Core runtime globals, timekeeping, and shared utility functions.

PifError pif_error = E_SUCCESS;

volatile PifState pif_state = PST_NONE;

uint32_t pif_timer1us = 0L;
volatile uint16_t pif_timer1ms = 0;
volatile uint32_t pif_timer1sec = 0L;
volatile PifDateTime pif_datetime;

volatile uint32_t pif_cumulative_timer1ms = 0L;

PifPerformance pif_performance = {
		._count = 0,
		._task_time1us = 0UL,
		._max_delay = 0UL,
		._miss_count = 0UL,
		._task_load = 0,
		.__state = FALSE,
#ifdef PIF_DEBUG
		.__max_loop_time1us = 0UL
#endif
};

PifId pif_id = 1;

PifActTimer1us pif_act_timer1us = NULL;

PifActGpioRead pif_act_gpio_read = NULL;
PifActGpioWrite pif_act_gpio_write = NULL;

PifActIrqDisable pif_act_irq_disable = NULL;
PifActIrqRestore pif_act_irq_restore = NULL;

PifActMarker pif_act_marker = NULL;

const char* kPifMonth3[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char* kPifHexUpperChar = "0123456789ABCDEF";
const char* kPifHexLowerChar = "0123456789abcdef";

const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };


// Writes the len characters of p_num right-aligned in a field of width, filling the front with spaces.
// Nothing is written if it does not fit.
static size_t _putSpacePadded(char* p_buffer, size_t buffer_size, size_t offset, const char* p_num, size_t len, uint16_t width)
{
	size_t pad = len < width ? width - len : 0;

	if (offset + pad + len < buffer_size - 1) {
		memset(p_buffer + offset, ' ', pad);
		memcpy(p_buffer + offset + pad, p_num, len);
		offset += pad + len;
	}
	return offset;
}

// ASCII only, so that it does not depend on the locale of the C library.
static int _toLower(int ch)
{
	return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}


BOOL pif_Init(PifActTimer1us act_timer1us, PifActSetup act_port_setup, PifActSetup act_app_setup)
{
	if (!act_timer1us || !act_port_setup) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	pif_act_timer1us = act_timer1us;

	pif_datetime.month = 1;
	pif_datetime.day = 1;

	// The time base runs from here, so that the setups can wait with pif_Delay1ms().
	pif_state = PST_TIMER;
	if (!(*act_port_setup)()) goto fail;
	if (act_app_setup && !(*act_app_setup)()) goto fail;

	pif_state = PST_RUNNING;
    return TRUE;

fail:
	pif_Exit();
	return FALSE;
}

void pif_Exit()
{
	pif_state = PST_NONE;
	pifTaskManager_Clear();
}

void pif_sigTimer1ms()
{
	uint8_t days;
	uint16_t year;

	if (pif_state < PST_TIMER) return;

	pif_cumulative_timer1ms++;
    pif_timer1ms++;
    if (pif_timer1ms >= 1000) {
        pif_timer1ms = 0;

        pif_timer1sec++;
    	pif_datetime.second++;
    	if (pif_datetime.second >= 60) {
    		pif_datetime.second = 0;
    		pif_datetime.minute++;
    		if (pif_datetime.minute >= 60) {
    			pif_datetime.minute = 0;
    			pif_datetime.hour++;
    			if (pif_datetime.hour >= 24) {
    				pif_datetime.hour = 0;
    				pif_datetime.day++;
    				days = kDaysInMonth[pif_datetime.month - 1];
    				if (pif_datetime.month == 2) {
    					year = 2000 + pif_datetime.year;
    					if (year % 4 == 0) {
    						if (year % 100 == 0) {
    							if (year % 400 == 0) days++;
    						}
    						else days++;
    					}
    				}
    				if (pif_datetime.day > days) {
    					pif_datetime.day = 1;
    					pif_datetime.month++;
    					if (pif_datetime.month > 12) {
    						pif_datetime.month = 1;
    						pif_datetime.year++;
    					}
    				}
    			}
    		}

#ifdef PIF_DEBUG
        	pif_performance.__state |= 4;
#endif			
    	}

    	pif_performance.__state |= 2;
    }

   	pif_performance.__state |= 1;
}

BOOL pif_SetDateTime(const PifDateTime* p_datetime)
{
	uint8_t days;
	uint16_t year;
	uint32_t state;

	if (!p_datetime || p_datetime->month < 1 || p_datetime->month > 12 || p_datetime->day < 1 ||
			p_datetime->hour >= 24 || p_datetime->minute >= 60 || p_datetime->second >= 60 ||
			p_datetime->millisecond >= 1000) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	days = kDaysInMonth[p_datetime->month - 1];
	if (p_datetime->month == 2) {
		year = 2000 + p_datetime->year;
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) days++;
	}
	if (p_datetime->day > days) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// pif_sigTimer1ms() may run from an interrupt, so the fields change together.
	state = pif_IrqDisable();
	pif_datetime = *p_datetime;
	pif_timer1ms = p_datetime->millisecond;
	pif_IrqRestore(state);
	return TRUE;
}

void pif_Delay1ms(uint32_t delay)
{
	uint32_t start, diff;

	start = pif_cumulative_timer1ms;
	do {
		diff = pif_cumulative_timer1ms - start;
	} while (diff < delay);
}

void pif_Delay1us(uint32_t delay)
{
	uint32_t start, diff;

	start = (*pif_act_timer1us)();
	do {
		diff = (*pif_act_timer1us)() - start;
	} while (diff < delay);
}

void PIF_WEAK pif_ChangeStatusLed(int num, BOOL state)
{
	(void)num;
	(void)state;
}

void PIF_WEAK pif_ToggleStatusLed(int num)
{
	(void)num;
}

void pif_ClearError()
{
	pif_error = E_SUCCESS;
}

int pif_BinToString(char* p_buffer, uint32_t value, uint16_t str_cnt)
{
	int i, idx = 0;
	BOOL first;
    uint32_t tmp_val;

    if (str_cnt) {
    	for (i = str_cnt - 1; i >= 0; i--) {
    		p_buffer[idx++] = '0' + ((value >> i) & 1);
    	}
    }
    else if (value > 0) {
    	first = TRUE;
    	for (i = 31; i >= 0; i--) {
    		tmp_val = (value >> i) & 1;
    		if (!first || tmp_val) {
    			p_buffer[idx++] = '0' + tmp_val;
    			first = FALSE;
    		}
    	}
    }
    else {
    	p_buffer[idx++] = '0';
    }
    return idx;
}

int pif_DecToString(char* p_buffer, uint32_t value, uint16_t str_cnt)
{
    uint16_t exp_cnt = 0;
    uint16_t zero_str_cnt = 0;
    int idx = 0;
    uint32_t idx_inv = 0;
    uint32_t tmp_val;
    char inv_buf[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

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
        }

        if ((str_cnt != 0) && (exp_cnt < str_cnt)) {
            zero_str_cnt = str_cnt - exp_cnt;
            while (zero_str_cnt) {
            	p_buffer[idx++] = '0';
                zero_str_cnt--;
            }
        }
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
        }
        while (zero_str_cnt);
    }
    return idx;
}

int pif_HexToString(char* p_buffer, uint32_t value, uint16_t str_cnt, BOOL upper)
{
	int i, idx = 0;
	BOOL first;
    uint32_t tmp_val;
    const char* kHexChar = upper ? kPifHexUpperChar : kPifHexLowerChar;

    if (str_cnt) {
    	for (i = (str_cnt - 1) * 4; i >= 0; i -= 4) {
    		tmp_val = (value >> i) & 0x0F;
    		p_buffer[idx++] = kHexChar[tmp_val];
    	}
    }
    else if (value > 0) {
    	first = TRUE;
    	for (i = 28; i >= 0; i -= 4) {
    		tmp_val = (value >> i) & 0x0F;
    		if (!first || tmp_val) {
    			p_buffer[idx++] = kHexChar[tmp_val];
    			first = FALSE;
    		}
    	}
    }
    else {
    	p_buffer[idx++] = '0';
    }
    return idx;
}

int pif_FloatToString(char* p_buffer, double value, uint16_t point)
{
	uint16_t i, idx = 0;
	uint32_t num;

	if (value < 0.0L) {
		p_buffer[idx++] = '-';
		value *= -1.0L;
	}

	num = (uint32_t)value;
	idx += pif_DecToString(p_buffer + idx, num, 0);
	p_buffer[idx++] = '.';

	if (point == 0) point = 6;
	value -= num;
	for (i = 0; i < point; i++) value *= 10;

	idx += pif_DecToString(p_buffer + idx, (uint32_t)value, point);
    return idx;
}

int pif_StrNCaseCmp(const char* p_str1, const char* p_str2, size_t n)
{
	int ch1, ch2;

	for (; n; n--, p_str1++, p_str2++) {
		ch1 = _toLower((unsigned char)*p_str1);
		ch2 = _toLower((unsigned char)*p_str2);
		if (ch1 != ch2 || !ch1) return ch1 - ch2;
	}
	return 0;
}

int pif_StrCaseCmp(const char* p_str1, const char* p_str2)
{
	return pif_StrNCaseCmp(p_str1, p_str2, (size_t)-1);
}

void pif_PrintFormat(char* p_buffer, size_t buffer_size, va_list* p_data, const char* p_format)
{
	int int_val;
	long long_val;
	uint16_t num_str_cnt;
	uint16_t max_str_cnt;
	BOOL is_long;
	BOOL is_max;
	char *p_var_str;
	size_t offset = 0;
	size_t size;
	size_t pad;
	char num_buf[33];

	while (*p_format && offset < buffer_size - 1) {
        if (*p_format == '%') {
            num_str_cnt = 0;
            max_str_cnt = 0;
        	is_long = FALSE;
        	is_max = FALSE;
NEXT_STR:
			p_format = p_format + 1;
            switch(*p_format) {
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
                	if (is_max) {
                		max_str_cnt *= 10;
                		max_str_cnt += *p_format - '0';
                	}
                	else {
                		num_str_cnt *= 10;
                		num_str_cnt += *p_format - '0';
                	}
                    goto NEXT_STR;

                // The number after it is the most characters of 's' that are written.
                case '.':
					is_max = TRUE;
					goto NEXT_STR;

                case 'l':
					is_long = TRUE;
					goto NEXT_STR;

                // Only the hex formats fill the width with '0'; 'b', 'd' and 'u' fill it with spaces.
                case 'b':
                	if (is_long) {
						size = pif_BinToString(num_buf, va_arg(*p_data, unsigned long), 0);
                	}
                	else {
						size = pif_BinToString(num_buf, va_arg(*p_data, unsigned int), 0);
                	}
					offset = _putSpacePadded(p_buffer, buffer_size, offset, num_buf, size, num_str_cnt);
                    break;

                case 'd':
					size = 0;
                	if (is_long) {
            			long_val = va_arg(*p_data, long);
            			if (long_val < 0) {
            				num_buf[size++] = '-';
            				long_val *= -1;
            			}
            			size += pif_DecToString(num_buf + size, long_val, 0);
                	}
                	else {
            			int_val = va_arg(*p_data, int);
            			if (int_val < 0) {
            				num_buf[size++] = '-';
                			int_val *= -1;
            			}
            			size += pif_DecToString(num_buf + size, int_val, 0);
                	}
					offset = _putSpacePadded(p_buffer, buffer_size, offset, num_buf, size, num_str_cnt);
                    break;

                case 'u':
                	if (is_long) {
						size = pif_DecToString(num_buf, va_arg(*p_data, unsigned long), 0);
                	}
                	else {
						size = pif_DecToString(num_buf, va_arg(*p_data, unsigned int), 0);
                	}
					offset = _putSpacePadded(p_buffer, buffer_size, offset, num_buf, size, num_str_cnt);
                    break;

                case 'x':
                	if (is_long) {
    					if (offset + (num_str_cnt ? num_str_cnt : 8) < buffer_size - 1) {
    						offset += pif_HexToString(p_buffer + offset, va_arg(*p_data, unsigned long), num_str_cnt, FALSE);
                        }
                	}
                	else {
    					if (offset + (num_str_cnt ? num_str_cnt : 4) < buffer_size - 1) {
    						offset += pif_HexToString(p_buffer + offset, va_arg(*p_data, unsigned int), num_str_cnt, FALSE);
                        }
                	}
                    break;

                case 'X':
                	if (is_long) {
    					if (offset + (num_str_cnt ? num_str_cnt : 8) < buffer_size - 1) {
                    		offset += pif_HexToString(p_buffer + offset, va_arg(*p_data, unsigned long), num_str_cnt, TRUE);
                        }
                	}
                	else {
    					if (offset + (num_str_cnt ? num_str_cnt : 4) < buffer_size - 1) {
                    		offset += pif_HexToString(p_buffer + offset, va_arg(*p_data, unsigned int), num_str_cnt, TRUE);
                        }
                	}
                    break;

                case 'f':
#ifndef PIF_USE_PRINTF_FLOAT
					// The argument is still taken so that the ones after it stay in place.
					(void)va_arg(*p_data, double);
#else
					{
						double float_val = va_arg(*p_data, double);

						if (offset + (num_str_cnt ? num_str_cnt + 12 : 20) < buffer_size - 1) {
							offset += pif_FloatToString(p_buffer + offset, float_val, num_str_cnt);
						}
					}
#endif
                    break;

                case 's':
                    p_var_str = va_arg(*p_data, char *);
                    if (!p_var_str) p_var_str = "";
					size = strlen(p_var_str);
					if (is_max && size > max_str_cnt) size = max_str_cnt;
					pad = size < num_str_cnt ? num_str_cnt - size : 0;
					// A string is left-aligned in its width and, unlike a number, cut where the buffer ends.
					if (size > buffer_size - 1 - offset) size = buffer_size - 1 - offset;
					memcpy(p_buffer + offset, p_var_str, size);
					offset += size;
					if (pad > buffer_size - 1 - offset) pad = buffer_size - 1 - offset;
					memset(p_buffer + offset, ' ', pad);
					offset += pad;
                    break;

                case 'c':
					if (offset + 1 < buffer_size - 1) {
    					p_buffer[offset++] = va_arg(*p_data, int);
                    }
                    break;

                case '%':
					if (offset + 1 < buffer_size - 1) {
                    	p_buffer[offset++] = '%';
                    }
                    break;
            }
        }
        else {
			if (offset + 1 < buffer_size - 1) {
            	p_buffer[offset++] = *p_format;
            }
        }
        p_format = p_format + 1;
	}
	p_buffer[offset] = 0;
}

void pif_Printf(char* p_buffer, size_t buffer_size, const char* p_format, ...)
{
	va_list data;

	va_start(data, p_format);
	pif_PrintFormat(p_buffer, buffer_size, &data, p_format);
	va_end(data);
}

uint8_t pifCrc7_Add(uint8_t crc, uint8_t data)
{
    uint16_t i;

	for (i = 0; i < 8; i++) {
		crc <<= 1;
		if ((data & 0x80) ^ (crc & 0x80)) crc ^=0x09;
		data <<= 1;
	}
    return crc;
}

uint8_t pifCrc7_Result(uint8_t crc)
{
	return (crc << 1) | 1;
}

uint8_t pifCrc7(uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint8_t crc = 0;

	for (i = 0; i < length; i++) {
		crc = pifCrc7_Add(crc, p_data[i]);
	}
	return pifCrc7_Result(crc);
}

uint16_t pifCrc16_Add(uint16_t crc, uint8_t data)
{
	uint16_t i;

    crc ^= (uint16_t)data << 8;
    for (i = 0; i < 8; i++) {
        if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
        else crc <<= 1;
    }
	return crc;
}

uint16_t pifCrc16(uint8_t* p_data, uint16_t length)
{
	uint16_t i, crc = 0;

	for (i = 0; i < length; i++) {
		crc = pifCrc16_Add(crc, p_data[i]);
	}
	return crc;
}

uint16_t pifCrc16_Update(uint16_t crc, const uint8_t* p_data, uint16_t length)
{
	uint16_t i;

	for (i = 0; i < length; i++) {
		crc = pifCrc16_Add(crc, p_data[i]);
	}
	return crc;
}

#ifdef PIF_USE_CRC8_SMBUS_TABLE

// CRC-8 of PIF_CRC8_POLY_SMBUS after each byte, for pifCrc8_Add() and pifCrc8_Update() with that polynomial.
const uint8_t pif_crc8_smbus_table[256] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

#endif

uint8_t pifCrc8_Add(uint8_t crc, uint8_t data, uint8_t poly)
{
	uint8_t i;

#ifdef PIF_USE_CRC8_SMBUS_TABLE
	if (poly == PIF_CRC8_POLY_SMBUS) return pif_crc8_smbus_table[crc ^ data];
#endif
	crc ^= data;
	for (i = 0; i < 8; i++) {
		if (crc & 0x80) crc = (uint8_t)(crc << 1) ^ poly;
		else crc <<= 1;
	}
	return crc;
}

uint8_t pifCrc8_Update(uint8_t crc, const uint8_t* p_data, uint16_t length, uint8_t poly)
{
	uint16_t i;

#ifdef PIF_USE_CRC8_SMBUS_TABLE
	// The table only holds this polynomial; the others go bit by bit.
	if (poly == PIF_CRC8_POLY_SMBUS) {
		while (length--) crc = pif_crc8_smbus_table[crc ^ *p_data++];
		return crc;
	}
#endif
	for (i = 0; i < length; i++) {
		crc = pifCrc8_Add(crc, p_data[i], poly);
	}
	return crc;
}

uint8_t pifCrc8(const uint8_t* p_data, uint16_t length, uint8_t poly)
{
	return pifCrc8_Update(0, p_data, length, poly);
}

// CRC-32 of each 4-bit value: a 64 byte table and two lookups per byte, between the bitwise loop and the
// 1 KB byte table in speed and size.
static const uint32_t kCrc32Nibble[16] = {
	0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL, 0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
	0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL, 0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

uint32_t PIF_WEAK pifCrc32_Update(uint32_t crc, const uint8_t* p_data, uint32_t length)
{
	crc = ~crc;
	while (length--) {
		crc ^= *p_data++;
		crc = (crc >> 4) ^ kCrc32Nibble[crc & 0x0F];
		crc = (crc >> 4) ^ kCrc32Nibble[crc & 0x0F];
	}
	return ~crc;
}

uint32_t pifCrc32(const uint8_t* p_data, uint32_t length)
{
	return pifCrc32_Update(0, p_data, length);
}

uint32_t pifCheckSum(uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint32_t sum = 0UL;

	for (i = 0; i < length; i++) {
		sum += p_data[i];
	}
	return sum;
}

uint8_t pifCheckXor(uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint8_t val = 0;

	for (i = 0; i < length; i++) {
		val ^= p_data[i];
	}
	return val;
}

uint8_t pifCheckXor_Update(uint8_t val, const uint8_t* p_data, uint16_t length)
{
	uint16_t i;

	for (i = 0; i < length; i++) {
		val ^= p_data[i];
	}
	return val;
}
