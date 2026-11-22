// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/baro/pif_ms5611.h"


// How long to leave the device before a step that failed is tried again. A device that did not
// answer is unlikely to answer on the next pass, so the reading waits rather than spinning on it.
#define MS5611_RETRY_DELAY_US		100000UL

// Asked for as soon as the ring comes round again, which is what a step that needs no wait of its
// own wants. It is a period rather than pifTask_SetTrigger(), because a trigger is taken before
// the pause flag and before the check that the run fits in the time left before a TM_REALTIME
// release, and stepping a reading along is ordinary work that should be subject to both.
#define MS5611_NEXT_STEP_US			1UL


/**
 * @fn _promCrc
 * @brief The CRC-4 of the PROM, over the 16 bytes of its eight words with the nibble that holds the CRC itself
 *        taken out, as the datasheet describes it. The words are not touched.
 * @param p_prom The eight words read from the PROM.
 * @return The 4 bit remainder, to compare against the low nibble of the last word.
 */
static uint8_t _promCrc(const uint16_t* p_prom)
{
	uint16_t rem = 0;
	uint16_t word;
	int i, bit;

	for (i = 0; i < 16; i++) {
		// The last word carries the CRC in its low nibble, and the CRC is taken over that nibble as 0.
		word = (i >> 1) == 7 ? (uint16_t)(p_prom[7] & 0xFF00) : p_prom[i >> 1];
		rem ^= (i & 1) ? (uint16_t)(word & 0x00FF) : (uint16_t)(word >> 8);
		for (bit = 8; bit > 0; bit--) {
			if (rem & 0x8000) rem = (uint16_t)((rem << 1) ^ 0x3000);
			else rem = (uint16_t)(rem << 1);
		}
	}
	return (uint8_t)((rem >> 12) & 0x0F);
}

/**
 * @fn _checkProm
 * @brief Tells whether the words read from the PROM are the ones the factory wrote there, and says with
 *        pif_error which of the two ways it is not.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE when they are, otherwise FALSE.
 */
