// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_SENSOR_SWITCH_H
#define PIF_SENSOR_SWITCH_H


#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


struct StPifSensorSwitch;
typedef struct StPifSensorSwitch PifSensorSwitch;

#ifdef PIF_COLLECT_SIGNAL

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

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifSensorSwitch
 * @brief Defines the st pif sensor switch data structure.
 */
struct StPifSensorSwitch
{
	// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Private Member Variable
    SWITCH __state;

#ifdef PIF_COLLECT_SIGNAL
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
 * @brief Performs the sensor switch sig data operation.
 * @param p_owner Pointer to the owner instance.
 * @param swState Parameter swState used by this operation.
 */
void pifSensorSwitch_sigData(PifSensorSwitch* p_owner, SWITCH state);

/**
 * @fn pifSensorSwitch_ProcessAcquire
 * @brief Processes one acquisition cycle for sensor switch process acquire.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
uint16_t pifSensorSwitch_ProcessAcquire(PifSensorSwitch* p_owner);

/**
 * @fn pifSensorSwitch_AttachTaskAcquire
 * @brief Creates and attaches a task for sensor switch attach task acquire processing.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param mode Task operating mode that controls scheduling behavior.
 * @param period Task period value; its unit depends on the selected mode.
 * @param start Set to TRUE to start the task immediately.
 * @return Pointer to the created or selected object, or NULL on failure.
 */
PifTask *pifSensorSwitch_AttachTaskAcquire(PifSensorSwitch* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef PIF_COLLECT_SIGNAL

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

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_SWITCH_H
