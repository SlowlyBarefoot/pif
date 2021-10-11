#ifndef PIF_SENSOR_H
#define PIF_SENSOR_H


#include "pifPulse.h"
#include "pifTask.h"


typedef uint16_t (*PifActSensorAcquire)(PifId id);
typedef void (*PifEvtSensorChange)(PifId id, uint16_t level, void* p_issuer);

/**
 * @class StPifSensor
 * @brief
 */
typedef struct StPifSensor
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;
    SWITCH _init_state;
    SWITCH _curr_state;						// Default: enInitState

	// Private Member Variable
    void* __p_change_issuer;

	// Private Action Function
	PifActSensorAcquire __act_acquire;			// Default: NULL

	// Private Event Function
   	PifEvtSensorChange __evt_change;			// Default: NULL
} PifSensor;


#ifdef __cplusplus
extern "C" {
#endif

void pifSensor_AttachAction(PifSensor* p_owner, PifActSensorAcquire act_acquire);

void pifSensor_AttachEvtChange(PifSensor* p_owner, PifEvtSensorChange evt_change, void* p_issuer);
void pifSensor_DetachEvtChange(PifSensor* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
