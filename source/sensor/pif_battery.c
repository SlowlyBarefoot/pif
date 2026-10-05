// SPDX-License-Identifier: BSD-3-Clause
#include "sensor/pif_battery.h"


// mA * us in one mAh, and mW * us in one mWh.
#define US_PER_HOUR			3600000000LL


#define PRESET(MIN, WARNING, FULL, MAX, HYSTERESIS, SETTLE_DELTA) { \
		.cell_min_mv = MIN, .cell_warning_mv = WARNING, .cell_full_mv = FULL, .cell_max_mv = MAX, \
		.hysteresis_mv = HYSTERESIS, .present_mv = 0, .settle_ms = 500, .settle_delta_mv = SETTLE_DELTA, \
		.voltage_cutoff_hz = 1.0f, .current_cutoff_hz = 5.0f, .warning_delay_ms = 0, .critical_delay_ms = 0 }

//                                                   min   warn  full   max  hyst  delta
const PifBatteryConfig pif_battery_lipo    = PRESET(3300, 3500, 4100, 4300,  50,  100);
const PifBatteryConfig pif_battery_lihv    = PRESET(3300, 3500, 4250, 4400,  50,  100);
const PifBatteryConfig pif_battery_liion   = PRESET(3000, 3300, 4100, 4250,  50,  100);
const PifBatteryConfig pif_battery_lifepo4 = PRESET(2800, 3000, 3400, 3650,  30,   50);
const PifBatteryConfig pif_battery_nimh    = PRESET(1000, 1100, 1400, 1500,  20,   50);


/**
 * @fn _checkConfig
 * @param p_config Configuration to check.
 * @return TRUE if it is consistent, otherwise FALSE with E_INVALID_PARAM.
 */
