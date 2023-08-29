#ifndef PIF_MAX31855_H
#define PIF_MAX31855_H


#include "communication/pif_spi.h"
#include "filter/pif_noise_filter.h"
#include "sensor/pif_sensor.h"


struct StPifMax31855;
typedef struct StPifMax31855 PifMax31855;

typedef void (*PifEvtMax31855Measure)(PifMax31855* p_owner, double temperature, PifIssuerP p_issuer);		// distance unit : degree


#ifdef __PIF_COLLECT_SIGNAL__

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

#endif	// __PIF_COLLECT_SIGNAL__

/**
 * @class StPifMax31855
 * @brief
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

#ifdef __PIF_COLLECT_SIGNAL__
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_port
 * @return
 */
BOOL pifMax31855_Init(PifMax31855* p_owner, PifId id, PifSpiPort* p_port);

/**
 * @fn pifMax31855_Clear
 * @brief
 * @param p_owner
 */
void pifMax31855_Clear(PifMax31855* p_owner);

/**
 * @fn pifMax31855_Measure
 * @brief
 * @param p_owner
 * @param p_temperature
 * @param p_internal
 * @return
 */
BOOL pifMax31855_Measure(PifMax31855* p_owner, double* p_temperature, double* p_internal);

/**
 * @fn pifMax31855_StartMeasurement
 * @brief
 * @param p_owner
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param evt_measure
 * @return
 */
BOOL pifMax31855_StartMeasurement(PifMax31855* p_owner, uint16_t period, PifEvtMax31855Measure evt_measure);

/**
 * @fn pifMax31855_StopMeasurement
 * @brief
 * @param p_owner
 */
void pifMax31855_StopMeasurement(PifMax31855* p_owner);

/**
 * @fn pifMax31855_SetThreshold
 * @brief
 * @param p_owner
 * @param low_threshold
 * @param high_threshold
 */
void pifMax31855_SetThreshold(PifMax31855* p_owner, double low_threshold, double high_threshold);

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifMax31855_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifMax31855_SetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

/**
 * @fn pifMax31855_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifMax31855_ResetCsFlag(PifMax31855* p_owner, PifMax31855CsFlag flag);

/**
 * @fn pifMax31855ColSig_SetFlag
 * @brief
 * @param flag
 */
void pifMax31855ColSig_SetFlag(PifMax31855CsFlag flag);

/**
 * @fn pifMax31855ColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifMax31855ColSig_ResetFlag(PifMax31855CsFlag flag);

#endif	// __PIF_COLLECT_SIGNAL__

#ifdef __cplusplus
}
#endif


#endif  // PIF_MAX31855_H
