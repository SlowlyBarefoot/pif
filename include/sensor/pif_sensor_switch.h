// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sensor_switch.h
 * @brief ON/OFF input such as a limit switch or a photo interrupter, debounced and reported on change.
 *
 * A PifSensor whose samples come from act_acquire on a task or from pifSensorSwitch_sigData(), optionally
 * through a bit noise filter.
 */
#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_SENSOR_SWITCH)
	#define COLLECT_SIGNAL_SENSOR_SWITCH	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_SENSOR_SWITCH is too
	#include "core/pif_collect_signal.h"
#endif


struct StPifSensorSwitch;
typedef struct StPifSensorSwitch PifSensorSwitch;

#ifdef COLLECT_SIGNAL_SENSOR_SWITCH

/**
 * @brief Signals of a switch sensor for pifSensorSwitch_SetCsFlag() and pifSensorSwitch_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifSensorSwitchCsFlag
{
    SS_CSF_OFF			= 0,

    SS_CSF_RAW_IDX		= 0,	///< Raw sample
	SS_CSF_FILTER_IDX	= 1,	///< Filtered state

	SS_CSF_RAW_BIT		= 1,
    SS_CSF_FILTER_BIT	= 2,
    SS_CSF_ALL_BIT		= 3,

    SS_CSF_COUNT		= 2
} PifSensorSwitchCsFlag;

#endif	// COLLECT_SIGNAL_SENSOR_SWITCH

/**
 * @class StPifSensorSwitch
 * @brief An ON/OFF input such as a limit switch, a photo interrupter or a door sensor, debounced by
 *        the filter of pifSensorSwitch_AttachFilter() (a PifNfBit) and reported on every change. Buttons
 *        that need clicks or long presses belong to pifKeypad.
 */
struct StPifSensorSwitch
{
	/// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Private Member Variable
    PifNoiseFilter* __p_filter;
    SWITCH __state;

#ifdef COLLECT_SIGNAL_SENSOR_SWITCH
    PifCollectSignalChannel __cs[SS_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a switch sensor.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param init_state State until the first sample.
 * @param act_acquire Reads the switch for pifSensorSwitch_ProcessAcquire() and the task of pifSensorSwitch_AttachTaskAcquire(). NULL when the samples are given with
 *                    pifSensorSwitch_sigData().
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire);

/**
 * @brief Removes the collected signals of the sensor.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorSwitch_Clear(PifSensorSwitch* p_owner);

/**
 * @brief Attaches a filter that every sample goes through, such as a PifNfBit.
 * @param p_owner Pointer to the owner instance.
 * @param p_filter Filter that takes and gives SWITCH (NFV_SWITCH), or NULL to detach it. It must stay valid
 *                 while it is attached.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when the filter takes another type.
 */
BOOL pifSensorSwitch_AttachFilter(PifSensorSwitch* p_owner, PifNoiseFilter* p_filter);

/**
 * @brief Puts the sensor back to its initial state, without a change event.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorSwitch_InitialState(PifSensorSwitch* p_owner);

/**
 * @brief Gives a sample, through the attached filter if any, for sensors without act_acquire. It may be
 *        called from an interrupt, and the state changes at the next pifSensorSwitch_ProcessAcquire().
 * @param p_owner Pointer to the owner instance.
 * @param state Sampled state. Any value but OFF counts as ON.
 */
void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state);

/**
 * @brief Takes a sample from act_acquire when it is set, and calls the change event when the
 *        filtered state differs from _curr_state.
 *
 * pifSensorSwitch_AttachTaskAcquire() runs it on a task of the sensor's own. To poll several
 * sensors on one task instead, call it for each of them from that task. Sensors without
 * act_acquire can then share one read of the port, given to each with pifSensorSwitch_sigData():
 *
 *     static uint32_t _taskSwitches(PifTask* p_task)
 *     {
 *         uint8_t port = readPort();
 *
 *         for (int i = 0; i < SWITCH_COUNT; i++) {
 *             pifSensorSwitch_sigData(&s_switch[i], (port >> i) & 1);
 *             pifSensorSwitch_ProcessAcquire(&s_switch[i]);
 *         }
 *         return 0;
 *     }
 *
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorSwitch_ProcessAcquire(PifSensorSwitch* p_owner);

/**
 * @brief Creates a task of the sensor's own that calls pifSensorSwitch_ProcessAcquire(). For several
 *        sensors, one task that calls it for each of them saves the tasks.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param mode Task operating mode that controls scheduling behavior.
 * @param period Task period value; its unit depends on the selected mode.
 * @param start Set to TRUE to start the task immediately.
 * @return Pointer to the created or selected object, or NULL on failure.
 */
PifTask *pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef COLLECT_SIGNAL_SENSOR_SWITCH

/**
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSensorSwitch_SetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag);

/**
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifSensorSwitch_ResetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag);

#endif	// COLLECT_SIGNAL_SENSOR_SWITCH

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