static BOOL _checkConfig(const PifBatteryConfig* p_config)
{
	if (!p_config || !p_config->cell_min_mv || p_config->cell_min_mv >= p_config->cell_warning_mv ||
			p_config->cell_warning_mv >= p_config->cell_full_mv || p_config->cell_full_mv > p_config->cell_max_mv ||
			p_config->voltage_cutoff_hz < 0.0f || p_config->current_cutoff_hz < 0.0f) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _presentMv
 * @param p_owner Pointer to the instance.
 * @return Pack voltage at and above which a battery is connected.
 */
static uint32_t _presentMv(PifBattery* p_owner)
{
	return p_owner->__config.present_mv ? p_owner->__config.present_mv : p_owner->__config.cell_min_mv / 2U;
}


/**
 * @fn _doTask
 * @brief Updates the monitor.
 * @param p_task Task whose client is the instance.
 * @return 0, as the period is fixed.
 */
static uint32_t _doTask(PifTask* p_task)
{
	pifBattery_Update((PifBattery*)p_task->_p_client);
	return 0;
}

/**
 * @fn _filter
 * @brief Runs one sample through a low-pass filter whose cutoff is kept for a varying sample period.
 * @param p_filter Filter.
 * @param cutoff_hz Cutoff, or 0 to pass the sample through.
 * @param input Sample.
 * @param elapsed_us Time since the previous sample.
 * @return Filtered sample.
 */
static float _filter(PifPtFilter* p_filter, float cutoff_hz, float input, uint32_t elapsed_us)
{
	if (cutoff_hz <= 0.0f) {
		pifPtFilter_Reset(p_filter, input);
		return input;
	}
	pifPtFilter_SetCutoff(p_filter, cutoff_hz, elapsed_us * 1e-6f);
	return pifPtFilter_Apply(p_filter, input);
}

/**
 * @fn _addTime
 * @brief Adds to a time counter that stops at its largest value.
 * @param p_counter Counter in microseconds.
 * @param elapsed_us Time to add.
 */
static void _addTime(uint32_t* p_counter, uint32_t elapsed_us)
{
	*p_counter = *p_counter > UINT32_MAX - elapsed_us ? UINT32_MAX : *p_counter + elapsed_us;
}

/**
 * @fn _evaluate
 * @brief Works out the state from the cell voltage. A worse state needs the cell voltage to have
 *        stayed below its threshold for the configured delay; a better one needs it to clear the
 *        threshold by the hysteresis.
 * @param p_owner Pointer to the instance.
 * @param current State now, BAS_OK to BAS_CRITICAL, or BAS_INIT for the first evaluation.
 * @param elapsed_us Time since the previous reading.
 * @return BAS_OK, BAS_WARNING or BAS_CRITICAL.
 */
static PifBatteryState _evaluate(PifBattery* p_owner, PifBatteryState current, uint32_t elapsed_us)
{
	const PifBatteryConfig* p_config = &p_owner->__config;
	uint32_t cell_mv = pifBattery_GetCellVoltage(p_owner);
	PifBatteryState worse = BAS_OK, better;

	if (cell_mv < p_config->cell_warning_mv) _addTime(&p_owner->__below_warning_us, elapsed_us);
	else p_owner->__below_warning_us = 0;
	if (cell_mv < p_config->cell_min_mv) _addTime(&p_owner->__below_min_us, elapsed_us);
	else p_owner->__below_min_us = 0;

	if (cell_mv < p_config->cell_warning_mv && p_owner->__below_warning_us >= p_config->warning_delay_ms * 1000UL) {
		worse = BAS_WARNING;
	}
	if (cell_mv < p_config->cell_min_mv && p_owner->__below_min_us >= p_config->critical_delay_ms * 1000UL) {
		worse = BAS_CRITICAL;
	}
	if (current < BAS_OK || worse > current) return worse;

	if (cell_mv < (uint32_t)p_config->cell_min_mv + p_config->hysteresis_mv) better = BAS_CRITICAL;
	else if (cell_mv < (uint32_t)p_config->cell_warning_mv + p_config->hysteresis_mv) better = BAS_WARNING;
	else better = BAS_OK;
	return better < current ? better : current;
}

/**
 * @fn _countCells
 * @brief Counts the cells as the fewest that keep each one at or below the maximum cell voltage.
 * @param p_owner Pointer to the instance.
 */
static void _countCells(PifBattery* p_owner)
{
	uint32_t cells;

	if (p_owner->__forced_cells) {
		cells = p_owner->__forced_cells;
	}
	else {
		cells = (p_owner->_voltage_mv + p_owner->__config.cell_max_mv - 1) / p_owner->__config.cell_max_mv;
		if (cells < 1) cells = 1;
		else if (cells > PIF_BATTERY_MAX_CELLS) cells = PIF_BATTERY_MAX_CELLS;
	}
	p_owner->_cell_count = cells;
	p_owner->_full_at_start = pifBattery_GetCellVoltage(p_owner) >= p_owner->__config.cell_full_mv;
}

BOOL pifBattery_Init(PifBattery* p_owner, PifId id, const PifBatteryConfig* p_config, PifActBatteryRead act_voltage)
{
	if (!p_owner || !_checkConfig(p_config)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifBattery));

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
	p_owner->_state = BAS_NOT_PRESENT;
	p_owner->__config = *p_config;
	pifPtFilter_Init(&p_owner->__voltage_lpf, 1, 1.0f, 0.01f);
	pifPtFilter_Init(&p_owner->__current_lpf, 1, 1.0f, 0.01f);
	p_owner->__act_voltage = act_voltage;
	return TRUE;
}

void pifBattery_Clear(PifBattery* p_owner)
{
	if (p_owner->_p_task) {
		pifTaskManager_Remove(p_owner->_p_task);
		p_owner->_p_task = NULL;
	}
}

void pifBattery_AttachActCurrent(PifBattery* p_owner, PifActBatteryRead act_current)
{
	p_owner->__act_current = act_current;
}

BOOL pifBattery_AttachTask(PifBattery* p_owner, uint32_t period1us, BOOL start)
{
	if (!p_owner || !period1us) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_p_task) {
		pif_error = E_ALREADY_ATTACHED;
		return FALSE;
	}

	p_owner->_p_task = pifTaskManager_Add(PIF_ID_AUTO, TM_PERIOD, period1us, _doTask, p_owner, start);
	if (!p_owner->_p_task) return FALSE;
	p_owner->_p_task->name = "Battery";
	return TRUE;
}

