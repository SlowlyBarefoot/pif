// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_BATTERY_H
#define PIF_BATTERY_H


#include "core/pif_task_manager.h"
#include "filter/pif_pt_filter.h"


/*
 * Battery monitor for a pack of identical cells in series.
 * The pack voltage, and the current if there is a sensor, come from the client in millivolts and
 * milliamps; a PifAdc channel scaled with pifAdc_SetScale() gives them directly. From them this
 * keeps:
 *   - filtered voltage and current,
 *   - whether a battery is connected, and the number of cells once the voltage has settled,
 *   - charge and energy drawn since the battery was connected,
 *   - a state of OK, WARNING or CRITICAL from the voltage per cell, with hysteresis so that a
 *     sagging pack under load does not flip between states,
 *   - the remaining charge in percent, from the capacity if it is known, else from the voltage.
 * The thresholds depend on the cell chemistry and come in a PifBatteryConfig. Presets for common
 * chemistries are provided; copy one and change what differs for a particular pack.
 */


#ifndef PIF_BATTERY_MAX_CELLS
#define PIF_BATTERY_MAX_CELLS		12
#endif


typedef enum EnPifBatteryState
{
	BAS_NOT_PRESENT		= 0,	// Voltage below the presence threshold
	BAS_INIT			= 1,	// Connected, waiting for the voltage to settle
	BAS_OK				= 2,
	BAS_WARNING			= 3,	// Cell voltage below the warning threshold
	BAS_CRITICAL		= 4		// Cell voltage below the minimum
} PifBatteryState;


/**
 * @class StPifBatteryConfig
 * @brief Thresholds and timing of a battery monitor. Cell voltages must satisfy
 *        0 < cell_min_mv < cell_warning_mv < cell_full_mv <= cell_max_mv.
 */
typedef struct StPifBatteryConfig
{
	uint16_t cell_min_mv;		// Below this the state is BAS_CRITICAL; 0 % for the voltage-based charge
	uint16_t cell_warning_mv;	// Below this the state is BAS_WARNING
	uint16_t cell_full_mv;		// A charged cell at rest; 100 % for the voltage-based charge
	uint16_t cell_max_mv;		// Highest voltage of a cell, used to count the cells
	uint16_t hysteresis_mv;		// How far above a threshold a cell must come back to leave a worse state
	uint16_t present_mv;		// Pack voltage at and above which a battery is connected; 0 for cell_min_mv / 2
	uint16_t settle_ms;			// Time the voltage must stay within settle_delta_mv before the cells are counted
	uint16_t settle_delta_mv;	// Largest change of the voltage while settling
	float voltage_cutoff_hz;	// Cutoff of the voltage low-pass filter, or 0 for none
	float current_cutoff_hz;	// Cutoff of the current low-pass filter, or 0 for none
	uint16_t warning_delay_ms;	// Time a cell must stay below cell_warning_mv before BAS_WARNING
	uint16_t critical_delay_ms;	// Time a cell must stay below cell_min_mv before BAS_CRITICAL
} PifBatteryConfig;

// Presets with the usual nominal values of each chemistry. Check them against the data sheet of
// the cells in use.
extern const PifBatteryConfig pif_battery_lipo;		// Lithium polymer, 4.2 V charge
extern const PifBatteryConfig pif_battery_lihv;		// High-voltage lithium polymer, 4.35 V charge
extern const PifBatteryConfig pif_battery_liion;	// Lithium-ion cylindrical cells such as 18650
extern const PifBatteryConfig pif_battery_lifepo4;	// Lithium iron phosphate
extern const PifBatteryConfig pif_battery_nimh;		// Nickel-metal hydride


struct StPifBattery;
typedef struct StPifBattery PifBattery;

/**
 * @fn PifActBatteryRead
 * @brief Gives the latest pack voltage in millivolts, or current in milliamps.
 * @param p_owner Pointer to the battery instance.
 * @return Voltage or current. A current is positive while discharging.
 */
