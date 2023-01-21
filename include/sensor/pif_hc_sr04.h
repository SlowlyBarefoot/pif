#ifndef PIF_HC_SR04_H
#define PIF_HC_SR04_H


#include "core/pif_task.h"
#include "sensor/pif_sensor_event.h"


typedef enum EnPifHcSr04State
{
	HSS_READY		= 0,
	HSS_TRIGGER		= 1,
	HSS_HIGH		= 2,
	HSS_LOW			= 3
} PifHcSr04State;

typedef void (*PifActHcSr04Trigger)(SWITCH state);


/**
 * @class StPifHcSr04
 * @brief
 */
typedef struct StPifHcSr04
{
	// Public Member Variable
	PifActHcSr04Trigger act_trigger;
	PifEvtSonarRead evt_read;

	// Read-only Member Variable
	PifId _id;
    PifTask* _p_task;
    float _transform_const;

	// Private Member Variable
    PifHcSr04State __state;
    uint32_t __tigger_time_us;
	int32_t __distance;
} PifHcSr04;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifHcSr04_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifHcSr04_Init(PifHcSr04* p_owner, PifId id);

/**
 * @fn pifHcSr04_Clear
 * @brief
 * @param p_owner
 */
void pifHcSr04_Clear(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_Trigger
 * @brief
 * @param p_owner
 */
void pifHcSr04_Trigger(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_StartTrigger
 * @brief
 * @param p_owner
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @return
 */
BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period);

/**
 * @fn pifHcSr04_StopTrigger
 * @brief
 * @param p_owner
 */
void pifHcSr04_StopTrigger(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_SetTemperature
 * @brief
 * @param p_owner
 * @param temperature
 */
void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature);

/**
 * @fn pifHcSr04_sigReceiveEcho
 * @brief
 * @param p_owner
 * @param state
 */
void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state);

#ifdef __cplusplus
}
#endif


#endif  // PIF_HC_SR04_H
