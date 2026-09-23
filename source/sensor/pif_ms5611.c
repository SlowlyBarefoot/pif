#include "sensor/pif_ms5611.h"

#include <math.h>


// How long to leave the device before a step that failed is tried again. A device that did not
// answer is unlikely to answer on the next pass, so the reading waits rather than spinning on it.
#define MS5611_RETRY_DELAY_US		100000UL

// Asked for as soon as the ring comes round again, which is what a step that needs no wait of its
// own wants. It is a period rather than pifTask_SetTrigger(), because a trigger is taken before
// the pause flag and before the check that the run fits in the time left before a TM_REALTIME
// release, and stepping a reading along is ordinary work that should be subject to both.
#define MS5611_NEXT_STEP_US			1UL


/**
 * @fn _checkPromCrc
 * @brief Checks internal state for check prom crc and reports the result.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE on failure.
 */
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

/**
 * @fn _calcurateBarometric
 * @brief Internal helper that supports calcurate barometric logic.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Pointer to pressure.
 * @param p_temperature Pointer to temperature.
 * @return None.
 */
static void _calcurateBarometric(PifMs5611* p_owner, float* p_pressure, float* p_temperature)
{
	int64_t dT;
	int64_t temp;
	int64_t off, sens;
	int64_t delt;

	dT = (int64_t)p_owner->__D2 - ((int64_t)p_owner->_prom[5] * 256);
	temp = 2000 + ((dT * (int64_t)p_owner->_prom[6]) >> 23);

	off = ((int64_t)p_owner->_prom[2] << 16) + (((int64_t)p_owner->_prom[4] * dT) >> 7);
	sens = ((int64_t)p_owner->_prom[1] << 15) + (((int64_t)p_owner->_prom[3] * dT) >> 8);

	if (temp < 2000) {
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

        temp -= ((int64_t)dT * dT) / 2147483648L;
	}

    *p_temperature = temp / 100.0;
	*p_pressure = (float)(((((int64_t)p_owner->__D1 * sens) >> 21) - off) >> 15);
}

/**
 * @fn _startConversion
 * @brief Tells the device to convert one of its two channels.
 * @param p_owner Pointer to the owner instance.
 * @param reg MS5611_REG_CONV_D1 for pressure or MS5611_REG_CONV_D2 for temperature.
 * @return TRUE when the command was sent, otherwise FALSE.
 */
static BOOL _startConversion(PifMs5611* p_owner, uint8_t reg)
{
	uint8_t value = reg + p_owner->_over_sampling_rate;

	return pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, &value, 1);
}

/**
 * @fn _readAdc
 * @brief Reads the result of the conversion that has been left for its conversion time.
 * @param p_owner Pointer to the owner instance.
 * @param p_data Pointer to the 24 bit result.
 * @return TRUE when the result was read, otherwise FALSE.
 */
static BOOL _readAdc(PifMs5611* p_owner, uint32_t* p_data)
{
	uint8_t value[3];

	if (!pifI2cDevice_ReadRegBytes(p_owner->_p_i2c, MS5611_REG_ADC_READ, value, 3)) return FALSE;

	*p_data = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
	return TRUE;
}

/**
 * @fn _conversionTicks
 * @brief The conversion time of the current oversampling rate, in ticks of the timer manager.
 * @param p_owner Pointer to the owner instance.
 * @return Ticks to wait.
 */
static uint32_t _conversionTicks(PifMs5611* p_owner)
{
	uint32_t period = p_owner->__p_timer_manager->_period1us;

	// Rounded up: a tick too many leaves the result sitting in the device a little longer, which
	// costs nothing, while a tick too few would read a conversion that is not finished.
	// It cannot round down to zero, which pifTimer_Start() would reject, because the shortest
	// conversion this device offers is 2ms.
	return (p_owner->_conversion_time * 1000UL + period - 1) / period;
}

