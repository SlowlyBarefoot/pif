#ifndef PIF_BUZZER_H
#define PIF_BUZZER_H


#include "core/pif_task_manager.h"


#define PIF_BUZZER_STOP			0xF0
#define PIF_BUZZER_REPEAT(N)	(0xF0 + (N - 1))


typedef void (*PifActBuzzerAction)(BOOL action);

typedef void (*PifEvtBuzzerPeriod)(PifId id);
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
 * @brief Buzzer controller object that drives an ON/OFF sequence using a periodic task.
 */
typedef struct StPifBuzzer
{
	// Public Member Variable

	// Private Event Function
	PifEvtBuzzerPeriod evt_period;
	PifEvtBuzzerChange evt_change;
	PifEvtBuzzerFinish evt_finish;

	// Read-only Member Variable
    PifId _id;
    PifTask* _p_task;
    PifBuzzerState _state;

	// Private Member Variable
    const uint8_t* __p_sequence;
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
 * @brief Initializes a buzzer instance and creates its periodic state-machine task.
 * @param p_owner Pointer to the buzzer instance to initialize.
 * @param id Identifier to assign to the instance. Use PIF_ID_AUTO for automatic assignment.
 * @param period1ms Task period in milliseconds used to advance buzzer sequence timing.
 * @param act_action Hardware action callback used to set buzzer output ON or OFF.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifBuzzer_Init(PifBuzzer* p_owner, PifId id, uint16_t period1ms, PifActBuzzerAction act_action);

/**
 * @fn pifBuzzer_Clear
 * @brief Releases resources owned by the buzzer instance.
 * @param p_owner Pointer to the buzzer instance to clear.
 */
void pifBuzzer_Clear(PifBuzzer* p_owner);

/**
 * @fn pifBuzzer_Start
 * @brief Starts buzzer playback using the given encoded ON/OFF sequence.
 * @param p_owner Pointer to the buzzer instance.
 * @param p_sequence Pointer to sequence data encoded with durations and stop/repeat markers.
 * @return TRUE if playback is started, otherwise FALSE.
 */
BOOL pifBuzzer_Start(PifBuzzer* p_owner, const uint8_t* p_sequence);

/**
 * @fn pifBuzzer_Stop
 * @brief Stops buzzer playback immediately and transitions the state machine to stop.
 * @param p_owner Pointer to the buzzer instance.
 */
void pifBuzzer_Stop(PifBuzzer* p_owner);

/**
 * @fn pifBuzzer_State
 * @brief Checks whether the buzzer is currently active.
 * @param p_owner Pointer to the buzzer instance.
 * @return TRUE if buzzer output sequence is in START or ON state, otherwise FALSE.
 */
BOOL pifBuzzer_State(PifBuzzer* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BUZZER_H
