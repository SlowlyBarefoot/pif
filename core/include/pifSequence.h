#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "pifPulse.h"
#include "pifTask.h"


#define PIF_SEQUENCE_PHASE_NO_IDLE	0xFF

#define PIF_SEQUENCE_STEP_INIT		0


typedef enum _PIF_enSequenceCsFlag
{
    SqCsF_enOff			= 0,

    SqCsF_enPhaseIdx	= 0,

	SqCsF_enPhaseBit	= 1,
	SqCsF_enAllBit		= 1,

    SqCsF_enCount		= 1
} PIF_enSequenceCsFlag;

typedef enum _PIF_enSequenceResult
{
    SR_enContinue	= 1,
    SR_enNext		= 2,
    SR_enFinish		= 3
} PIF_enSequenceResult;


struct _PIF_stSequence;
typedef struct _PIF_stSequence PIF_stSequence;

typedef PIF_enSequenceResult (*PIF_fnSequence)(PIF_stSequence *pstOwner);
typedef void (*PIF_evtSequenceError)(PIF_stSequence *pstOwner);


typedef struct _PIF_stSequencePhase
{
	PIF_fnSequence fnProcess;
	uint8_t ucPhaseNoNext;
} PIF_stSequencePhase;

#ifdef __PIF_COLLECT_SIGNAL__

typedef struct
{
	PIF_stSequence* p_owner;
	uint8_t flag;
    void* p_device[SqCsF_enCount];
} PIF_SequenceColSig;

#endif

struct _PIF_stSequence
{
	// Public Member Variable
	uint8_t ucStep;
	uint8_t ucPhaseNoNext;
	uint32_t unDelay1us;
	void *pvParam;

    // Public Event Function
	PIF_evtSequenceError evtError;

	// Read-only Member Variable
	PifId _usPifId;
	uint8_t _ucPhaseNo;

	// Private Member Variable
	PIF_stPulse* __pstTimer;
	const PIF_stSequencePhase *__pstPhaseList;
	PIF_stPulseItem *__pstTimerTimeout;
	uint32_t __unTargetDelay;
#ifdef __PIF_COLLECT_SIGNAL__
	PIF_SequenceColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

PIF_stSequence* pifSequence_Create(PifId usPifId, PIF_stPulse* pstTimer, const PIF_stSequencePhase* pstPhaseList, void* pvParam);
void pifSequence_Destroy(PIF_stSequence** pp_owner);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSequence_SetCsFlagAll(PIF_enSequenceCsFlag enFlag);
void pifSequence_ResetCsFlagAll(PIF_enSequenceCsFlag enFlag);

void pifSequence_SetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag);
void pifSequence_ResetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag);

#endif

void pifSequence_Start(PIF_stSequence *pstOwner);

BOOL pifSequence_SetTimeout(PIF_stSequence *pstOwner, uint16_t usTimeout);

// Task Function
PifTask *pifSequence_AttachTask(PIF_stSequence *pstOwner, PifTaskMode enMode, uint16_t usPeriod, BOOL bStart);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
