// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_hc_sr04.h
 * @brief HC-SR04 ultrasonic distance sensor.
 *
 * A task triggers a measurement, the platform gives the edges of the echo with pifHcSr04_sigReceiveEcho(), and
 * the distance in cm goes to evt_read. The speed of sound can be corrected for the temperature.
 */
#ifndef PIF_HC_SR04_H
#define PIF_HC_SR04_H


#include "core/pif_task_manager.h"
#include "sensor/pif_sensor_event.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_HC_SR04)
	#define COLLECT_SIGNAL_HC_SR04	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_HC_SR04 is too
	#include "core/pif_collect_signal.h"
#endif


/// How long a measurement may wait for its echo, from the trigger to the rising edge or from there
/// to the falling edge, before the next visit of the task triggers again. The echo of the sensor
/// lasts at most about 38ms, which is what it gives when nothing is in range.
#ifndef PIF_HC_SR04_ECHO_TIMEOUT_US
#define PIF_HC_SR04_ECHO_TIMEOUT_US		60000UL
#endif


/**
 * @brief Step of a measurement.
 */
typedef enum EnPifHcSr04State
{
	HSS_READY		= 0,	///< Ready to trigger
	HSS_TRIGGER		= 1,	///< Trigger pulse given, waiting for the echo to rise
	HSS_HIGH		= 2,	///< Echo is high
	HSS_LOW			= 3	///< Echo fell, the distance is known
} PifHcSr04State;

#ifdef COLLECT_SIGNAL_HC_SR04

/**
 * @brief Signals of an HC-SR04 for pifHcSr04_SetCsFlag() and pifHcSr04_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifHcSr04CsFlag
{
	HS_CSF_OFF				= 0,

	HS_CSF_STATE_IDX		= 0,	///< PifHcSr04State
	HS_CSF_DISTANCE_IDX		= 1,	///< Distance given to evt_read

	HS_CSF_STATE_BIT		= 1,
	HS_CSF_DISTANCE_BIT		= 2,
	HS_CSF_ALL_BIT			= 3,

	HS_CSF_COUNT			= 2
} PifHcSr04CsFlag;

#endif	// COLLECT_SIGNAL_HC_SR04

/**
 * @brief Drives the trigger pin of the sensor.
 */
typedef void (*PifActHcSr04Trigger)(SWITCH state);


/**
 * @class StPifHcSr04
 * @brief HC-SR04 ultrasonic distance sensor.
 */
typedef struct StPifHcSr04
{
	// Public Member Variable
	PifActHcSr04Trigger act_trigger;	///< Drives the trigger pin. Required.
	PifEvtSonarRead evt_read;	///< Gets each distance in cm. May be NULL.

	// Read-only Member Variable
    PifTask* _p_task;	///< Measurement task
    float _transform_const;	///< Echo time in us per cm of distance, from the speed of sound
	PifId _id;	///< Instance ID

	// Private Member Variable
	/// Shared with pifHcSr04_sigReceiveEcho(), which is normally called from an interrupt.
    volatile PifHcSr04State __state;
    volatile uint32_t __trigger_time_us;
	volatile int32_t __distance;
#ifdef COLLECT_SIGNAL_HC_SR04
	PifCollectSignalChannel __cs[HS_CSF_COUNT];
#endif
} PifHcSr04;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up the sensor with a measurement task of 50 ms, paused, and the speed of sound at 20 degrees C.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHcSr04_Init(PifHcSr04* p_owner, PifId id);

/**
 * @brief Removes the measurement task.
 * @param p_owner Pointer to the owner instance.
 */
void pifHcSr04_Clear(PifHcSr04* p_owner);

/**
 * @brief Gives the 10 us trigger pulse that starts a measurement.
 * @param p_owner Pointer to the owner instance.
 * @return TRUE on success, FALSE when act_trigger is not set.
 */
BOOL pifHcSr04_Trigger(PifHcSr04* p_owner);

/**
 * @brief Triggers a measurement every period. The sensor wants at least 60ms between
 *        measurements, so that an echo of the previous one is not taken for the next.
 * @param p_owner Pointer to the owner instance.
 * @param period1ms Trigger period in units of 1 ms.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period1ms);

/**
 * @brief Stops the periodic trigger. A measurement on its way is dropped.
 * @param p_owner Pointer to the owner instance.
 */
void pifHcSr04_StopTrigger(PifHcSr04* p_owner);

/**
 * @brief Sets the air temperature, which sets the speed of sound used for the distance.
 * @param p_owner Pointer to the owner instance.
 * @param temperature Temperature value in degrees Celsius.
 */
void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature);

/**
 * @brief Gives an edge of the echo pin. Call it from the edge interrupt.
 * @param p_owner Pointer to the owner instance.
 * @param state Level of the echo pin after the edge.
 */
void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state);

#ifdef COLLECT_SIGNAL_HC_SR04

/**
 * @brief Adds the selected signals of the sensor to pifCollectSignal as channels.
 * @param p_owner Pointer to the sensor instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifHcSr04_SetCsFlag(PifHcSr04* p_owner, PifHcSr04CsFlag flag);

/**
 * @brief Removes the selected signals of the sensor from pifCollectSignal.
 * @param p_owner Pointer to the sensor instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifHcSr04_ResetCsFlag(PifHcSr04* p_owner, PifHcSr04CsFlag flag);

#endif	// COLLECT_SIGNAL_HC_SR04

#ifdef __cplusplus
}
#endif


#endif  // PIF_HC_SR04_H
