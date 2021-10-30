#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "pif_pulse.h"
#include "pif_task.h"


#define PIF_SEQUENCE_PHASE_NO_IDLE	0xFF

#define PIF_SEQUENCE_STEP_INIT		0


typedef enum EnPifSequenceCsFlag
{
    SQ_CSF_OFF			= 0,

    SQ_CSF_PHASE_IDX	= 0,

	SQ_CSF_PHASE_BIT	= 1,
	SQ_CSF_ALL_BIT		= 1,

    SQ_CSF_COUNT		= 1
} PifSequenceCsFlag;

typedef enum EnPifSequenceResult
{
    SR_CONTINUE			= 1,
    SR_NEXT				= 2,
    SR_FINISH			= 3
} PifSequenceResult;


struct StPifSequence;
typedef struct StPifSequence PifSequence;

typedef PifSequenceResult (*PifSequenceProcess)(PifSequence* p_owner);
typedef void (*PifEvtSequenceError)(PifSequence* p_owner);


typedef struct StPifSequencePhase
{
	PifSequenceProcess process;
	uint8_t phase_no_next;
} PifSequencePhase;

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct StPifSequenceColSig
{
	PifSequence* p_owner;
	uint8_t flag;
    void* p_device[SQ_CSF_COUNT];
} PIF_SequenceColSig;

#endif

struct StPifSequence
{
	// Public Member Variable
	uint8_t step;
	uint8_t phase_no_next;
	uint32_t delay1us;
	void* p_param;

    // Public Event Function
	PifEvtSequenceError evt_error;

	// Read-only Member Variable
	PifId _id;
	uint8_t _phase_no;

	// Private Member Variable
	PifTask* __p_task;
	PifPulse* __p_timer;
	const PifSequencePhase* __p_phase_list;
	PifPulseItem* __p_timer_timeout;
	uint32_t __target_delay;
#ifdef __PIF_COLLECT_SIGNAL__
	PIF_SequenceColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSequence_Create
 * @brief Sequence를 추가한다.
 * @param id
 * @param p_timer
 * @param control_period1ms
 * @param p_phase_list
 * @param p_param
 * @return Sequence 구조체 포인터를 반환한다.
 */
PifSequence* pifSequence_Create(PifId id, PifPulse* p_timer, uint16_t control_period1ms,
		const PifSequencePhase* p_phase_list, void* p_param);

/**
 * @fn pifSequence_Destroy
 * @brief Sequence용 메모리를 반환한다.
 * @param pp_owner
 */
void pifSequence_Destroy(PifSequence** pp_owner);

/**
 * @fn pifSequence_Init
 * @brief Sequence를 추가한다.
 * @param p_owner
 * @param id
 * @param p_timer
 * @param control_period1ms
 * @param p_phase_list
 * @param p_param
 * @return Sequence 구조체 포인터를 반환한다.
 */
BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifPulse* p_timer, uint16_t control_period1ms,
		const PifSequencePhase* p_phase_list, void* p_param);

/**
 * @fn pifSequence_Clear
 * @brief Sequence용 메모리를 반환한다.
 * @param p_owner
 */
void pifSequence_Clear(PifSequence* p_owner);

#ifdef __PIF_COLLECT_SIGNAL__

/**
 * @fn pifSequence_SetCsFlagAll
 * @brief
 * @param flag
 */
void pifSequence_SetCsFlagAll(PifSequenceCsFlag flag);

/**
 * @fn pifSequence_ResetCsFlagAll
 * @brief
 * @param flag
 */
void pifSequence_ResetCsFlagAll(PifSequenceCsFlag flag);

/**
 * @fn pifSequence_SetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_SetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag);

/**
 * @fn pifSequence_ResetCsFlagEach
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_ResetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag);

#endif

/**
 * @fn pifSequence_Start
 * @brief 
 * @param p_owner
 */
void pifSequence_Start(PifSequence* p_owner);

/**
 * @fn pifSequence_SetTimeout
 * @brief
 * @param p_owner
 * @param timeout
 * @return
 */
BOOL pifSequence_SetTimeout(PifSequence* p_owner, uint16_t timeout);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
