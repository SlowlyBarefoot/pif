// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_sensor_threshold.h
 * @brief Analog level turned into an ON/OFF state by a threshold with hysteresis.
 *
 * A PifSensor whose level comes from act_acquire on a task or from pifSensorThreshold_sigData(), optionally
 * through an integer noise filter. The state turns ON at or above the high threshold and OFF below the low one.
 */
#ifndef PIF_SENSOR_THRESHOLD_H
#define PIF_SENSOR_THRESHOLD_H


#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_SENSOR_THRESHOLD)
	#define COLLECT_SIGNAL_SENSOR_THRESHOLD	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_SENSOR_THRESHOLD is too
	#include "core/pif_collect_signal.h"
#endif


struct StPifSensorThreshold;
typedef struct StPifSensorThreshold PifSensorThreshold;


#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD

/**
 * @brief Signals of a threshold sensor for pifSensorThreshold_SetCsFlag() and pifSensorThreshold_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifSensorThresholdCsFlag
{
    ST_CSF_OFF			= 0,

    ST_CSF_STATE_IDX	= 0,	///< ON/OFF state
    ST_CSF_LEVEL_IDX	= 1,	///< Level

	ST_CSF_STATE_BIT	= 1,
	ST_CSF_LEVEL_BIT	= 2,
    ST_CSF_ALL_BIT		= 3,

    ST_CSF_COUNT		= 2
} PifSensorThresholdCsFlag;

#endif	// COLLECT_SIGNAL_SENSOR_THRESHOLD

/**
 * @class StPifSensorThreshold
 * @brief Turns a level, such as an ADC reading, into an ON/OFF state with a hysteresis (see
 *        pifSensor_Hysteresis()), and reports every change with the level as an int32_t*.
 *
 * The state stays as it is until pifSensorThreshold_SetThreshold() succeeds and a level is given.
 */
struct StPifSensorThreshold
{
	/// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Read-only Member Variable
    int32_t _curr_level;			///< Last level after the attached filter

	// Private Member Variable
    PifNoiseFilter* __p_filter;
	int32_t __low_threshold;
	int32_t __high_threshold;
	BOOL __threshold_set;
	BOOL __level_set;

#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD
    PifCollectSignalChannel __cs[ST_CSF_COUNT];
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up a threshold sensor.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param init_state State until the level first crosses a threshold, and after
 *        pifSensorThreshold_InitialState().
 * @param act_acquire Returns the level, or NULL to give it with pifSensorThreshold_sigData().
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifSensorThreshold_Init(PifSensorThreshold* p_owner, PifId id, SWITCH init_state, PifActSensorAcquire act_acquire);

/**
 * @brief Removes the collected signals of the sensor.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorThreshold_Clear(PifSensorThreshold* p_owner);

/**
 * @brief Attaches a filter that every level goes through, such as a PifNfInt32Average.
 * @param p_owner Pointer to the owner instance.
 * @param p_filter Filter that takes and gives int32_t (NFV_INT32), or NULL to detach it. It must stay valid
 *                 while it is attached.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when the filter takes another type.
 */
BOOL pifSensorThreshold_AttachFilter(PifSensorThreshold* p_owner, PifNoiseFilter* p_filter);

/**
 * @brief Returns to the initial state and forgets the last level, without calling the change event.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorThreshold_InitialState(PifSensorThreshold* p_owner);

/**
 * @brief Sets the hysteresis: the state turns ON at or above high_threshold and OFF below
 *        low_threshold. Equal thresholds make a single threshold.
 * @param p_owner Pointer to the owner instance.
 * @param low_threshold Lower threshold level.
 * @param high_threshold Upper threshold level.
 * @return TRUE on success, FALSE when low_threshold is above high_threshold.
 */
BOOL pifSensorThreshold_SetThreshold(PifSensorThreshold* p_owner, int32_t low_threshold, int32_t high_threshold);

/**
 * @brief Gives a level, through the attached filter if any, for sensors without act_acquire. It may be
 *        called from an interrupt, and the state changes at the next pifSensorThreshold_ProcessAcquire().
 *        Levels given while the filter is still filling its window are not used.
 * @param p_owner Pointer to the owner instance.
 * @param level Sampled level.
 */
void pifSensorThreshold_sigData(PifSensorThreshold* p_owner, int32_t level);

/**
 * @brief Takes a level from act_acquire when it is set, and applies the thresholds to the last level.
 *
 * pifSensorThreshold_AttachTaskAcquire() runs it on a task of the sensor's own. To poll several
 * sensors on one task instead, call it for each of them from that task. Sensors without
 * act_acquire can then take their levels from one scan, given to each with
 * pifSensorThreshold_sigData():
 *
 *     static uint32_t _taskLevels(PifTask* p_task)
 *     {
 *         for (int i = 0; i < SENSOR_COUNT; i++) {
 *             pifSensorThreshold_sigData(&s_sensor[i], s_adc[i]);
 *             pifSensorThreshold_ProcessAcquire(&s_sensor[i]);
 *         }
 *         return 0;
 *     }
 *
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorThreshold_ProcessAcquire(PifSensorThreshold* p_owner);

/**
 * @brief Creates a task of the sensor's own that calls pifSensorThreshold_ProcessAcquire(). For
 *        several sensors, one task that calls it for each of them saves the tasks.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param mode Task operating mode that controls scheduling behavior.
 * @param period Task period value; its unit depends on the selected mode.
 * @param start Set to TRUE to start the task immediately.
 * @return Pointer to the created or selected object, or NULL on failure.
 */
PifTask* pifSensorThreshold_AttachTaskAcquire(PifSensorThreshold* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef COLLECT_SIGNAL_SENSOR_THRESHOLD

/**
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifSensorThreshold_SetCsFlag(PifSensorThreshold* p_owner, PifSensorThresholdCsFlag flag);

/**
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifSensorThreshold_ResetCsFlag(PifSensorThreshold* p_owner, PifSensorThresholdCsFlag flag);

#endif	// COLLECT_SIGNAL_SENSOR_THRESHOLD

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_THRESHOLD_H
