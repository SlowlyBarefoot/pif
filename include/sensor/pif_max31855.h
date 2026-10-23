// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_max31855.h
 * @brief MAX31855 thermocouple converter on SPI, as a PifSensor.
 *
 * A task reads the thermocouple and the cold junction temperatures, and the sensor turns ON or OFF at a
 * temperature threshold.
 */
#ifndef PIF_MAX31855_H
#define PIF_MAX31855_H


#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_MAX31855)
	#define COLLECT_SIGNAL_MAX31855	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_MAX31855 is too
	#include "core/pif_collect_signal.h"
#endif


struct StPifMax31855;
typedef struct StPifMax31855 PifMax31855;

/**
 * @brief Gets each temperature measured, in degree Celsius.
 */
typedef void (*PifEvtMax31855Measure)(PifMax31855* p_owner, double temperature, PifIssuerP p_issuer);


#ifdef COLLECT_SIGNAL_MAX31855

/**
 * @brief Signals of a MAX31855 for pifMax31855_SetCsFlag() and pifMax31855_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifMax31855CsFlag
{
    M3_CSF_OFF			= 0,

    M3_CSF_STATE_IDX	= 0,

	M3_CSF_STATE_BIT	= 1,
    M3_CSF_ALL_BIT		= 1,

    M3_CSF_COUNT		= 1
} PifMax31855CsFlag;

#endif	// COLLECT_SIGNAL_MAX31855

/**
 * @class StPifMax31855
 * @brief MAX31855 thermocouple converter.
 */
struct StPifMax31855
{
	/// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Read-only Member Variable
	PifDeviceReg *_p_device;		///< SPI device of the converter
    PifTask* _p_task;				///< Measurement task
	double _internal;				///< Cold junction temperature of the last periodic measurement, in degree Celsius

	// Private Member Variable
    PifNoiseFilter* __p_filter;
	int32_t __low_threshold;			///< In 0.25 degree units, rounded up
	int32_t __high_threshold;
	BOOL __threshold_set;
    PifIssuerP __p_issuer;

#ifdef COLLECT_SIGNAL_MAX31855
    PifCollectSignalChannel __cs[M3_CSF_COUNT];
#endif

	// Private Event Function
	PifEvtMax31855Measure __evt_measure;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Adds the MAX31855 to the SPI port.
 * @param p_owner Pointer to the owner instance.
 * @param id Instance ID, or PIF_ID_AUTO.
 * @param p_port SPI port the device is on.
 * @param p_client Client pointer handed to the port functions of the device.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port, void *p_client);

/**
 * @brief Stops the measurement and removes the device from the port.
 * @param p_owner Pointer to the owner instance.
 */
void pifMax31855_Clear(PifMax31855* p_owner);

/**
 * @brief Attaches a filter that every thermocouple measure goes through, such as a PifNfInt16Median. The cold
 *        junction temperature is not filtered.
 * @param p_owner Pointer to the owner instance.
 * @param p_filter Filter that takes and gives int16_t (NFV_INT16) in 0.25 degree units, or NULL to detach it. It
 *                 must stay valid while it is attached.
 * @return TRUE on success, FALSE with E_INVALID_PARAM when the filter takes another type.
 */
BOOL pifMax31855_AttachFilter(PifMax31855* p_owner, PifNoiseFilter* p_filter);

/**
 * @brief Reads the thermocouple and the cold junction temperatures, waiting for the transfer.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Receives the thermocouple temperature in degree Celsius. May be NULL.
 * @param p_internal Receives the cold junction temperature in degree Celsius. May be NULL.
 * @return TRUE on success, FALSE when the transfer failed or the converter reports a fault.
 */
BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal);

/**
 * @brief Measures every period on a task of its own. Fails with E_INVALID_STATE while that task
 *        is already running.
 * @param p_owner Pointer to the owner instance.
 * @param period1ms Task period in units of 1 ms.
 * @param evt_measure Gets each temperature. May be NULL.
 * @param p_issuer Pointer handed to evt_measure.
 * @param name Name of the task. If NULL, "MAX31855" is used.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period1ms,
	PifEvtMax31855Measure evt_measure, PifIssuerP p_issuer, const char *name);

/**
 * @brief Removes the measurement task.
 * @param p_owner Pointer to the owner instance.
 */
void pifMax31855_StopMeasurement(PifMax31855* p_owner);

/**
 * @brief Sets the hysteresis of the state given to the change event: it turns ON at or above
 *        high_threshold and OFF below low_threshold. The state stays as it is until this is called.
 * @param p_owner Pointer to the owner instance.
 * @param low_threshold Lower threshold level.
 * @param high_threshold Upper threshold level.
 * @return TRUE on success, FALSE when low_threshold is above high_threshold.
 */
BOOL pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold);

#ifdef COLLECT_SIGNAL_MAX31855

/**
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifMax31855_SetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

/**
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifMax31855_ResetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

#endif	// COLLECT_SIGNAL_MAX31855

#ifdef __cplusplus
}
#endif


#endif  // PIF_MAX31855_H
