#ifndef PIF_SEQUENCE_H
#define PIF_SEQUENCE_H


#include "core/pif_task_manager.h"
#include "core/pif_timer_manager.h"


struct StPifSequence;
typedef struct StPifSequence PifSequence;

typedef void (*PifSequenceProcess)(PifSequence* p_owner);
typedef void (*PifEvtSequenceError)(PifSequence* p_owner);


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifSequenceCsFlag
{
    SQ_CSF_OFF			= 0,

    SQ_CSF_PHASE_IDX	= 0,

	SQ_CSF_PHASE_BIT	= 1,
	SQ_CSF_ALL_BIT		= 1,

    SQ_CSF_COUNT		= 1
} PifSequenceCsFlag;

typedef struct StPifSequenceColSig
{
	PifSequence* p_owner;
	uint8_t flag;
    void* p_device[SQ_CSF_COUNT];
} PifSequenceColSig;

#endif	// PIF_COLLECT_SIGNAL

struct StPifSequence
{
	// Public Member Variable
	void* p_param;

    // Public Event Function
	PifEvtSequenceError evt_error;

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifTask* __p_task;
	PifTimerManager* __p_timer_manager;
	PifTimer *__p_timer_timeout;
	PifSequenceProcess __process;
	PifSequenceProcess __next_process;
#ifdef PIF_COLLECT_SIGNAL
	PifSequenceColSig* __p_colsig;
#endif
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifSequence_Init
 * @brief Sequence를 추가한다.
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param p_param
 * @return Sequence 구조체 포인터를 반환한다.
 */
BOOL pifSequence_Init(PifSequence* p_owner, PifId id, PifTimerManager* p_timer_manager, void* p_param);

/**
 * @fn pifSequence_Clear
 * @brief Sequence용 메모리를 반환한다.
 * @param p_owner
 */
void pifSequence_Clear(PifSequence* p_owner);

/**
 * @fn pifSequence_IsRunning
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifSequence_IsRunning(PifSequence *p_owner);

/**
 * @fn pifSequence_Start
 * @brief
 * @param p_owner
 * @param process
 * @return
 */
BOOL pifSequence_Start(PifSequence *p_owner, PifSequenceProcess process);

/**
 * @fn pifSequence_NextDelay
 * @brief
 * @param p_owner
 * @param process
 * @param delay1ms
 * @return
 */
BOOL pifSequence_NextDelay(PifSequence *p_owner, PifSequenceProcess process, uint16_t delay1ms);

/**
 * @fn pifSequence_NextEvent
 * @brief
 * @param p_owner
 * @param process
 * @param timeout1ms
 * @return
 */
BOOL pifSequence_NextEvent(PifSequence *p_owner, PifSequenceProcess process, uint16_t timeout1ms);

/**
 * @fn pifSequence_TriggerEvent
 * @brief
 * @param p_owner
 */
void pifSequence_TriggerEvent(PifSequence *p_owner);


#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifSequence_SetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_SetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag);

/**
 * @fn pifSequence_ResetCsFlag
 * @brief
 * @param p_owner
 * @param flag
 */
void pifSequence_ResetCsFlag(PifSequence* p_owner, PifSequenceCsFlag flag);

/**
 * @fn pifSequenceColSig_SetFlag
 * @brief
 * @param flag
 */
void pifSequenceColSig_SetFlag(PifSequenceCsFlag flag);

/**
 * @fn pifSequenceColSig_ResetFlag
 * @brief
 * @param flag
 */
void pifSequenceColSig_ResetFlag(PifSequenceCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_SEQUENCE_H
