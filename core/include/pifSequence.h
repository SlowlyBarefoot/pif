#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "pifPulse.h"
#include "pifTask.h"


#define PIF_SEQUENCE_PHASE_NO_IDLE	0xFF

#define PIF_SEQUENCE_STEP_INIT		0
#define PIF_SEQUENCE_STEP_DELAY		0xFF


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

struct _PIF_stSequence
{
	// Public Member Variable
	uint8_t ucStep;
	uint8_t ucPhaseNoNext;
	uint16_t usDelay;
	void *pvParam;

    // Public Event Function
	PIF_evtSequenceError evtError;

	// Read-only Member Variable
	PIF_usId _usPifId;
	uint8_t _ucPhaseNo;

	// Private Member Variable
	uint8_t __ucIndex;
	const PIF_stSequencePhase *__pstPhaseList;
	PIF_stPulseItem *__pstTimerTimeout;
	PIF_stPulseItem *__pstTimerDelay;
#ifdef __PIF_COLLECT_SIGNAL__
	uint8_t __ucCsFlag;
    int8_t __cCsIndex[SqCsF_enCount];
#endif

	PIF_enTaskLoop __enTaskLoop;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSequence_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifSequence_Exit();

PIF_stSequence *pifSequence_Add(PIF_usId usPifId, const PIF_stSequencePhase *pstPhaseList, void *pvParam);

#ifdef __PIF_COLLECT_SIGNAL__

void pifSequence_SetCsFlagAll(PIF_enSequenceCsFlag enFlag);
void pifSequence_ResetCsFlagAll(PIF_enSequenceCsFlag enFlag);

void pifSequence_SetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag);
void pifSequence_ResetCsFlagEach(PIF_stSequence *pstOwner, PIF_enSequenceCsFlag enFlag);

#endif

void pifSequence_Start(PIF_stSequence *pstOwner);

BOOL pifSequence_SetTimeout(PIF_stSequence *pstOwner, uint16_t usTimeout);

// Task Function
void pifSequence_taskAll(PIF_stTask *pstTask);
void pifSequence_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