typedef int32_t (*PifActBatteryRead)(PifBattery* p_owner);

/**
 * @fn PifEvtBatteryState
 * @brief Reports a change of state.
 * @param p_owner Pointer to the battery instance.
 * @param old_state State before the change.
 */
typedef void (*PifEvtBatteryState)(PifBattery* p_owner, PifBatteryState old_state);


/**
 * @class StPifBattery
 * @brief Battery monitor with cell detection, consumption and voltage warnings.
 */
struct StPifBattery
{
	// Public Event Function
	PifEvtBatteryState evt_state;

	// Read-only Member Variable
	PifId _id;
	PifTask* _p_task;
	PifBatteryState _state;
	uint8_t _cell_count;		// 0 until detected
	uint32_t _voltage_mv;		// Filtered pack voltage
	int32_t _current_ma;		// Filtered current, 0 without a current sensor
	uint32_t _consumed_mah;		// Charge drawn since the battery was connected
	uint32_t _consumed_mwh;		// Energy drawn since the battery was connected
	BOOL _full_at_start;		// The cells were at least at the full voltage when detected

	// Private Member Variable
	PifBatteryConfig __config;
	uint8_t __forced_cells;
	uint32_t __capacity_mah;
	PifPtFilter __voltage_lpf;
	PifPtFilter __current_lpf;
	BOOL __primed;				// The filters hold a first reading
	BOOL __timed;				// __last_us holds the time of the last update
	uint32_t __last_us;
	uint32_t __settle_elapsed_us;
	uint32_t __settle_ref_mv;
	uint32_t __below_warning_us;	// Time the cell voltage has been below the warning threshold
	uint32_t __below_min_us;		// Time the cell voltage has been below the minimum
	BOOL __hold_presence;
	int64_t __charge_ma_us;		// Charge drawn, in mA * us; charging counts down
	int64_t __energy_mw_us;		// Energy drawn, in mW * us