/**
 * @fn _abortTimerRead
 * @brief Gives up on a reading whose transfer failed and says so.
 * @param p_owner Pointer to the owner instance.
 */
static void _abortTimerRead(PifMs5611* p_owner)
{
	p_owner->_state = MS5611_STATE_IDLE;
	// Reported rather than dropped. Saying nothing would leave a caller that waits on the event
	// waiting for a reading that is never coming. There are no values on this path.
	if (p_owner->__evt_timer_read) (*p_owner->__evt_timer_read)(p_owner, FALSE, 0.0f, 0.0f);
}

/**
 * @fn _evtTimerFinish
 * @brief Carries the reading on from the conversion the timer was waiting out.
 * @param p_issuer Issuer pointer castable to PifMs5611.
 */
static void _evtTimerFinish(PifIssuerP p_issuer)
{
	PifMs5611* p_owner = (PifMs5611*)p_issuer;
	float pressure;
	float temperature;

	// Reached from the timer process of the task manager, which runs at the start of a loop with
	// the CPU to itself, so the transfers below are in the same context as ones inside any task.
	switch (p_owner->_state) {
	case MS5611_STATE_TEMPERATURE_WAIT:
		if (!_readAdc(p_owner, &p_owner->__D2)) {
			_abortTimerRead(p_owner);
			return;
		}
		// The second conversion goes out at once: only the waiting needs the timer.
		if (!_startConversion(p_owner, MS5611_REG_CONV_D1)) {
			_abortTimerRead(p_owner);
			return;
		}
		p_owner->_state = MS5611_STATE_PRESSURE_WAIT;
		if (!pifTimer_Start(p_owner->__p_timer, _conversionTicks(p_owner))) {
			_abortTimerRead(p_owner);
		}
		break;

	case MS5611_STATE_PRESSURE_WAIT:
		if (!_readAdc(p_owner, &p_owner->__D1)) {
			_abortTimerRead(p_owner);
			return;
		}
		_calcurateBarometric(p_owner, &pressure, &temperature);

		// Left before the event so that the next reading may be started from inside it.
		p_owner->_state = MS5611_STATE_IDLE;
		if (p_owner->__evt_timer_read) {
			(*p_owner->__evt_timer_read)(p_owner, TRUE, pressure, temperature);
		}
		break;

	default:
		break;
	}
}

