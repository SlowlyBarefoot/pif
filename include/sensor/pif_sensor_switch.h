// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_SENSOR_SWITCH)
	#define COLLECT_SIGNAL_SENSOR_SWITCH
	#include "core/pif_collect_signal.h"
#endif


struct StPifSensorSwitch;
typedef struct StPifSensorSwitch PifSensorSwitch;

#ifdef COLLECT_SIGNAL_SENSOR_SWITCH

typedef enum EnPifSensorSwitchCsFlag
{
    SS_CSF_OFF			= 0,

    SS_CSF_RAW_IDX		= 0,
	SS_CSF_FILTER_IDX	= 1,

	SS_CSF_RAW_BIT		= 1,
    SS_CSF_FILTER_BIT	= 2,
    SS_CSF_ALL_BIT		= 3,

    SS_CSF_COUNT		= 2
} PifSensorSwitchCsFlag;

#endif	// COLLECT_SIGNAL_SENSOR_SWITCH

/**
 * @class StPifSensorSwitch
 * @brief An ON/OFF input such as a limit switch, a photo interrupter or a door sensor, debounced by
 *        p_filter (a pifNoiseFilterBit) and reported on every change. Buttons that need clicks or
 *        long presses belong to pifKeypad.
 */
struct StPifSensorSwitch
{
	// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Private Member Variable
    SWITCH __state;

#ifdef COLLECT_SIGNAL_SENSOR_SWITCH
    PifCollectSignalChannel __cs[SS_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSensorSwitch_Init
 * @brief Initializes sensor switch init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param init_state Parameter init_state used by this operation.
 * @param act_acquire Parameter act_acquire used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifSensorSwitch_Init(PifSensorSwitch* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire);

/**
 * @fn pifSensorSwitch_Clear
 * @brief Releases resources used by sensor switch clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorSwitch_Clear(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_InitialState
 * @brief Initializes sensor switch initial state and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorSwitch_InitialState(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_sigData
 * @brief Gives a sample, through p_filter when it is set, for sensors without act_acquire. It may be
 *        called from an interrupt, and the state changes at the next pifSensorSwitch_ProcessAcquire().
 * @param p_owner Pointer to the owner instance.
 * @param state Sampled state. Any value but OFF counts as ON.
 */
void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state);

/**
 * @fn pifSensorSwitch_ProcessAcquire
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
 * @fn pifSensorSwitch_AttachTaskAcquire
 * @brief Creates a task of the sensor's own that calls pifSensorSwitch_ProcessAcquire(). For several
 *        sensors, one task that calls it for each of them saves the tasks.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param mode Task operating mode that controls scheduling behavior.
 * @param period Task period value; its unit depends on the selected mode.
 * @param start Set to TRUE to start the task immediately.
 * @return Pointer to the created or selected object, or NULL on failure.
 */
PifTask *pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef COLLECT_SIGNAL_SENSOR_SWITCH

/**
 * @fn pifSensorSwitch_SetCsFlag
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSensorSwitch_SetCsFlag(PifSensorSwitch* p_owner, PifSensorSwitchCsFlag flag);

/**
 * @fn pifSensorSwitch_ResetCsFlag
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
