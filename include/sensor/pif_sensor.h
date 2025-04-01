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
	// Read-only Member Variable
	PifId _id;
    SWITCH _init_state;
    SWITCH _curr_state;						// Default: _init_state

	// Private Member Variable
    PifIssuerP __p_issuer;

	// Private Action Function
	PifActSensorAcquire __act_acquire;

	// Private Event Function
    PifEvtSensorChange __evt_change;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSensor_AttachEvtChange
 * @brief
 * @param p_owner
 * @param evt_change
 * @param p_issuer
 */
void pifSensor_AttachEvtChange(PifSensor *p_owner, PifEvtSensorChange evt_change, PifIssuerP p_issuer);

/**
 * @fn pifSensor_DetachEvtChange
 * @brief
 * @param p_owner
 */
void pifSensor_DetachEvtChange(PifSensor *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SENSOR_H
