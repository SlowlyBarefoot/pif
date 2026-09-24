#ifndef PIF_BUZZER_H
#define PIF_BUZZER_H


#include "core/pif_task_manager.h"


/*
 * Sequence format
 *
 * A sequence is an array of durations, counted in task periods (period1ms of
 * pifBuzzer_Init), that alternate ON and OFF: entries 0, 2, 4... are ON
 * durations and entries 1, 3, 5... are OFF durations. A duration of N holds
 * its level for exactly N periods. A duration of 0 is skipped, so the output
 * keeps its level; a sequence that starts with 0 therefore starts with a pause.
 * Durations must be below PIF_BUZZER_STOP.
 *
 * The sequence ends with PIF_BUZZER_STOP or PIF_BUZZER_REPEAT(N), which may
 * follow either an ON or an OFF duration. The output is turned off when the
 * sequence ends. PIF_BUZZER_REPEAT(N) plays the whole sequence N times in all
 * (1 to 16), so PIF_BUZZER_REPEAT(1) is the same as PIF_BUZZER_STOP.
 */
#define PIF_BUZZER_STOP			0xF0
#define PIF_BUZZER_REPEAT(N)	(0xF0 + ((N) - 1))


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
	BS_STOP			= 4		// No longer entered: a finished sequence goes straight to BS_IDLE.
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
    BOOL _output;				// Current output level, as last passed to the action.

	// Private Member Variable
    const uint8_t* __p_sequence;
    uint8_t __pos;
    uint8_t __repeat;
	uint16_t __count;

	// Private Action Function
    PifActBuzzerAction __act_action;
} PifBuzzer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifBuzzer_Init
 * @brief Initializes a buzzer instance and creates its periodic state-machine task.
 * @details act_action and evt_change are called only when the output level changes. While
 *          they handle the first edge of a sequence, _state is still BS_START.
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
 * @details Playback begins on the next task period. Starting while a sequence plays replaces
 *          it from its first entry; the output changes only if the new sequence needs it.
 * @param p_owner Pointer to the buzzer instance.
 * @param p_sequence Pointer to sequence data, in the format described at PIF_BUZZER_STOP.
 *                   It must stay valid while it plays.
 * @return TRUE if playback is started, otherwise FALSE (E_INVALID_PARAM for a NULL argument,
 *         E_INVALID_STATE if the buzzer has no task).
 */
BOOL pifBuzzer_Start(PifBuzzer* p_owner, const uint8_t* p_sequence);

/**
 * @fn pifBuzzer_Stop
 * @brief Stops buzzer playback immediately, drives the output OFF and returns to idle.
 * @details evt_finish is not called.
 * @param p_owner Pointer to the buzzer instance.
 */
void pifBuzzer_Stop(PifBuzzer* p_owner);

/**
 * @fn pifBuzzer_State
 * @brief Checks whether the buzzer output is currently on.
 * @param p_owner Pointer to the buzzer instance.
 * @return TRUE if the output is ON, otherwise FALSE.
 */
BOOL pifBuzzer_State(PifBuzzer* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_BUZZER_H
