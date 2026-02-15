#ifndef PIF_HC_SR04_H
#define PIF_HC_SR04_H


#include "core/pif_task_manager.h"
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
 * @brief Defines the st pif hc sr04 data structure.
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
 * @brief Initializes hc sr04 init and prepares it for use.
 * @param p_owner Pointer to the owner instance.
 * @param id Unique identifier for the instance or task.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHcSr04_Init(PifHcSr04* p_owner, PifId id);

/**
 * @fn pifHcSr04_Clear
 * @brief Releases resources used by hc sr04 clear.
 * @param p_owner Pointer to the owner instance.
 */
void pifHcSr04_Clear(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_Trigger
 * @brief Performs the hc sr04 trigger operation.
 * @param p_owner Pointer to the owner instance.
 */
void pifHcSr04_Trigger(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_StartTrigger
 * @brief Performs the hc sr04 start trigger operation.
 * @param p_owner Pointer to the owner instance.
 * @param period Task period value; its unit depends on the selected mode.
 * @return TRUE on success, FALSE on failure.
 */
BOOL pifHcSr04_StartTrigger(PifHcSr04* p_owner, uint16_t period);

/**
 * @fn pifHcSr04_StopTrigger
 * @brief Performs the hc sr04 stop trigger operation.
 * @param p_owner Pointer to the owner instance.
 */
void pifHcSr04_StopTrigger(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_SetTemperature
 * @brief Sets configuration values required by hc sr04 set temperature.
 * @param p_owner Pointer to the owner instance.
 * @param temperature Temperature value in degrees Celsius.
 */
void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature);

/**
 * @fn pifHcSr04_sigReceiveEcho
 * @brief Performs the hc sr04 sig receive echo operation.
 * @param p_owner Pointer to the owner instance.
 * @param state Parameter state used by this operation.
 */
void pifHcSr04_sigReceiveEcho(PifHcSr04* p_owner, SWITCH state);

#ifdef __cplusplus
}
#endif


#endif  // PIF_HC_SR04_H