BOOL pifBattery_SetConfig(PifBattery* p_owner, const PifBatteryConfig* p_config)
{
	PifBatteryState old_state = p_owner->_state;

	if (!_checkConfig(p_config)) return FALSE;

	p_owner->__config = *p_config;

	// The cell count depends on the thresholds, so count again once the voltage has settled.
	if (old_state > BAS_INIT) {
		p_owner->_state = BAS_INIT;
		p_owner->_cell_count = 0;
		p_owner->__settle_elapsed_us = 0;
		p_owner->__settle_ref_mv = p_owner->_voltage_mv;
		if (p_owner->evt_state) (*p_owner->evt_state)(p_owner, old_state);
	}
	return TRUE;
}

const PifBatteryConfig* pifBattery_GetConfig(PifBattery* p_owner)
{
	return &p_owner->__config;
}

BOOL pifBattery_SetCellCount(PifBattery* p_owner, uint8_t cells)
{
	if (cells > PIF_BATTERY_MAX_CELLS) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__forced_cells = cells;
	if (p_owner->_cell_count) _countCells(p_owner);
	return TRUE;
}

void pifBattery_SetCapacity(PifBattery* p_owner, uint32_t capacity_mah)
{
	p_owner->__capacity_mah = capacity_mah;
}

void pifBattery_Update(PifBattery* p_owner)
{
	uint32_t now, elapsed = 0;
	int32_t voltage, current = 0;

	if (!p_owner->__act_voltage) return;

	now = (*pif_act_timer1us)();
	if (p_owner->__timed) elapsed = now - p_owner->__last_us;
	p_owner->__last_us = now;
	p_owner->__timed = TRUE;

	voltage = (*p_owner->__act_voltage)(p_owner);
	if (p_owner->__act_current) current = (*p_owner->__act_current)(p_owner);
	pifBattery_Process(p_owner, voltage > 0 ? voltage : 0, current, elapsed);
}

