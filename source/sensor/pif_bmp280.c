#include "sensor/pif_bmp280.h"


// How long to leave the device before asking again: after a transfer that failed, and between the
// reads of the status register while the measurement is still running.
#define BMP280_POLL_DELAY_US		1000UL

// Asked for as soon as the ring comes round again, which is what a step that needs no wait of its
// own wants. It is a period rather than pifTask_SetTrigger(), because a trigger is taken before
// the pause flag and before the check that the run fits in the time left before a TM_REALTIME
// release, and stepping a reading along is ordinary work that should be subject to both.
#define BMP280_NEXT_STEP_US			1UL

#include <math.h>


// Returns temperature in DegC, float precision. Output value of 51.23 equals 51.23 DegC.
// t_fine carries fine temperature as global value
/**
 * @fn _compensate_T
 * @brief Internal helper that supports compensate t logic.
 * @param p_owner Pointer to the owner instance.
 * @param adc_T Raw ADC temperature sample.
 * @return Computed floating-point value.
 */
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
/**
 * @fn _compensate_P
 * @brief Internal helper that supports compensate p logic.
 * @param p_owner Pointer to the owner instance.
 * @param adc_P Raw ADC pressure sample.
 * @return Computed floating-point value.
 */
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

/**
 * @fn _doTask
 * @brief Internal helper that supports do task logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifBmp280* p_owner = p_task->_p_client;
	uint8_t data[6];
	uint32_t delay = BMP280_POLL_DELAY_US;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->__state) {
	case BMP280_STATE_START:
		p_owner->__start_time = pif_cumulative_timer1ms;
		if (pifBmp280_StartMeasurement(p_owner)) {
			delay = p_owner->__delay * 1000UL;
			p_owner->__state = BMP280_STATE_WAIT;
		}
		break;

	case BMP280_STATE_WAIT:
		if ((p_owner->_fn.read_byte)(p_owner->_fn.p_device, BMP280_REG_STATUS, data)) {
			if (!(data[0] & BMP280_MEASURING_MASK)) {
				// Nothing more has to be left for: the sample is sitting in the device.
				p_owner->__state = BMP280_STATE_READ;
				delay = BMP280_NEXT_STEP_US;
			}
		}
		break;

	case BMP280_STATE_READ:
		if ((p_owner->_fn.read_bytes)(p_owner->_fn.p_device, BMP280_REG_PRESS_MSB, data, 6)) {
			p_owner->__raw_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
			p_owner->__raw_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
			p_owner->__state = BMP280_STATE_CALCURATE;
			delay = BMP280_NEXT_STEP_US;
		}
		break;

	case BMP280_STATE_CALCURATE:
		temperature = _compensate_T(p_owner, p_owner->__raw_temperature);
		pressure = _compensate_P(p_owner, p_owner->__raw_pressure);

		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		// What is left of the read period, so that the readings keep to their grid however long the
		// measurement took. A reading that already overran it starts the next one straight away.
		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = (p_owner->__read_period - gap) * 1000UL;
		}
		else {
			delay = BMP280_NEXT_STEP_US;
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
	uint8_t data;

	if (!p_owner || !p_owner->_fn.p_device
			|| !p_owner->_fn.read_byte || !p_owner->_fn.read_bytes || !p_owner->_fn.read_bit
			|| !p_owner->_fn.write_byte || !p_owner->_fn.write_bytes || !p_owner->_fn.write_bit) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;

	if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, BMP280_REG_CALIB, (uint8_t*)&p_owner->__calib_param, 24)) return FALSE;

    if (!(p_owner->_fn.read_byte)(p_owner->_fn.p_device, BMP280_REG_CTRL_MEAS, &data)) return FALSE;
	pifBmp280_SetOverSamplingRate(p_owner, data & BMP280_OSRS_P_MASK, data & BMP280_OSRS_T_MASK);

	if (!(p_owner->_fn.write_bit)(p_owner->_fn.p_device, BMP280_REG_CONFIG, BMP280_FILTER_MASK, BMP280_FILTER_X16)) return FALSE;
    return TRUE;
}

void pifBmp280_SetOverSamplingRate(PifBmp280* p_owner, uint8_t osrs_p, uint8_t osrs_t)
{
	uint8_t b;
	int i;

	p_owner->_osrs_p = osrs_p;
	p_owner->_osrs_t = osrs_t;
	p_owner->__delay = 6;		// 5.5ms =:= 6ms
	b = osrs_p >> 2;
	for (i = 1; i < b; i++) p_owner->__delay += 1 << b;
	b = osrs_t >> 5;
	for (i = 1; i < b; i++) p_owner->__delay += 1 << b;
}

BOOL pifBmp280_StartMeasurement(PifBmp280* p_owner)
{
	return (p_owner->_fn.write_byte)(p_owner->_fn.p_device, BMP280_REG_CTRL_MEAS,
			BMP280_MODE_FORCED | p_owner->_osrs_p | p_owner->_osrs_t);
}

BOOL pifBmp280_ReadRawData(PifBmp280* p_owner, int32_t* p_pressure, int32_t* p_temperature)
{
	uint8_t data[6];

	// Nothing is waited for. The device says whether the measurement has finished, so a caller that
	// finds it has not comes back on its next release instead of holding the CPU until it does.
	// That is also what keeps the wait from having to be estimated: the measurement takes as long
	// as it takes, and __delay is only how long to leave it before asking the first time.
	if (!(p_owner->_fn.read_byte)(p_owner->_fn.p_device, BMP280_REG_STATUS, data)) return FALSE;
	if (data[0] & BMP280_MEASURING_MASK) return FALSE;

	if (!(p_owner->_fn.read_bytes)(p_owner->_fn.p_device, BMP280_REG_PRESS_MSB, data, 6)) return FALSE;

    *p_pressure = (int32_t)((((uint32_t)(data[0])) << 12) | (((uint32_t)(data[1])) << 4) | ((uint32_t)data[2] >> 4));
    *p_temperature = (int32_t)((((uint32_t)(data[3])) << 12) | (((uint32_t)(data[4])) << 4) | ((uint32_t)data[5] >> 4));
	return TRUE;
}

BOOL pifBmp280_ReadBarometric(PifBmp280* p_owner, float* p_pressure, float* p_temperature)
{
	int32_t pressure, temperature;

	if (!pifBmp280_ReadRawData(p_owner, &pressure, &temperature)) return FALSE;

    if (p_temperature) *p_temperature = _compensate_T(p_owner, temperature);
    if (p_pressure) *p_pressure = _compensate_P(p_owner, pressure);
	return TRUE;
}

BOOL pifBmp280_AttachTaskForReading(PifBmp280* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, read_period * 1000, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "BMP280";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->__state = BMP280_STATE_START;
    return TRUE;
}