	// Private Action Function
	PifActBatteryRead __act_voltage;
	PifActBatteryRead __act_current;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBattery_Init
 * @brief Initializes a battery monitor.
 * @param p_owner Pointer to the instance.
 * @param id Identifier, or PIF_ID_AUTO.
 * @param p_config Thresholds and timing, for example &pif_battery_lipo. They are copied.
 * @param act_voltage Gives the pack voltage in millivolts. May be NULL if only
 *        pifBattery_Process() is used.
 * @return TRUE on success, otherwise FALSE (E_INVALID_PARAM for a missing or inconsistent config).
 */
BOOL pifBattery_Init(PifBattery* p_owner, PifId id, const PifBatteryConfig* p_config, PifActBatteryRead act_voltage);

/**
 * @fn pifBattery_Clear
 * @brief Removes the task of the instance, if it has one.
 * @param p_owner Pointer to the instance.
 */
void pifBattery_Clear(PifBattery* p_owner);

/**
 * @fn pifBattery_AttachActCurrent
 * @brief Gives the instance a current sensor, which enables the consumption.
 * @param p_owner Pointer to the instance.
 * @param act_current Gives the current in milliamps.
 */
void pifBattery_AttachActCurrent(PifBattery* p_owner, PifActBatteryRead act_current);

/**
 * @fn pifBattery_AttachTask
 * @brief Updates the monitor from a task of its own.
 * @param p_owner Pointer to the instance.
 * @param period1us Period of the task in microseconds.
 * @param start TRUE to start the task now.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifBattery_AttachTask(PifBattery* p_owner, uint32_t period1us, BOOL start);

/**
 * @fn pifBattery_SetConfig
 * @brief Replaces the thresholds and timing. A connected battery goes back to BAS_INIT, and its
 *        cells are counted again with the new thresholds once the voltage has settled.
 * @param p_owner Pointer to the instance.
 * @param p_config New thresholds and timing. They are copied.
 * @return TRUE on success, otherwise FALSE with the old configuration kept.
 */
BOOL pifBattery_SetConfig(PifBattery* p_owner, const PifBatteryConfig* p_config);

/**
 * @fn pifBattery_GetConfig
 * @brief Returns the thresholds and timing in use.
 * @param p_owner Pointer to the instance.
 * @return Configuration of the instance.
 */
const PifBatteryConfig* pifBattery_GetConfig(PifBattery* p_owner);

/**
 * @fn pifBattery_SetCellCount
 * @brief Fixes the number of cells instead of detecting it.
 *
 * Detection takes the fewest cells that keep each one at or below cell_max_mv, which is right for
 * n cells only while each is above cell_max_mv * (n - 1) / n: 3.23 V for a 4S lithium polymer
 * pack, but 3.58 V for 6S. Fix the count for packs of many cells that may be connected partly
 * discharged, and for nickel-metal hydride, whose cell voltage varies too widely to count by.
 * @param p_owner Pointer to the instance.
 * @param cells 1 to PIF_BATTERY_MAX_CELLS, or 0 to detect.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifBattery_SetCellCount(PifBattery* p_owner, uint8_t cells);

/**
 * @fn pifBattery_SetCapacity
 * @brief Sets the capacity, which makes the remaining charge come from the consumption.
 * @param p_owner Pointer to the instance.
 * @param capacity_mah Capacity in mAh, or 0 to use the voltage.
 */
void pifBattery_SetCapacity(PifBattery* p_owner, uint32_t capacity_mah);

/**
 * @fn pifBattery_Update
 * @brief Reads the voltage and the current through the actions and processes them, with the time
 *        since the last update from pif_act_timer1us.
 * @param p_owner Pointer to the instance.
 */
void pifBattery_Update(PifBattery* p_owner);

/**
 * @fn pifBattery_Process
 * @brief Processes one reading, for a client that measures without the actions.
 * @param p_owner Pointer to the instance.
 * @param voltage_mv Pack voltage in millivolts.
 * @param current_ma Current in milliamps, positive while discharging, 0 without a sensor.
 * @param elapsed_us Time since the previous reading. 0 for the first one.
 */
void pifBattery_Process(PifBattery* p_owner, uint32_t voltage_mv, int32_t current_ma, uint32_t elapsed_us);

/**
 * @fn pifBattery_HoldPresence
 * @brief Freezes the connection state, for example while a vehicle is in use: a connected battery
 *        is not taken as unplugged however far its voltage sags, and a battery is not detected
 *        or counted. The voltage states keep being worked out.
 * @param p_owner Pointer to the instance.
 * @param hold TRUE to freeze, FALSE to detect again.
 */
void pifBattery_HoldPresence(PifBattery* p_owner, BOOL hold);

/**
 * @fn pifBattery_SetConsumedMah
 * @brief Sets the charge drawn, for a battery whose consumption is measured elsewhere, such as by a
 *        smart battery or an ESC. Readings with a current keep adding to it.
 * @param p_owner Pointer to the instance.
 * @param consumed_mah Charge drawn in mAh.
 */
void pifBattery_SetConsumedMah(PifBattery* p_owner, uint32_t consumed_mah);

/**
 * @fn pifBattery_ResetConsumption
 * @brief Clears the charge and energy drawn.
 * @param p_owner Pointer to the instance.
 */
void pifBattery_ResetConsumption(PifBattery* p_owner);

/**
 * @fn pifBattery_GetCellVoltage
 * @brief Returns the average cell voltage.
 * @param p_owner Pointer to the instance.
 * @return Cell voltage in millivolts, or 0 before the cells are counted.
 */
uint16_t pifBattery_GetCellVoltage(PifBattery* p_owner);

/**
 * @fn pifBattery_GetRemainingPercent
 * @brief Returns the remaining charge: from the consumption if a capacity is set, otherwise from
 *        the cell voltage between the minimum (0 %) and full (100 %) voltages.
 * @param p_owner Pointer to the instance.
 * @return 0 to 100, or 0 before the cells are counted.
 */
uint8_t pifBattery_GetRemainingPercent(PifBattery* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BATTERY_H