/**
 * @fn _doTask
 * @brief Internal helper that supports do task logic.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return Computed integer value.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifMs5611* p_owner = p_task->_p_client;
	uint32_t delay = MS5611_RETRY_DELAY_US;
	uint16_t gap;
	float pressure;
	float temperature;

	switch (p_owner->_state) {
	case MS5611_STATE_TEMPERATURE_START:
		p_owner->__start_time = pif_cumulative_timer1ms;
		if (_startConversion(p_owner, MS5611_REG_CONV_D2)) {
			p_owner->_state = MS5611_STATE_TEMPERATURE_WAIT;
			delay = p_owner->_conversion_time * 1000UL;
		}
		break;

	case MS5611_STATE_TEMPERATURE_WAIT:
		if (_readAdc(p_owner, &p_owner->__D2)) {
			p_owner->_state = MS5611_STATE_PRESSURE_START;
			// The second conversion can go out at once: nothing has to be left for it.
			delay = MS5611_NEXT_STEP_US;
		}
		break;

	case MS5611_STATE_PRESSURE_START:
		if (_startConversion(p_owner, MS5611_REG_CONV_D1)) {
			p_owner->_state = MS5611_STATE_PRESSURE_WAIT;
			delay = p_owner->_conversion_time * 1000UL;
		}
		break;

	case MS5611_STATE_PRESSURE_WAIT:
		if (_readAdc(p_owner, &p_owner->__D1)) {
			p_owner->_state = MS5611_STATE_CALCURATE;
			delay = MS5611_NEXT_STEP_US;
		}
		break;

	case MS5611_STATE_CALCURATE:
		_calcurateBarometric(p_owner, &pressure, &temperature);
		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		// What is left of the read period, so that the readings keep to their grid however long the
		// two conversions took. A reading that already overran it starts the next one straight away.
		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) {
			delay = (p_owner->__read_period - gap) * 1000UL;
		}
		else {
			delay = MS5611_NEXT_STEP_US;
		}
		p_owner->_state = MS5611_STATE_TEMPERATURE_START;
		break;

	default:
		break;
	}
	return delay;
}

BOOL pifMs5611_Init(PifMs5611* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client)
{
	int i;
    uint8_t cmd = MS5611_REG_RESET;

	if (!p_owner || !p_i2c) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMs5611));

    p_owner->_p_i2c = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_owner->_p_i2c) return FALSE;

    // A zeroed instance already means MS5611_OSR_256, but _conversion_time would stay 0 with it,
    // and everything that waits for a conversion reads that. Set the pair together.
    pifMs5611_SetOverSamplingRate(p_owner, MS5611_OSR_256);

	if (!pifI2cDevice_Write(p_owner->_p_i2c, 0, 0, &cmd, 1)) goto fail;
	// The reset needs this long before the PROM can be read. Initialization runs before there is
	// anything to schedule, so the wait holds the CPU and nothing is lost by it.
	pif_Delay1ms(100);

	for (i = 0; i < 8; i++) {
		if (!pifI2cDevice_ReadRegWord(p_owner->_p_i2c, MS5611_REG_READ_PROM + i * 2, (uint16_t*)&p_owner->_prom[i])) goto fail;
	}
	if (!_checkPromCrc(p_owner)) goto fail;

	if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    return TRUE;

fail:
	pifMs5611_Clear(p_owner);
	return FALSE;
}

void pifMs5611_Clear(PifMs5611* p_owner)
{
	pifMs5611_DetachTimer(p_owner);
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
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

BOOL pifMs5611_StartBarometric(PifMs5611* p_owner)
{
	if (!p_owner->__p_timer) {
		pif_error = E_CANNOT_FOUND;
		return FALSE;
	}
	// There is one pair of raw values and one timer, so the reading on its way has to end before
	// the next may start.
	if (p_owner->_state != MS5611_STATE_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__start_time = pif_cumulative_timer1ms;
	if (!_startConversion(p_owner, MS5611_REG_CONV_D2)) return FALSE;

	p_owner->_state = MS5611_STATE_TEMPERATURE_WAIT;
	if (!pifTimer_Start(p_owner->__p_timer, _conversionTicks(p_owner))) {
		p_owner->_state = MS5611_STATE_IDLE;
		return FALSE;
	}
	return TRUE;
}

BOOL pifMs5611_AttachTimer(PifMs5611* p_owner, PifTimerManager* p_timer_manager, PifEvtMs5611Read evt_read)
{
	if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->__p_timer) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
	if (!p_owner->__p_timer) return FALSE;

	// Not pifTimer_AttachEvtIntFinish(): that one is called from inside pifTimerManager_sigTick(),
	// and reading a conversion is an I2C transfer that has no business in a tick interrupt.
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner);

	p_owner->__p_timer_manager = p_timer_manager;
	p_owner->__evt_timer_read = evt_read;
	p_owner->_state = MS5611_STATE_IDLE;
	return TRUE;
}

void pifMs5611_DetachTimer(PifMs5611* p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
	p_owner->__p_timer_manager = NULL;
	p_owner->_state = MS5611_STATE_IDLE;
}

BOOL pifMs5611_AttachTaskForReading(PifMs5611* p_owner, PifId id, uint16_t read_period, PifEvtBaroRead evt_read, BOOL start)
{
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, read_period * 1000, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
    p_owner->_p_task->name = "MS5611";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->_state = MS5611_STATE_TEMPERATURE_START;
    return TRUE;
}