static BOOL _checkProm(PifMs5611* p_owner)
{
	int i;

	// A bus that answers every read with zeros passes the CRC, since the remainder of all zeros is zero.
	// The factory words are never all zero, so take that for no device rather than for a calibration.
	for (i = 0; i < 8; i++) {
		if (p_owner->_prom[i]) break;
	}
	if (i >= 8) {
		pif_error = E_WRONG_DATA;
		return FALSE;
	}

	if (_promCrc(p_owner->_prom) != (p_owner->_prom[7] & 0x0F)) {
		pif_error = E_MISMATCH_CRC;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _compensate
 * @brief Turns the raw conversions into pressure and temperature with the PROM coefficients, first and second
 *        order both, as the datasheet gives them.
 * @param p_owner Pointer to the owner instance.
 * @param p_pressure Receives the pressure in hPa.
 * @param p_temperature Receives the temperature in degrees C.
 */
static void _compensate(PifMs5611* p_owner, float* p_pressure, float* p_temperature)
{
	int64_t dT, temp, off, sens, p;
	int64_t low;

	// C5 is the reference temperature and C6 its coefficient: TEMP comes out in 0.01 degrees C.
	dT = (int64_t)p_owner->__D2 - ((int64_t)p_owner->_prom[5] << 8);
	temp = 2000 + ((dT * (int64_t)p_owner->_prom[6]) >> 23);

	// C2 and C4 are the pressure offset and its temperature coefficient, C1 and C3 its sensitivity.
	off = ((int64_t)p_owner->_prom[2] << 16) + (((int64_t)p_owner->_prom[4] * dT) >> 7);
	sens = ((int64_t)p_owner->_prom[1] << 15) + (((int64_t)p_owner->_prom[3] * dT) >> 8);

	// Below 20 degrees C the first order result needs the second order correction, and below
	// -15 degrees C a further one on top of it.
	if (temp < 2000) {
		low = 5 * (temp - 2000) * (temp - 2000);
		off -= low >> 1;
		sens -= low >> 2;

		if (temp < -1500) {
			low = (temp + 1500) * (temp + 1500);
			off -= 7 * low;
			sens -= (11 * low) >> 1;
		}

		temp -= (dT * dT) >> 31;
	}

	// P comes out in 0.01 mbar, which is 1 Pa, so a hundredth of it is a hPa.
	p = (((((int64_t)p_owner->__D1 * sens) >> 21) - off) >> 15);

	*p_temperature = temp / 100.0f;
	*p_pressure = p / 100.0f;
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

	return pifDeviceReg_Write(p_owner->_p_device, 0, 0, &value, 1);
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

	if (!pifDeviceReg_ReadRegBytes(p_owner->_p_device, MS5611_REG_ADC_READ, value, 3)) return FALSE;

	*p_data = ((uint32_t)value[0] << 16) + (value[1] << 8) + value[2];
	return TRUE;
}

/**
 * @fn _stepReading
 * @brief Carries the reading in _state one step along: sends a conversion command or reads a conversion out.
 *        Both drivers walk the same steps through here and differ only in how they wait and how they report.
 * @param p_owner Pointer to the owner instance.
 * @param p_wait1ms Receives how long the device has to be left before the next step, 0 for no wait at all.
 * @return TRUE when the step was taken, FALSE when its transfer failed.
 */
static BOOL _stepReading(PifMs5611* p_owner, uint16_t* p_wait1ms)
{
	*p_wait1ms = 0;

	switch (p_owner->_state) {
	case MS5611_STATE_TEMPERATURE_START:
		if (!_startConversion(p_owner, MS5611_REG_CONV_D2)) return FALSE;
		p_owner->_state = MS5611_STATE_TEMPERATURE_WAIT;
		*p_wait1ms = p_owner->_conversion_time;
		break;

	case MS5611_STATE_TEMPERATURE_WAIT:
		if (!_readAdc(p_owner, &p_owner->__D2)) return FALSE;
		// The pressure conversion can go out at once: nothing has to be left for it.
		p_owner->_state = MS5611_STATE_PRESSURE_START;
		break;

	case MS5611_STATE_PRESSURE_START:
		if (!_startConversion(p_owner, MS5611_REG_CONV_D1)) return FALSE;
		p_owner->_state = MS5611_STATE_PRESSURE_WAIT;
		*p_wait1ms = p_owner->_conversion_time;
		break;

	case MS5611_STATE_PRESSURE_WAIT:
		if (!_readAdc(p_owner, &p_owner->__D1)) return FALSE;
		p_owner->_state = MS5611_STATE_CALCURATE;
		break;

	default:
		break;
	}
	return TRUE;
}

/**
 * @fn _conversionTicks
 * @brief A wait in ms as ticks of the timer manager.
 * @param p_owner Pointer to the owner instance.
 * @param wait1ms The wait in ms.
 * @return Ticks to wait.
 */
static uint32_t _conversionTicks(PifMs5611* p_owner, uint16_t wait1ms)
{
	uint32_t period = p_owner->__p_timer_manager->_period1us;

	// Rounded up: a tick too many leaves the result sitting in the device a little longer, which
	// costs nothing, while a tick too few would read a conversion that is not finished.
	// It cannot round down to zero, which pifTimer_Start() would reject, because the shortest
	// conversion this device offers is 2ms.
	return (wait1ms * 1000UL + period - 1) / period;
}

/**
 * @fn _runToWait
 * @brief Steps the reading of the timer driver along until the device needs time, and leaves that wait to the
 *        timer. On return the reading is either waiting on the timer or at MS5611_STATE_CALCURATE.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE while the reading is on its way, FALSE when a transfer or the timer failed.
 */
static BOOL _runToWait(PifMs5611* p_owner)
{
	uint16_t wait1ms;

	// Only the four steps of a reading are walked here. Any other state, MS5611_STATE_IDLE above all, is
	// no reading on its way, and the caller sees that in _state.
	while (p_owner->_state >= MS5611_STATE_TEMPERATURE_START && p_owner->_state <= MS5611_STATE_PRESSURE_WAIT) {
		if (!_stepReading(p_owner, &wait1ms)) return FALSE;
		if (wait1ms) return pifTimer_Start(p_owner->__p_timer, _conversionTicks(p_owner, wait1ms));
	}
	return TRUE;
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
	if (!_runToWait(p_owner)) {
		p_owner->_state = MS5611_STATE_IDLE;
		// Reported rather than dropped. Saying nothing would leave a caller that waits on the event
		// waiting for a reading that is never coming. There are no values on this path.
		if (p_owner->__evt_timer_read) (*p_owner->__evt_timer_read)(p_owner, FALSE, 0.0f, 0.0f);
		return;
	}
	if (p_owner->_state != MS5611_STATE_CALCURATE) return;

	_compensate(p_owner, &pressure, &temperature);

	// Left before the event so that the next reading may be started from inside it.
	p_owner->_state = MS5611_STATE_IDLE;
	if (p_owner->__evt_timer_read) (*p_owner->__evt_timer_read)(p_owner, TRUE, pressure, temperature);
}

/**
 * @fn _doTask
 * @brief Takes the reading of the task driver one step per run, and keeps the readings to the read period.
 * @param p_task Pointer to the task instance that invokes this callback.
 * @return How long before the next run, in microseconds.
 */
static uint32_t _doTask(PifTask* p_task)
{
	PifMs5611* p_owner = p_task->_p_client;
	uint16_t wait1ms;
	uint16_t gap;
	float pressure;
	float temperature;

	// Both drivers share _state, and the timer driver leaves it here between its readings. A task on an
	// instance whose readings the timer is carrying has nothing to do, and waits rather than spinning.
	if (p_owner->_state == MS5611_STATE_IDLE) return MS5611_RETRY_DELAY_US;

	if (p_owner->_state == MS5611_STATE_CALCURATE) {
		_compensate(p_owner, &pressure, &temperature);
		if (p_owner->__evt_read) (*p_owner->__evt_read)(pressure, temperature);

		// What is left of the read period, so that the readings keep to their grid however long the
		// two conversions took. A reading that already overran it starts the next one straight away.
		p_owner->_state = MS5611_STATE_TEMPERATURE_START;
		gap = pif_cumulative_timer1ms - p_owner->__start_time;
		if (gap < p_owner->__read_period) return (p_owner->__read_period - gap) * 1000UL;
		return MS5611_NEXT_STEP_US;
	}

	if (p_owner->_state == MS5611_STATE_TEMPERATURE_START) p_owner->__start_time = pif_cumulative_timer1ms;

	if (!_stepReading(p_owner, &wait1ms)) return MS5611_RETRY_DELAY_US;
	return wait1ms ? wait1ms * 1000UL : MS5611_NEXT_STEP_US;
}

BOOL pifMs5611_Init(PifMs5611* p_owner, PifId id, PifI2cPort* p_i2c, uint8_t addr, void *p_client)
{
	PifI2cDevice *p_device;

	int i;
	uint8_t cmd = MS5611_REG_RESET;

	if (!p_owner || !p_i2c) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
	}

	memset(p_owner, 0, sizeof(PifMs5611));

    p_device = pifI2cPort_AddDevice(p_i2c, PIF_ID_AUTO, addr, p_client);
    if (!p_device) return FALSE;
    p_owner->_p_device = &p_device->parent;

    // A zeroed instance already means MS5611_OSR_256, but _conversion_time would stay 0 with it,
    // and everything that waits for a conversion reads that. Set the pair together.
    pifMs5611_SetOverSamplingRate(p_owner, MS5611_OSR_256);

	if (!pifDeviceReg_Write(p_owner->_p_device, 0, 0, &cmd, 1)) goto fail;
	// The reset loads the PROM into the internal register, and nothing may be read before that is
	// through. Initialization runs before there is anything to schedule, so the wait holds the CPU
	// and nothing is lost by it.
	pif_Delay1ms(PIF_MS5611_RESET_DELAY_MS);

	// Eight words at two addresses apart: the setup word, the six coefficients, and the serial code
	// with the CRC of them all.
	for (i = 0; i < 8; i++) {
		if (!pifDeviceReg_ReadRegWord(p_owner->_p_device, MS5611_REG_READ_PROM + i * 2, &p_owner->_prom[i])) goto fail;
	}
	if (!_checkProm(p_owner)) goto fail;

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
	if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
	}
}

