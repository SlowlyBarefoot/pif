#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "pifPulse.h"
#include "pifTask.h"


#define PIF_SEQUENCE_ITEM_NO_IDLE	0xFF

#define PIF_SEQUENCE_STEP_INIT		0
#define PIF_SEQUENCE_STEP_DELAY		0xFF


typedef enum _PIF_enSequenceResult
{
    SR_enContinue	= 1,
    SR_enNext		= 2,
    SR_enError		= 3
} PIF_enSequenceResult;

typedef struct _PIF_stSequence
{
	uint8_t ucId;
	uint8_t ucItemNo;
	uint8_t ucStep;
	uint8_t ucNextItemNo;
	uint16_t usDelay;
	void *pvParam;
} PIF_stSequence;

typedef PIF_enSequenceResult (*PIF_fnSequence)(PIF_stSequence *pstCommon);
typedef void (*PIF_evtSequenceFinish)(uint8_t ucId);

typedef struct _PIF_stSequenceItem 
{
	PIF_fnSequence fnProcess;
	uint16_t usDelay;
	uint8_t ucItemNoNext;
	uint8_t ucItemNoError;
} PIF_stSequenceItem;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifSequence_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifSequence_Exit();

PIF_stSequence *pifSequence_Add(uint8_t ucId, const PIF_stSequenceItem *pstItemList, void *pvParam);

void pifSequence_Start(PIF_stSequence *pstOwner);

// Attach Event Function
void pifSequence_AttachEvtFinish(PIF_stSequence *pstOwner, PIF_evtSequenceFinish evtFinish);

// Task Function
void pifSequence_taskAll(PIF_stTask *pstTask);
void pifSequence_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
