#ifndef PIF_HC_SR04_H
#define PIF_HC_SR04_H


#include "pif_task.h"


typedef void (*PifActHcSr04Trigger)(SWITCH state);

typedef void (*PifEvtHcSr04Distance)(int32_t distance);		// distance unit : cm


/**
 * @class StPifHcSr04
 * @brief
 */
typedef struct StPifHcSr04
{
	// Public Member Variable
	PifActHcSr04Trigger act_trigger;
	PifEvtHcSr04Distance evt_distance;

	// Read-only Member Variable
	PifId _id;
    PifTask* _p_task;

	// Private Member Variable
    uint32_t __tigger_time_us;
    float __transform_const;
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
 * @fn pifHcSr04_Trigger
 * @brief
 * @param p_owner
 */
void pifHcSr04_Trigger(PifHcSr04* p_owner);

/**
 * @fn pifHcSr04_SetTemperature
 * @brief
 * @param p_owner
 * @param temperature
 */
void pifHcSr04_SetTemperature(PifHcSr04* p_owner, float temperature);

/**
 * @fn pifHcSr04_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifHcSr04_AttachTask(PifHcSr04* p_owner, uint16_t period, BOOL start);

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
