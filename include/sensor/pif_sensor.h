// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sensor.h
 * @brief Base of the digital sensors: a state that turns ON or OFF, reported by a change event.
 *
 * PifSensorSwitch, PifSensorThreshold and PifMax31855 embed a PifSensor as their first member, so the motor and
 * input modules take any of them through a PifSensor pointer.
 */
#ifndef PIF_SENSOR_H
#define PIF_SENSOR_H


#include "core/pif.h"


/// The value that a change event points to depends on the sensor, and is valid only during the event:
///   PifSensorSwitch     NULL
///   PifSensorThreshold  int32_t*, the level that crossed the threshold
///   PifMax31855         double*, the temperature in degree Celsius
typedef void* PifSensorValueP;

struct StPifSensor;
typedef struct StPifSensor PifSensor;

/// Returns the sample: OFF or ON for PifSensorSwitch, the level for PifSensorThreshold.
typedef int32_t (*PifActSensorAcquire)(PifSensor* p_owner);
/**
 * @brief Reports that the state of a sensor changed.
 *
 * p_value points to the value of the sensor during the call. See PifSensorValueP.
 */
typedef void (*PifEvtSensorChange)(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer);


/**
 * @class StPifSensor
 * @brief State of a sensor, shared by its kinds.
 */
struct StPifSensor
{
	// Read-only Member Variable
	PifId _id;								///< Sensor ID
    SWITCH _init_state;						///< State at the start
    SWITCH _curr_state;						///< Default: _init_state

	// Private Member Variable
    PifIssuerP __p_issuer;

	// Private Action Function
	PifActSensorAcquire __act_acquire;

	// Private Event Function
    PifEvtSensorChange __evt_change;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the callback called when the state of the sensor changes.
 * @param p_owner Pointer to the owner instance.
 * @param evt_change Callback.
 * @param p_issuer Pointer handed to the callback.
 */
void pifSensor_AttachEvtChange(PifSensor *p_owner, PifEvtSensorChange evt_change, PifIssuerP p_issuer);

/**
 * @brief Removes the change callback.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensor_DetachEvtChange(PifSensor *p_owner);

/**
 * @brief Sets _curr_state and, when it changed, calls the change event with the new state already
 *        set. For the sensors built on PifSensor.
 * @param p_owner Pointer to the owner instance.
 * @param state New state.
 * @param p_value Value given to the change event, see PifSensorValueP.
 * @return TRUE when the state changed.
 */
BOOL pifSensor_ChangeState(PifSensor *p_owner, SWITCH state, PifSensorValueP p_value);

/**
 * @brief Applies a hysteresis to a level: OFF turns ON at or above high_threshold, and ON turns OFF
 *        below low_threshold. Below rather than at, so that equal thresholds make a single threshold
 *        that does not toggle on every sample at its own level.
 * @param state Current state.
 * @param level Level to compare.
 * @param low_threshold Lower threshold level, not above high_threshold.
 * @param high_threshold Upper threshold level.
 * @return The new state.
 */
SWITCH pifSensor_Hysteresis(SWITCH state, int32_t level, int32_t low_threshold, int32_t high_threshold);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