void pifMs5611_SetOverSamplingRate(PifMs5611* p_owner, uint16_t osr)
{
	p_owner->_over_sampling_rate = osr;
	// The conversion times the datasheet gives at most, 0.60 to 8.22ms, in whole ms and a ms of margin
	// on top: the device says nothing about a conversion being over, so it is only ever left for this.
	switch (osr) {
	case MS5611_OSR_256: p_owner->_conversion_time = 1 + 1; break;
	case MS5611_OSR_512: p_owner->_conversion_time = 2 + 1; break;
	case MS5611_OSR_1024: p_owner->_conversion_time = 3 + 1; break;
	case MS5611_OSR_2048: p_owner->_conversion_time = 5 + 1; break;
	case MS5611_OSR_4096: p_owner->_conversion_time = 9 + 1; break;
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
	p_owner->_state = MS5611_STATE_TEMPERATURE_START;
	if (!_runToWait(p_owner)) {
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

	// Not event_into_int: that callback is called from inside pifTimerManager_sigTick(),
	// and reading a conversion is an I2C transfer that has no business in a tick interrupt.
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner, FALSE);

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

BOOL pifMs5611_AttachTaskForReading(PifMs5611* p_owner, PifId id, uint16_t read_period,
	PifEvtBaroRead evt_read, BOOL start, const char *name)
{
	p_owner->_p_task = pifTaskManager_Add(id, TM_PERIOD, read_period * 1000UL, _doTask, p_owner, start);
    if (!p_owner->_p_task) return FALSE;
    p_owner->_p_task->name = name ? name : "MS5611";

    p_owner->__read_period = read_period;
    p_owner->__evt_read = evt_read;
    p_owner->_state = MS5611_STATE_TEMPERATURE_START;
    return TRUE;
}