void pifBattery_Process(PifBattery* p_owner, uint32_t voltage_mv, int32_t current_ma, uint32_t elapsed_us)
{
	PifBatteryState old_state = p_owner->_state;
	uint32_t delta;
	float value;

	// Filter. The first reading, and the first one after a battery is plugged in, start the
	// voltage filter at the reading instead of ramping up to it.
	if (!p_owner->__primed || !elapsed_us) {
		if (!p_owner->__primed) pifPtFilter_Reset(&p_owner->__current_lpf, current_ma);
		pifPtFilter_Reset(&p_owner->__voltage_lpf, voltage_mv);
		p_owner->__primed = TRUE;
	}
	else {
		if (old_state == BAS_NOT_PRESENT && voltage_mv >= _presentMv(p_owner)) {
			pifPtFilter_Reset(&p_owner->__voltage_lpf, voltage_mv);
		}
		else {
			_filter(&p_owner->__voltage_lpf, p_owner->__config.voltage_cutoff_hz, voltage_mv, elapsed_us);
		}
		_filter(&p_owner->__current_lpf, p_owner->__config.current_cutoff_hz, current_ma, elapsed_us);
	}
	value = pifPtFilter_Output(&p_owner->__voltage_lpf);
	p_owner->_voltage_mv = value > 0.0f ? (uint32_t)(value + 0.5f) : 0;
	value = pifPtFilter_Output(&p_owner->__current_lpf);
	p_owner->_current_ma = (int32_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);

	// Consumption, from the unfiltered current so that nothing is lost to the filter lag.
	if (elapsed_us && current_ma) {
		p_owner->__charge_ma_us += (int64_t)current_ma * elapsed_us;
		p_owner->__energy_mw_us += (int64_t)current_ma * voltage_mv * elapsed_us / 1000;
		p_owner->_consumed_mah = p_owner->__charge_ma_us > 0 ? p_owner->__charge_ma_us / US_PER_HOUR : 0;
		p_owner->_consumed_mwh = p_owner->__energy_mw_us > 0 ? p_owner->__energy_mw_us / US_PER_HOUR : 0;
	}

	// State. A battery is plugged in when the reading reaches the presence threshold, and
	// unplugged when the filtered voltage falls below it, so a short sag does not count.
	switch (old_state) {
	case BAS_NOT_PRESENT:
		if (!p_owner->__hold_presence && voltage_mv >= _presentMv(p_owner)) {
			p_owner->_state = BAS_INIT;
			p_owner->_cell_count = 0;
			p_owner->__settle_elapsed_us = 0;
			p_owner->__settle_ref_mv = p_owner->_voltage_mv;
			pifBattery_ResetConsumption(p_owner);
		}
		break;

	case BAS_INIT:
		if (p_owner->__hold_presence) break;
		if (p_owner->_voltage_mv < _presentMv(p_owner)) {
			p_owner->_state = BAS_NOT_PRESENT;
			break;
		}
		delta = p_owner->_voltage_mv > p_owner->__settle_ref_mv ? p_owner->_voltage_mv - p_owner->__settle_ref_mv
				: p_owner->__settle_ref_mv - p_owner->_voltage_mv;
		if (delta > p_owner->__config.settle_delta_mv) {
			p_owner->__settle_ref_mv = p_owner->_voltage_mv;
			p_owner->__settle_elapsed_us = 0;
			break;
		}
		p_owner->__settle_elapsed_us += elapsed_us;
		if (p_owner->__settle_elapsed_us >= p_owner->__config.settle_ms * 1000UL) {
			_countCells(p_owner);
			p_owner->__below_warning_us = 0;
			p_owner->__below_min_us = 0;
			p_owner->_state = _evaluate(p_owner, BAS_INIT, 0);
		}
		break;

	default:
		if (!p_owner->__hold_presence && p_owner->_voltage_mv < _presentMv(p_owner)) {
			p_owner->_state = BAS_NOT_PRESENT;
			p_owner->_cell_count = 0;
		}
		else {
			p_owner->_state = _evaluate(p_owner, old_state, elapsed_us);
		}
		break;
	}

	if (p_owner->_state != old_state && p_owner->evt_state) {
		(*p_owner->evt_state)(p_owner, old_state);
	}
}

void pifBattery_HoldPresence(PifBattery* p_owner, BOOL hold)
{
	p_owner->__hold_presence = hold;
}

void pifBattery_SetConsumedMah(PifBattery* p_owner, uint32_t consumed_mah)
{
	p_owner->__charge_ma_us = (int64_t)consumed_mah * US_PER_HOUR;
	p_owner->_consumed_mah = consumed_mah;
}

void pifBattery_ResetConsumption(PifBattery* p_owner)
{
	p_owner->__charge_ma_us = 0;
	p_owner->__energy_mw_us = 0;
	p_owner->_consumed_mah = 0;
	p_owner->_consumed_mwh = 0;
}

uint16_t pifBattery_GetCellVoltage(PifBattery* p_owner)
{
	if (!p_owner->_cell_count) return 0;
	return p_owner->_voltage_mv / p_owner->_cell_count;
}

uint8_t pifBattery_GetRemainingPercent(PifBattery* p_owner)
{
	uint32_t cell_mv;

	if (!p_owner->_cell_count) return 0;

	if (p_owner->__capacity_mah) {
		if (p_owner->_consumed_mah >= p_owner->__capacity_mah) return 0;
		return 100 - p_owner->_consumed_mah * 100 / p_owner->__capacity_mah;
	}

	cell_mv = pifBattery_GetCellVoltage(p_owner);
	if (cell_mv <= p_owner->__config.cell_min_mv) return 0;
	if (cell_mv >= p_owner->__config.cell_full_mv) return 100;
	return (cell_mv - p_owner->__config.cell_min_mv) * 100 / (p_owner->__config.cell_full_mv - p_owner->__config.cell_min_mv);
}
