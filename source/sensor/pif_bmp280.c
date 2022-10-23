#include "core/pif_task.h"
#include "sensor/pif_bmp280.h"

#include <math.h>


// Returns temperature in DegC, float precision. Output value of 51.23 equals 51.23 DegC.
// t_fine carries fine temperature as global value
static float _compensate_T(PifBmp280* p_owner, int32_t adc_T)
{
    float var1, var2, T;
	PifBmp280CalibParam* p_calib_param = &p_owner->__calib_param;

    var1 = (((float)adc_T) / 16384.0f - ((float)p_calib_param->dig_T1) / 1024.0f) * ((float)p_calib_param->dig_T2);
    var2 = ((((float)adc_T) / 131072.0f - ((float)p_calib_param->dig_T1) / 8192.0f) * (((float)adc_T) / 131072.0f - ((float)p_calib_param->dig_T1) / 8192.0f)) * ((float)p_calib_param->dig_T3);
    p_calib_param->t_fine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0f;

    return T;
}

// Returns pressure in Pa as float. Output value of 96386.2 equals 96386.2 Pa = 963.862 hPa
static float _compensate_P(PifBmp280* p_owner, int32_t adc_P)
{
    float var1, var2, p;
	PifBmp280CalibParam* p_calib_param = &p_owner->__calib_param;

    var1 = ((float)p_calib_param->t_fine / 2.0f) - 64000.0f;
    var2 = var1 * var1 * ((float)p_calib_param->dig_P6) / 32768.0f;
    var2 = var2 + var1 * ((float)p_calib_param->dig_P5) * 2.0f;
    var2 = (var2 / 4.0f) + (((float)p_calib_param->dig_P4) * 65536.0f);
    var1 = (((float)p_calib_param->dig_P3) * var1 * var1 / 524288.0f + ((float)p_calib_param->dig_P2) * var1) / 524288.0f;
    var1 = (1.0f + var1 / 32768.0f) * ((float)p_calib_param->dig_P1);
    if (var1 == 0.0f)
        return 0.0f; // avoid exception caused by division by zero

    p = 1048576.0f - (float)adc_P;
    p = (p - (var2 / 4096.0f)) * 6250.0f / var1;
    var1 = ((float)p_calib_param->dig_P9) * p * p / 2147483648.0f;
    var2 = p * ((float)p_calib_param->dig_P8) / 32768.0f;
    p = p + (var1 + var2 + ((float)p_calib_param->dig_P7)) / 16.0f;

    return p;
}

static uint16_t _doTask(PifTask* p_task)
{
	PifBmp280* p_owner = p_task->_p_client;
	PifBmp280CtrlMeas ctrl_meas;
	uint8_t data[6];
	uint16_t delay = 4;
	int32_t pressure;
	float temperature;
	static uint32_t start_time;
	static int32_t raw_pressure, raw_temperature;

	switch (p_owner->__state) {
	case BMP280_STATE_START:
		start_time = pif_cumulative_timer1ms;
		ctrl_meas.bit.mode = BMP280_MODE_FORCED;
		ctrl_meas.bit.osrs_p = p_owner->_osrs_p;
		ctrl_meas.bit.osrs_t = p_owner->_osrs_t;
		if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, ctrl_meas.byte)) break;
		p_owner->__state = BMP280_STATE_WAIT;
		break;

	case BMP280_STATE_WAIT:
		if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, BMP280_REG_STATUS, data)) break;
		if (data[0] & 8) {
			p_owner->__state = BMP280_STATE_READ;
			delay = 0;
		}
		break;

	case BMP280_STATE_READ:
		if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, BMP280_REG_PRESS_MSB, data, 6)) break;

		raw_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
		raw_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
		p_owner->__state = BMP280_STATE_CALCURATE;
		delay = 0;
		break;

	case BMP280_STATE_CALCURATE:
		temperature = _compensate_T(p_owner, raw_temperature);
		pressure = (int32_t)_compensate_P(p_owner, raw_pressure);

		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		if (pif_cumulative_timer1ms - start_time < p_owner->__read_period) {
			delay = p_owner->__read_period - (pif_cumulative_timer1ms - start_time);
		}
		p_owner->__state = BMP280_STATE_START;
		break;

	default:
		break;
	}
	return delay;
}

BOOL pifBmp280_Init(PifBmp280* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr)
{
	uint8_t data;
	PifBmp280CtrlMeas ctrl_meas;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBmp280));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_p_i2c->addr = addr;

	if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, BMP280_REG_ID, &data)) goto fail;
	if (data != 0x58) {
		pif_error = E_INVALID_ID;
		goto fail;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, BMP280_REG_CALIB, (uint8_t*)&p_owner->__calib_param, 24)) goto fail;

    if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, &ctrl_meas.byte)) goto fail;
	pifBmp280_SetOverSamplingRate(p_owner, ctrl_meas.bit.osrs_p, ctrl_meas.bit.osrs_t);
    return TRUE;

fail:
	pifBmp280_Clear(p_owner);
	return FALSE;
}

void pifBmp280_Clear(PifBmp280* p_owner)
{
	if (p_owner->__p_task) {
		pifTaskManager_Remove(p_owner->__p_task);
		p_owner->__p_task = NULL;
	}
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

void pifBmp280_SetOverSamplingRate(PifBmp280* p_owner, uint8_t osrs_p, uint8_t osrs_t)
{
	p_owner->_osrs_p = osrs_p;
	p_owner->_osrs_t = osrs_t;
}

BOOL pifBmp280_ReadRawData(PifBmp280* p_owner, int32_t* p_pressure, int32_t* p_temperature)
{
	uint8_t data[6];
	PifBmp280CtrlMeas ctrl_meas;

	ctrl_meas.bit.mode = BMP280_MODE_FORCED;
	ctrl_meas.bit.osrs_p = p_owner->_osrs_p;
	ctrl_meas.bit.osrs_t = p_owner->_osrs_t;
	if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, BMP280_REG_CTRL_MEAS, ctrl_meas.byte)) return FALSE;

	while (1) {
		pifTaskManager_YieldMs(4);

		if (!pifI2cDevice_ReadRegByte(p_owner->_p_i2c, BMP280_REG_STATUS, data)) return FALSE;
		if (data[0] & 8) break;
	}

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, BMP280_REG_PRESS_MSB, data, 6)) return FALSE;

    *p_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    *p_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
	return TRUE;
}

BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, int32_t* p_pressure, float* p_temperature)
{
    float t, p;
	int32_t pressure, temperature;

	if (!pifBmp280_ReadRawData(p_owner, &pressure, &temperature)) return FALSE;

    t = _compensate_T(p_owner, temperature);
    p = _compensate_P(p_owner, pressure);

    if (p_pressure)
        *p_pressure = (int32_t)p;
    if (p_temperature)
        *p_temperature = t;
	return TRUE;
}

BOOL pifBmp280_AddTaskForReading(PifBmp280* p_owner, uint16_t read_period, PifEvtBmp280Read evt_read)
{
	p_owner->__p_task = pifTaskManager_Add(TM_CHANGE_MS, read_period, _doTask, p_owner, TRUE);
    if (!p_owner->__p_task) return FALSE;

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = BMP280_STATE_START;
    return TRUE;
}

