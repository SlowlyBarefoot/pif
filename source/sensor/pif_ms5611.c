#include "core/pif_task.h"
#include "sensor/pif_ms5611.h"

#include <math.h>


static BOOL _checkPromCrc(PifMs5611* p_owner)
{
    int32_t i, j;
    uint32_t res = 0;
    uint8_t zero = 1;
    uint8_t crc = p_owner->_prom[7] & 0xF;
    p_owner->_prom[7] &= 0xFF00;

    for (i = 0; i < 8; i++) {
        if (p_owner->_prom[i] != 0)
            zero = 0;
    }
    if (zero)
        return FALSE;

    for (i = 0; i < 16; i++) {
        if (i & 1)
            res ^= ((p_owner->_prom[i >> 1]) & 0x00FF);
        else
            res ^= (p_owner->_prom[i >> 1] >> 8);
        for (j = 8; j > 0; j--) {
            if (res & 0x8000)
                res ^= 0x1800;
            res <<= 1;
        }
    }
    p_owner->_prom[7] |= crc;
    if (crc == ((res >> 12) & 0xF))
        return TRUE;

    return FALSE;
}

static void _calcurateBarometric(PifMs5611* p_owner, float* p_pressure, float* p_temperature)
{
	int32_t dT;
	int64_t temp;
	int64_t off, sens;
	int64_t delt;

	dT = (int64_t)p_owner->__D2 - ((int64_t)p_owner->_prom[5] * 256);
	temp = 2000 + ((dT * (int64_t)p_owner->_prom[6]) >> 23);

	off = ((int64_t)p_owner->_prom[2] << 16) + (((int64_t)p_owner->_prom[4] * dT) >> 7);
	sens = ((int64_t)p_owner->_prom[1] << 15) + (((int64_t)p_owner->_prom[3] * dT) >> 8);

	if (temp < 2000) {
		temp -= ((int64_t)dT * dT) / 2147483648L;

		delt = temp - 2000;
		delt = 5 * delt * delt;
		off -= delt >> 1;
		sens -= delt >> 2;

		if (temp < -1500) {
			delt = temp + 1500;
			delt = delt * delt;
			off -= 7 * delt;
			sens -= (11 * delt) >> 1;
		}
	}

    *p_temperature = temp / 100.0;
	*p_pressure = (float)(((((int64_t)p_owner->__D1 * sens) >> 21) - off) >> 15);
}

static uint16_t _doTask(PifTask* p_task)
{
	PifMs5611* p_owner = p_task->_p_client;
	uint8_t value[3];
	uint16_t delay = 1;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->__state) {
	case MS5611_STATE_TEMPERATURE_START:
		p_owner->__start_time = pif_cumulative_timer1ms;
		value[0] = MS5611_REG_CONV_D2 + p_owner->_over_sampling_rate;
		if (pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, value, 1)) {
			p_owner->__state = MS5611_STATE_TEMPERATURE_WAIT;
			delay = p_owner->_conversion_time;
		}
		break;

	case MS5611_STATE_TEMPERATURE_WAIT:
		if (pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MS5611_REG_ADC_READ, value, 3)) {
			p_owner->__D2 = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
			p_owner->__state = MS5611_STATE_PRESSURE_START;
		}
		break;

	case MS5611_STATE_PRESSURE_START:
		value[0] = MS5611_REG_CONV_D1 + p_owner->_over_sampling_rate;
		if (pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, value, 1)) {
			p_owner->__state = MS5611_STATE_PRESSURE_WAIT;
			delay = p_owner->_conversion_time;
		}
		break;

	case MS5611_STATE_PRESSURE_WAIT:
		if (pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MS5611_REG_ADC_READ, value, 3)) {
			p_owner->__D1 = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
			p_owner->__state = MS5611_STATE_CALCURATE;
		}
		break;

	case MS5611_STATE_CALCURATE:
		_calcurateBarometric(p_owner, &pressure, &temperature);
		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);
		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = p_owner->__read_period - gap;
		}
		p_owner->__state = MS5611_STATE_TEMPERATURE_START;
		break;

	default:
		break;
	}
	return delay;
}

BOOL pifMs5611_Init(PifMs5611* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr)
{
	int i;

	if (!p_owner || !p_i2c) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMs5611));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->_p_i2c->addr = addr;

	if (!pifI2cDevice_WriteRegByte(p_owner->_p_i2c, MS5611_REG_RESET, 0)) goto fail;
	pifTaskManager_YieldMs(100);

	for (i = 0; i < 8; i++) {
		if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, MS5611_REG_READ_PROM + i * 2, (uint16_t*)&p_owner->_prom[i])) goto fail;
	}
	if (!_checkPromCrc(p_owner)) goto fail;

    pifMs5611_SetOverSamplingRate(p_owner, MS5611_OSR_1024);

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifMs5611_Clear(p_owner);
	return FALSE;
}

void pifMs5611_Clear(PifMs5611* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->__p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
}

void pifMs5611_SetOverSamplingRate(PifMs5611* p_owner, uint16_t osr)
{
	p_owner->_over_sampling_rate = osr;
	switch (osr) {
	case MS5611_OSR_256: p_owner->_conversion_time = 1 + 1; break;
	case MS5611_OSR_512: p_owner->_conversion_time = 2 + 1; break;
	case MS5611_OSR_1024: p_owner->_conversion_time = 3 + 1; break;
	case MS5611_OSR_2048: p_owner->_conversion_time = 5 + 1; break;
	case MS5611_OSR_4096: p_owner->_conversion_time = 10 + 1; break;
	}
}

BOOL pifMs5611_ReadRawTemperature(PifMs5611* p_owner, uint32_t* p_data)
{
	uint8_t value[3];

	value[0] = MS5611_REG_CONV_D2 + p_owner->_over_sampling_rate;
	if (!pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, value, 1)) return FALSE;

	pifTaskManager_YieldMs(p_owner->_conversion_time);

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MS5611_REG_ADC_READ, value, 3)) return FALSE;
	*p_data = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
	return TRUE;
}

BOOL pifMs5611_ReadRawPressure(PifMs5611* p_owner, uint32_t* p_data)
{
	uint8_t value[3];

	value[0] = MS5611_REG_CONV_D1 + p_owner->_over_sampling_rate;
	if (!pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, value, 1)) return FALSE;

	pifTaskManager_YieldMs(p_owner->_conversion_time);

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MS5611_REG_ADC_READ, value, 3)) return FALSE;
	*p_data = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
	return TRUE;
}

BOOL pifMs5611_ReadBarometric(PifMs5611* p_owner, float* p_pressure, float* p_temperature)
{
	if (!pifMs5611_ReadRawTemperature(p_owner, &p_owner->__D2)) return FALSE;
	if (!pifMs5611_ReadRawPressure(p_owner, &p_owner->__D1)) return FALSE;
	_calcurateBarometric(p_owner, p_pressure, p_temperature);
	return TRUE;
}

BOOL pifMs5611_AddTaskForReading(PifMs5611* p_owner, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(TM_CHANGE_MS, read_period, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = MS5611_STATE_TEMPERATURE_START;
    return TRUE;
}
