#ifndef PIF_BUZZER_H
#define PIF_BUZZER_H


#include "core/pif_task.h"


#define PIF_BUZZER_STOP			0xF0
#define PIF_BUZZER_REPEAT(N)	(0xF0 + (N - 1))


typedef void (*PifActBuzzerAction)(BOOL action);
typedef void (*PifEvtBuzzerChange)(PifId id, BOOL state);
typedef void (*PifEvtBuzzerFinish)(PifId id);

typedef enum EnPifBuzzerState
{
	BS_IDLE			= 0,
	BS_START		= 1,
	BS_ON			= 2,
	BS_OFF			= 3,
	BS_STOP			= 4
} PifBuzzerState;

/**
 * @class StPifBuzzer
 * @brief
 */
typedef struct StPifBuzzer
{
	// Public Member Variable

	// Private Event Function
	PifEvtBuzzerChange evt_change;
	PifEvtBuzzerFinish evt_finish;

	// Read-only Member Variable
    PifId _id;
    PifTask* _p_task;
    PifBuzzerState _state;

	// Private Member Variable
    const uint8_t* __p_sound_10ms;
    uint8_t __pos;
    uint8_t __repeat;

	// Private Action Function
    PifActBuzzerAction __act_action;
} PifBuzzer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBuzzer_Init
 * @brief
 * @param p_owner
 * @param id
 * @param act_action
 * @return
 */
BOOL pifBuzzer_Init(PifBuzzer* p_owner, PifId id, PifActBuzzerAction act_action);

/**
 * @fn pifBuzzer_Clear
 * @brief
 * @param p_owner
 */
void pifBuzzer_Clear(PifBuzzer* p_owner);

/**
 * @fn pifBuzzer_Start
 * @brief
 * @param p_owner
 * @param p_sound_10ms
 * @return
 */
BOOL pifBuzzer_Start(PifBuzzer* p_owner, const uint8_t* p_sound_10ms);

/**
 * @fn pifBuzzer_Stop
 * @brief
 * @param p_owner
 */
void pifBuzzer_Stop(PifBuzzer* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BUZZER_H
