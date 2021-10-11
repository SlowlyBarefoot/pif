#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "pifPulse.h"
#include "pifTask.h"


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

PifSequence* pifSequence_Create(PifId id, PifPulse* p_timer, const PifSequencePhase* p_phase_list, void* p_param);
void pifSequence_Destroy(PifSequence** pp_owner);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSequence_SetCsFlagAll(PifSequenceCsFlag flag);
void pifSequence_ResetCsFlagAll(PifSequenceCsFlag flag);

void pifSequence_SetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag);
void pifSequence_ResetCsFlagEach(PifSequence* p_owner, PifSequenceCsFlag flag);

#endif

void pifSequence_Start(PifSequence* p_owner);

BOOL pifSequence_SetTimeout(PifSequence* p_owner, uint16_t timeout);

// Task Function
PifTask* pifSequence_AttachTask(PifSequence* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
