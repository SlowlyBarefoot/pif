#ifndef PIF_SENSOR_DIGITAL_H
#define PIF_SENSOR_DIGITAL_H


#include "core/pif_task_manager.h"
#include "core/pif_timer.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"


struct StPifSensorDigital;
typedef struct StPifSensorDigital PifSensorDigital;


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifSensorDigitalCsFlag
{
    SD_CSF_OFF			= 0,

    SD_CSF_STATE_IDX	= 0,

	SD_CSF_STATE_BIT	= 1,
    SD_CSF_ALL_BIT		= 1,

    SD_CSF_COUNT		= 1
} PifSensorDigitalCsFlag;

typedef struct StPifSensorDigitalColSig
{
	PifSensorDigital* p_owner;
    uint8_t flag;
    void* p_device[SD_CSF_COUNT];
} PifSensorDigitalColSig;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifSensorDigital
 * @brief Defines the st pif sensor digital data structure.
 */
struct StPifSensorDigital
{
	// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Private Member Variable
	uint16_t __low_threshold;
	uint16_t __high_threshold;
    uint16_t __curr_level;
    uint16_t __prev_level;

#ifdef PIF_COLLECT_SIGNAL
    PifSensorDigitalColSig* __p_colsig;
#endif

	// Private Event Function
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSensorDigital_Init
 * @brief Initializes sensor digital init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param act_acquire Parameter act_acquire used by this operation.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifSensorDigital_Init(PifSensorDigital* p_owner, PifId id, PifActSensorAcquire act_acquire);

/**
 * @fn pifSensorDigital_Clear
 * @brief Releases resources used by sensor digital clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorDigital_Clear(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_InitialState
 * @brief Initializes sensor digital initial state and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 */
void pifSensorDigital_InitialState(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_SetThreshold
 * @brief Sets configuration values required by sensor digital set threshold.
 * @param p_owner Pointer to the owner instance.
 * @param low_threshold Lower threshold level.
 * @param high_threshold Upper threshold level.
 */
void pifSensorDigital_SetThreshold(PifSensorDigital* p_owner, uint16_t low_threshold, uint16_t high_threshold);

/**
 * @fn pifSensorDigital_sigData
 * @brief Performs the sensor digital sig data operation.
 * @param p_owner Pointer to the owner instance.
 * @param level Sampled signal level value.
 */
void pifSensorDigital_sigData(PifSensorDigital* p_owner, uint16_t level);

/**
 * @fn pifSensorDigital_ProcessAcquire
 * @brief Processes one acquisition cycle for sensor digital process acquire.
 * @param p_owner Pointer to the owner instance.
 * @return Computed integer value.
 */
uint16_t pifSensorDigital_ProcessAcquire(PifSensorDigital* p_owner);

/**
 * @fn pifSensorDigital_AttachTaskAcquire
 * @brief Creates and attaches a task for sensor digital attach task acquire processing.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param mode Task operating mode that controls scheduling behavior.
 * @param period Task period value; its unit depends on the selected mode.
 * @param start Set to TRUE to start the task immediately.
 * @return Pointer to the created or selected object, or NULL on failure.
 */
PifTask* pifSensorDigital_AttachTaskAcquire(PifSensorDigital* p_owner, PifId id, PifTaskMode mode, uint32_t period, BOOL start);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSensorDigital_SetCsFlag
 * @brief Sets configuration values required by sensor digital set cs flag.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit flag value to set or clear.
 */
void pifSensorDigital_SetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigital_ResetCsFlag
 * @brief Sets configuration values required by sensor digital reset cs flag.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit flag value to set or clear.
 */
void pifSensorDigital_ResetCsFlag(PifSensorDigital* p_owner, PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigitalColSig_SetFlag
 * @brief Sets configuration values required by sensor digital col sig set flag.
 * @param flag Bit flag value to set or clear.
 */
void pifSensorDigitalColSig_SetFlag(PifSensorDigitalCsFlag flag);

/**
 * @fn pifSensorDigitalColSig_ResetFlag
 * @brief Sets configuration values required by sensor digital col sig reset flag.
 * @param flag Bit flag value to set or clear.
 */
void pifSensorDigitalColSig_ResetFlag(PifSensorDigitalCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_DIGITAL_H
