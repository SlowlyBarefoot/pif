#include <string.h>

#include "pif.h"
#ifndef	__PIF_NO_LOG__
	#include "pif_log.h"
#endif
#include "pif_task.h"


PifError pif_error = E_SUCCESS;

volatile uint16_t pif_timer1ms = 0;
volatile uint32_t pif_timer1sec = 0L;
volatile PifDateTime pif_datetime;

volatile uint32_t pif_cumulative_timer1ms = 0L;

PifPerformance pif_performance = {
		._count = 0,
		.__state = FALSE,
#ifdef __PIF_DEBUG__
#ifndef __PIF_NO_LOG__
		.__max_loop_time1us = 0UL
#endif
#endif
};

PifId pif_id = 1;

PifActTimer1us pif_act_timer1us = NULL;

PifActGpioRead pif_act_gpio_read = NULL;
PifActGpioWrite pif_act_gpio_write = NULL;

const char* kPifMonth3[12] = { "Jan", "Fab", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char* kPifHexUpperChar = "0123456789ABCDEF";
const char* kPifHexLowerChar = "0123456789abcdef";

const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static uint8_t s_crc7;


#ifdef __PIF_COLLECT_SIGNAL__

void PIF_WEAK pifGpioColSig_Init() {}
void PIF_WEAK pifGpioColSig_Clear() {}

void PIF_WEAK pifPulseColSig_Init() {}
void PIF_WEAK pifPulseColSig_Clear() {}

void PIF_WEAK pifSensorDigitalColSig_Init() {}
void PIF_WEAK pifSensorDigitalColSig_Clear() {}

void PIF_WEAK pifSensorSwitchColSig_Init() {}
void PIF_WEAK pifSensorSwitchColSig_Clear() {}

void PIF_WEAK pifSolenoidColSig_Init() {}
void PIF_WEAK pifSolenoidColSig_Clear() {}

void PIF_WEAK pifSequenceColSig_Init() {}
void PIF_WEAK pifSequenceColSig_Clear() {}

#endif

void pif_Init(PifActTimer1us act_timer1us)
{
	pif_act_timer1us = act_timer1us;

	pif_datetime.month = 1;
	pif_datetime.day = 1;

#ifdef __PIF_COLLECT_SIGNAL__
    pifGpioColSig_Init();
    pifPulseColSig_Init();
    pifSensorDigitalColSig_Init();
    pifSensorSwitchColSig_Init();
    pifSolenoidColSig_Init();
    pifSequenceColSig_Init();
#endif
}

void pif_Exit()
{
	extern void pifTaskManager_Clear();

	pifTaskManager_Clear();

#ifdef __PIF_COLLECT_SIGNAL__
	pifGpioColSig_Clear();
	pifPulseColSig_Clear();
	pifSensorDigitalColSig_Clear();
	pifSensorSwitchColSig_Clear();
	pifSolenoidColSig_Clear();
    pifSequenceColSig_Clear();
#endif
}

void pif_Loop()
{
    extern void pifTaskManager_Loop();

#ifndef __PIF_NO_LOG__
#ifdef __PIF_DEBUG__
	static BOOL first = TRUE;
	static uint32_t pretime;
	uint32_t gap;
#endif

	if (pif_log_flag.bt.performance) {
		pif_performance._count++;
		if (pif_performance.__state) {
        	uint32_t value = 1000000L / pif_performance._count;
        	pifLog_Printf(LT_INFO, "Performance: %lur/s, %uns", pif_performance._count, value);
        	pif_performance._count = 0;
    		pif_performance.__state = FALSE;
        }
    }
#ifdef __PIF_DEBUG__
    else {
    	if (pif_act_timer1us) {
    		if (!first) {
    			gap = (*pif_act_timer1us)() - pretime;
    			if (gap > pif_performance.__max_loop_time1us) {
    				pif_performance.__max_loop_time1us = gap;
    				pifLog_Printf(LT_NONE, "\nMLT: %luus", pif_performance.__max_loop_time1us);
    			}
    		}
    		else {
    			first = FALSE;
    		}
    		pretime = (*pif_act_timer1us)();
    	}
	}
#endif
#endif

    pifTaskManager_Loop();
}

void pif_sigTimer1ms()
{
	uint8_t days;
	uint16_t year;
#ifndef __PIF_NO_LOG__
	static uint16_t usTimerPerform = 0;

    if (pif_log_flag.bt.performance) {
		usTimerPerform++;
		if (usTimerPerform >= 1000) {
			usTimerPerform = 0;
			pif_performance.__state = TRUE;
		}
    }
#endif

	pif_cumulative_timer1ms++;
    pif_timer1ms++;
    if (pif_timer1ms >= 1000) {
        pif_timer1ms = 0;

        pif_timer1sec++;
    	pif_datetime.second++;
    	if (pif_datetime.second >= 60) {
#ifdef __PIF_DEBUG__
#ifndef __PIF_NO_LOG__
    		pif_performance.__max_loop_time1us = 0UL;
#endif
#endif
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
    					if (year / 4 == 0) {
    						if (year / 100 == 0) {
    							if (year / 400 == 0) days++;
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
    	}
    }
}

void pif_Delay1ms(uint16_t delay)
{
	uint32_t start;
	uint16_t diff;

	start = pif_cumulative_timer1ms;
	do {
		diff = pif_cumulative_timer1ms - start;
	} while (diff < delay);
}

void pif_Delay1us(uint16_t delay)
{
	uint32_t start;
	uint16_t diff;

	start = (*pif_act_timer1us)();
	do {
		diff = (*pif_act_timer1us)() - start;
	} while (diff < delay);
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

	if (value < 0.0) {
		p_buffer[idx++] = '-';
		value *= -1.0;
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

void pif_PrintFormat(char* p_buffer, va_list* p_data, const char* p_format)
{
	unsigned int uint_val;
	int int_val;
	unsigned long ulong_val;
	long long_val;
	uint16_t num_str_cnt;
	BOOL is_long;
	char *p_var_str;
	int offset = 0;
	size_t size;

	while (*p_format) {
        if (*p_format == '%') {
            num_str_cnt = 0;
        	is_long = FALSE;
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
                    num_str_cnt *= 10;
                    num_str_cnt += *p_format - '0';
                    goto NEXT_STR;

                case 'l':
					is_long = TRUE;
					goto NEXT_STR;

                case 'b':
                	if (is_long) {
                		ulong_val = va_arg(*p_data, unsigned long);
						offset += pif_BinToString(p_buffer + offset, ulong_val, num_str_cnt);
                	}
                	else {
						uint_val = va_arg(*p_data, unsigned int);
						offset += pif_BinToString(p_buffer + offset, uint_val, num_str_cnt);
                	}
                    break;

                case 'd':
                case 'i':
                	if (is_long) {
            			long_val = va_arg(*p_data, long);
            			if (long_val < 0) {
            				p_buffer[offset++] = '-';
            				long_val *= -1;
            				if (num_str_cnt) num_str_cnt--;
            			}
            			offset += pif_DecToString(p_buffer + offset, long_val, num_str_cnt);
                	}
                	else {
            			int_val = va_arg(*p_data, int);
            			if (int_val < 0) {
            				p_buffer[offset++] = '-';
                			int_val *= -1;
                			if (num_str_cnt) num_str_cnt--;
            			}
            			offset += pif_DecToString(p_buffer + offset, int_val, num_str_cnt);
                	}
                    break;

                case 'u':
                	if (is_long) {
						ulong_val = va_arg(*p_data, unsigned long);
						offset += pif_DecToString(p_buffer + offset, ulong_val, num_str_cnt);
                	}
                	else {
						uint_val = va_arg(*p_data, unsigned int);
						offset += pif_DecToString(p_buffer + offset, uint_val, num_str_cnt);
                	}
                    break;

                case 'x':
                	if (is_long) {
                		ulong_val = va_arg(*p_data, unsigned long);
						offset += pif_HexToString(p_buffer + offset, ulong_val, num_str_cnt, FALSE);
                	}
                	else {
						uint_val = va_arg(*p_data, unsigned int);
						offset += pif_HexToString(p_buffer + offset, uint_val, num_str_cnt, FALSE);
                	}
                    break;

                case 'X':
                	if (is_long) {
                		ulong_val = va_arg(*p_data, unsigned long);
                		offset += pif_HexToString(p_buffer + offset, ulong_val, num_str_cnt, TRUE);
                	}
                	else {
                		uint_val = va_arg(*p_data, unsigned int);
                		offset += pif_HexToString(p_buffer + offset, uint_val, num_str_cnt, TRUE);
                	}
                    break;

                case 'f':
					offset += pif_FloatToString(p_buffer + offset, va_arg(*p_data, double), num_str_cnt);
                    break;

                case 's':
                    p_var_str = va_arg(*p_data, char *);
                    if (p_var_str) {
						size = strlen(p_var_str);
						if (offset + size < PIF_LOG_LINE_SIZE - 1) {
							strcpy(p_buffer + offset, p_var_str);
						}
						else {
							size = PIF_LOG_LINE_SIZE - 1 - offset;
							strncpy(p_buffer + offset, p_var_str, size);
						}
						offset += size;
                    }
                    break;

                case 'c':
                	p_buffer[offset++] = va_arg(*p_data, int);
                    break;

                case '%':
                	p_buffer[offset++] = '%';
                    break;
            }
        }
        else {
        	p_buffer[offset++] = *p_format;
        }
        p_format = p_format + 1;
	}
	p_buffer[offset] = 0;
}

void pif_Printf(char* p_buffer, const char* p_format, ...)
{
	va_list data;

	va_start(data, p_format);
	pif_PrintFormat(p_buffer, &data, p_format);
	va_end(data);
}

void pifCrc7_Init()
{
	s_crc7 = 0;
}

void pifCrc7_Calcurate(uint8_t data)
{
	for (int i = 0; i < 8; i++) {
		s_crc7 <<= 1;
		if ((data & 0x80) ^ (s_crc7 & 0x80)) s_crc7 ^=0x09;
		data <<= 1;
	}
}

uint8_t pifCrc7_Result()
{
	s_crc7 = (s_crc7 << 1) | 1;
	return s_crc7;
}

uint16_t pifCrc16(uint8_t* p_data, uint16_t length)
{
	uint16_t i, n;
	uint32_t crc16 = 0;
	uint32_t temp;

	for (i = 0; i < length; i++) {
		crc16 ^= (uint16_t)p_data[i] << 8;
		for (n = 0; n < 8; n++) {
			temp = crc16 << 1;
			if (crc16 & 0x8000) temp ^= 0x1021;
			crc16 = temp;
		}
	}
	return crc16 & 0xFFFF;
}

uint8_t pifCheckSum(uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint8_t sum = 0;

	for (i = 0; i < length; i++) {
		sum += p_data[i];
	}
	return sum & 0xFF;
}

uint8_t pifCheckXor(uint8_t* p_data, uint16_t length)
{
	uint16_t i;
	uint8_t xor = 0;

	for (i = 0; i < length; i++) {
		xor ^= p_data[i];
	}
	return xor & 0xFF;
}

void pifPidControl_Init(PifPidControl* p_owner, float kp, float ki, float kd, float max_integration)
{
	p_owner->kp = kp;
	p_owner->ki = ki;
	p_owner->kd = kd;
	p_owner->max_integration = max_integration;
	p_owner->err_sum = 0;
	p_owner->err_prev = 0;
}

float pifPidControl_Calcurate(PifPidControl* p_owner, float err)
{
	float up;			// Variable: Proportional output
	float ui;			// Variable: Integral output
	float ud;			// Variable: Derivative output
	float ed;
	float out;   		// Output: PID output

	// Compute the error sum
	p_owner->err_sum += err;

	// Compute the proportional output
	up = p_owner->kp * err;

	// Compute the integral output
	ui = p_owner->ki * p_owner->err_sum;
	if (ui > p_owner->max_integration) 					ui = p_owner->max_integration;
	else if (ui < (-1.0 * p_owner->max_integration))	ui = -1.0 * p_owner->max_integration;

	// Compute the derivative output
	ed = err - p_owner->err_prev;
	ud = ((err > 0 && ed > 0) || (err < 0 && ed < 0)) ? p_owner->kd * ed : 0;

	// Compute the pre-saturated output
	out = up + ui + ud;

	p_owner->err_prev = err;

	return out;
}
