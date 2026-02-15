#ifndef PIF_MAX31855_H
#define PIF_MAX31855_H


#include "communication/pif_spi.h"
#include "core/pif_task_manager.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"


struct StPifMax31855;
typedef struct StPifMax31855 PifMax31855;

typedef void (*PifEvtMax31855Measure)(PifMax31855* p_owner, double temperature, PifIssuerP p_issuer);		// distance unit : degree


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifMax31855CsFlag
{
    M3_CSF_OFF			= 0,

    M3_CSF_STATE_IDX	= 0,

	M3_CSF_STATE_BIT	= 1,
    M3_CSF_ALL_BIT		= 1,

    M3_CSF_COUNT		= 1
} PifMax31855CsFlag;

typedef struct StPifMax31855ColSig
{
	PifMax31855* p_owner;
    uint8_t flag;
    void* p_device[M3_CSF_COUNT];
} PifMax31855ColSig;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @class StPifMax31855
 * @brief Defines the st pif max31855 data structure.
 */
typedef struct StPifMax31855
{
	// The parent variable must be at the beginning of this structure.
	PifSensor parent;

	// Public Member Variable
    PifNoiseFilter* p_filter;

	// Read-only Member Variable
	PifSpiDevice* _p_spi;
    PifTask* _p_task;
	double _internal;

	// Private Member Variable
	double __low_threshold;
	double __high_threshold;
    PifIssuerP __p_issuer;

#ifdef PIF_COLLECT_SIGNAL
    PifMax31855ColSig* __p_colsig;
#endif

	// Private Event Function
	PifEvtMax31855Measure __evt_measure;
} PifMax31855;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMax31855_Init
 * @brief Initializes max31855 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @param p_port Pointer to the communication port interface.
 * @param p_client Pointer to optional client context data.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port, void *p_client);

/**
 * @fn pifMax31855_Clear
 * @brief Releases resources used by max31855 clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifMax31855_Clear(PifMax31855* p_owner);

/**
 * @fn pifMax31855_Measure
 * @brief Performs the max31855 measure operation.
 * @param p_owner Pointer to the owner instance.
 * @param p_temperature Pointer to temperature.
 * @param p_internal Pointer to internal.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal);

/**
 * @fn pifMax31855_StartMeasurement
 * @brief Performs the max31855 start measurement operation.
 * @param p_owner Pointer to the owner instance.
 * @param period1ms Task period in units of 1 ms.
 * @param evt_measure Parameter evt_measure used by this operation.
 * @param p_issuer Pointer to issuer.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period1ms, PifEvtMax31855Measure evt_measure, PifIssuerP p_issuer);

/**
 * @fn pifMax31855_StopMeasurement
 * @brief Performs the max31855 stop measurement operation.
 * @param p_owner Pointer to the owner instance.
 */
void pifMax31855_StopMeasurement(PifMax31855* p_owner);

/**
 * @fn pifMax31855_SetThreshold
 * @brief Sets configuration values required by max31855 set threshold.
 * @param p_owner Pointer to the owner instance.
 * @param low_threshold Lower threshold level.
 * @param high_threshold Upper threshold level.
 */
void pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifMax31855_SetCsFlag
 * @brief Sets configuration values required by max31855 set cs flag.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit flag value to set or clear.
 */
void pifMax31855_SetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

/**
 * @fn pifMax31855_ResetCsFlag
 * @brief Sets configuration values required by max31855 reset cs flag.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit flag value to set or clear.
 */
void pifMax31855_ResetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

/**
 * @fn pifMax31855ColSig_SetFlag
 * @brief Sets configuration values required by max31855 col sig set flag.
 * @param flag Bit flag value to set or clear.
 */
void pifMax31855ColSig_SetFlag(PifMax31855CsFlag flag);

/**
 * @fn pifMax31855ColSig_ResetFlag
 * @brief Sets configuration values required by max31855 col sig reset flag.
 * @param flag Bit flag value to set or clear.
 */
void pifMax31855ColSig_ResetFlag(PifMax31855CsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_MAX31855_H
