#ifndef PIF_SENSOR_H
#define PIF_SENSOR_H


#include "pifPulse.h"
#include "pifTask.h"


typedef uint16_t (*PIF_actSensorAcquire)(PIF_usId usPifId);
typedef void (*PIF_evtSensorChange)(PIF_usId usPifId, uint16_t usLevel, void *pvIssuer);

/**
 * @class _PIF_stSensor
 * @brief
 */
typedef struct _PIF_stSensor
{
	// Public Member Variable

	// Read-only Member Variable
    PIF_usId _usPifId;
    SWITCH _swInitState;
    SWITCH _swCurrState;						// Default: enInitState

	// Private Member Variable
    void *__pvChangeIssuer;

	// Private Action Function
	PIF_actSensorAcquire __actAcquire;			// Default: NULL

	// Private Event Function
   	PIF_evtSensorChange __evtChange;			// Default: NULL
} PIF_stSensor;


#ifdef __cplusplus
extern "C" {
#endif

void pifSensor_AttachAction(PIF_stSensor *pstOwner, PIF_actSensorAcquire actAcquire);

void pifSensor_AttachEvtChange(PIF_stSensor *pstOwner, PIF_evtSensorChange evtChange, void *pvIssuer);
void pifSensor_DetachEvtChange(PIF_stSensor *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
