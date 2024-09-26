#include "core/pif_task.h"
#include "sensor/pif_bmp280.h"

#include <math.h>


// Returns temperature in DegC, float precision. Output value of 51.23 equals 51.23 DegC.
// t_fine carries fine temperature as global value
static float _compensate_T(PifBmp280* p_owner, int32_t adc_T)
{
    int32_t var1, var2, T;
	PifBmp280CalibParam* p_calib_param = &p_owner->__calib_param;

    var1 = ((((adc_T >> 3) - ((int32_t)p_calib_param->dig_T1 << 1))) * ((int32_t)p_calib_param->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)p_calib_param->dig_T1)) * ((adc_T >> 4) - ((int32_t)p_calib_param->dig_T1))) >> 12) * ((int32_t)p_calib_param->dig_T3)) >> 14;
    p_calib_param->t_fine = var1 + var2;
    T = (p_calib_param->t_fine * 5 + 128) >> 8;

    return (float)T / 100.0f;
}

// Returns pressure in hPa as floating point.
static float _compensate_P(PifBmp280* p_owner, int32_t adc_P)
{
    int64_t var1, var2, p;
	PifBmp280CalibParam* p_calib_param = &p_owner->__calib_param;

    var1 = ((int64_t)p_calib_param->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)p_calib_param->dig_P6;
    var2 = var2 + ((var1 * (int64_t)p_calib_param->dig_P5) << 17);
    var2 = var2 + (((int64_t)p_calib_param->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)p_calib_param->dig_P3) >> 8) + ((var1 * (int64_t)p_calib_param->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)p_calib_param->dig_P1) >> 33;
    if (var1 == 0)
        return 0; // avoid exception caused by division by zero

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)p_calib_param->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)p_calib_param->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)p_calib_param->dig_P7) << 4);

    return (float)p / 25600.0f;
}

static uint16_t _doTask(PifTask* p_task)
{
	PifBmp280* p_owner = p_task->_p_client;
	PifBmp280CtrlMeas ctrl_meas;
	PifBmp280Status status;
	uint8_t data[6];
	uint16_t delay = 1;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->__state) {
	case BMP280_STATE_START:
		p_owner->__start_time = pif_cumulative_timer1ms;
		ctrl_meas.bit.mode = BMP280_MODE_FORCED;
		ctrl_meas.bit.osrs_p = p_owner->_osrs_p;
		ctrl_meas.bit.osrs_t = p_owner->_osrs_t;
		if ((p_owner->_fn.write_byte)(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, ctrl_meas.byte)) {
			delay = p_owner->__delay;
			p_owner->__state = BMP280_STATE_WAIT;
		}
		break;

	case BMP280_STATE_WAIT:
		if ((p_owner->_fn.read_byte)(p_owner->_p_i2c, BMP280_REG_STATUS, &status.byte)) {
			if (!status.bit.measuring) {
				p_owner->__state = BMP280_STATE_READ;
				pifTask_SetTrigger(p_task);
			}
		}
		break;

	case BMP280_STATE_READ:
		if ((p_owner->_fn.read_bytes)(p_owner->_p_i2c, BMP280_REG_PRESS_MSB, data, 6)) {
			p_owner->__raw_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
			p_owner->__raw_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
			p_owner->__state = BMP280_STATE_CALCURATE;
			pifTask_SetTrigger(p_task);
		}
		break;

	case BMP280_STATE_CALCURATE:
		temperature = _compensate_T(p_owner, p_owner->__raw_temperature);
		pressure = _compensate_P(p_owner, p_owner->__raw_pressure);

		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = p_owner->__read_period - gap;
		}
		else {
			pifTask_SetTrigger(p_task);
		}
		p_owner->__state = BMP280_STATE_START;
		break;

	default:
		break;
	}
	return delay;
}

BOOL pifBmp280_Config(PifBmp280* p_owner, PifId id)
{
	PifBmp280CtrlMeas ctrl_meas;

	if (!p_owner || !p_owner->_fn.read_byte || !p_owner->_fn.read_bytes || !p_owner->_fn.read_bit ||
			!p_owner->_fn.write_byte || !p_owner->_fn.write_bytes || !p_owner->_fn.write_bit) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

	if (!(p_owner->_fn.read_bytes)(p_owner->_p_i2c, BMP280_REG_CALIB, (uint8_t*)&p_owner->__calib_param, 24)) return FALSE;

    if (!(p_owner->_fn.read_byte)(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, &ctrl_meas.byte)) return FALSE;
	pifBmp280_SetOverSamplingRate(p_owner, ctrl_meas.bit.osrs_p, ctrl_meas.bit.osrs_t);

	if (!(p_owner->_fn.write_bit)(p_owner->_p_i2c, BMP280_REG_CONFIG, BMP280_CONFIG_FILTER, BMP280_FILTER_X16)) return FALSE;
    return TRUE;
}

void pifBmp280_SetOverSamplingRate(PifBmp280* p_owner, uint8_t osrs_p, uint8_t osrs_t)
{
	int i;

	p_owner->_osrs_p = osrs_p;
	p_owner->_osrs_t = osrs_t;
	p_owner->__delay = 6;		// 5.5ms =:= 6ms
	for (i = 1; i < osrs_p; i++) p_owner->__delay += 1 << osrs_p;
	for (i = 1; i < osrs_t; i++) p_owner->__delay += 1 << osrs_t;
}

BOOL pifBmp280_ReadRawData(PifBmp280* p_owner, int32_t* p_pressure, int32_t* p_temperature)
{
	uint8_t data[6];
	int i;
	PifBmp280CtrlMeas ctrl_meas;
	PifBmp280Status status;

	ctrl_meas.bit.mode = BMP280_MODE_FORCED;
	ctrl_meas.bit.osrs_p = p_owner->_osrs_p;
	ctrl_meas.bit.osrs_t = p_owner->_osrs_t;
	if (!(p_owner->_fn.write_byte)(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, ctrl_meas.byte)) return FALSE;

	for (i = 0; i < 10; i++) {
		pifTaskManager_YieldMs(4);

		if (!(p_owner->_fn.read_byte)(p_owner->_p_i2c, BMP280_REG_STATUS, &status.byte)) return FALSE;
		if (!status.bit.measuring) break;
	}
	if (i >= 10) return FALSE;

	if (!(p_owner->_fn.read_bytes)(p_owner->_p_i2c, BMP280_REG_PRESS_MSB, data, 6)) return FALSE;

    *p_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    *p_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
	return TRUE;
}

BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, float* p_pressure, float* p_temperature)
{
	int32_t pressure, temperature;

	if (!pifBmp280_ReadRawData(p_owner, &pressure, &temperature)) return FALSE;

    if (p_pressure) *p_pressure = _compensate_P(p_owner, pressure);
    if (p_temperature) *p_temperature = _compensate_T(p_owner, temperature);
	return TRUE;
}

BOOL pifBmp280_AddTaskForReading(PifBmp280* p_owner, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(TM_CHANGE_MS, read_period, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "BMP280";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = BMP280_STATE_START;
    return TRUE;
}

