#ifndef PIF_SENSOR_H
#define PIF_SENSOR_H


#include "core/pif.h"


typedef void* PifSensorValueP;

struct StPifSensor;
typedef struct StPifSensor PifSensor;

typedef uint16_t (*PifActSensorAcquire)(PifSensor* p_owner);
typedef void (*PifEvtSensorChange)(PifSensor* p_owner, SWITCH state, PifSensorValueP p_value, PifIssuerP p_issuer);


/**
 * @class StPifSensor
 * @brief
 */
struct StPifSensor
{
	// Public Member Variable
    PifIssuerP p_issuer;

	// Public Event Function
   	PifEvtSensorChange evt_change;

	// Read-only Member Variable
	PifId _id;
    SWITCH _init_state;
    SWITCH _curr_state;						// Default: _init_state

	// Private Action Function
	PifActSensorAcquire __act_acquire;
};


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
