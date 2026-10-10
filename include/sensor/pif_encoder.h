// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_ENCODER_H
#define PIF_ENCODER_H

// Quadrature encoder input.
//
// pifEncoder_sigState takes the levels of phase A and B on every edge of either phase, from the edge
// interrupts or from a poll that is fast enough to see every edge. It decodes each quarter step (X4) with
// its direction and counts a change of both phases at once, which means a missed edge, as an error.
//
//   - The position counts in X1, X2 or X4 and can be set at any time.
//   - The speed comes from the times of the last steps in the same direction, so it stays accurate at low
//     speed. Once the next step is later than the measured step period, the speed falls with the time
//     since the last step, and after the timeout it reads as stopped.
//
// Phase A leading phase B counts up. pifEncoder_SetReverse turns the direction around.


#include "core/pif.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


// Quarter steps whose times are kept for the speed. A power of two, at least 2. The speed averages over a
// whole number of encoder cycles (4 quarter steps) once 5 steps are kept.
#ifndef PIF_ENCODER_DATA_SIZE
	#define PIF_ENCODER_DATA_SIZE		8
#endif
#define PIF_ENCODER_DATA_MASK			(PIF_ENCODER_DATA_SIZE - 1)

#if PIF_ENCODER_DATA_SIZE < 2 || (PIF_ENCODER_DATA_SIZE & PIF_ENCODER_DATA_MASK)
	#error "PIF_ENCODER_DATA_SIZE must be a power of two and at least 2"
#endif

// Bits of the state given to pifEncoder_sigState.
#define ENCODER_PHASE_A					2
#define ENCODER_PHASE_B					1


/**
 * @enum EnPifEncoderResolution
 * @brief Counts per encoder cycle that the position and the speed use.
 */
typedef enum EnPifEncoderResolution
{
	ENCODER_RES_X1		= 0,	// 1 count per cycle
	ENCODER_RES_X2		= 1,	// 2 counts per cycle
	ENCODER_RES_X4		= 2		// 4 counts per cycle, every edge
} PifEncoderResolution;

/**
 * @enum EnPifEncoderResult
 * @brief Tells why pifEncoder_ReadSpeed has or has not a value.
 */
typedef enum EnPifEncoderResult
{
	ENCODER_R_OK		= 0,	// The value is valid
	ENCODER_R_NO_DATA	= 1,	// Not enough steps in one direction came yet
	ENCODER_R_TIMEOUT	= 2		// No step came within the timeout
} PifEncoderResult;

#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifEncoderCsFlag
{
	ENCODER_CSF_OFF				= 0,

	ENCODER_CSF_STATE_IDX		= 0,	// Phase A and B
	ENCODER_CSF_POSITION_IDX	= 1,	// Position

	ENCODER_CSF_STATE_BIT		= 1,
	ENCODER_CSF_POSITION_BIT	= 2,
	ENCODER_CSF_ALL_BIT			= 3,

	ENCODER_CSF_COUNT			= 2
} PifEncoderCsFlag;

#endif	// PIF_COLLECT_SIGNAL


/**
 * @brief Called from pifEncoder_sigState whenever the position changes.
 * @param direction 1 or -1.
 * @param p_issuer User context pointer given to pifEncoder_AttachEvtStep.
 */
typedef void (*PifEvtEncoderStep)(int8_t direction, PifIssuerP p_issuer);


struct StPifEncoder;
typedef struct StPifEncoder PifEncoder;

/**
 * @struct StPifEncoder
 * @brief Represents the quadrature encoder data structure used by this module.
 */
struct StPifEncoder
{
	// Read-only Member Variable
	PifId _id;
	PifEncoderResolution _resolution;

	// Private Member Variable
	// Written by pifEncoder_sigState, usually from an interrupt. __seq changes after every call so that
	// a reader can tell that a step came while it copied them.
	volatile int32_t __raw;					// Quarter steps
	volatile uint32_t __time[PIF_ENCODER_DATA_SIZE];	// Times of the last steps in __direction
	volatile uint32_t __error_count;
	volatile uint8_t __ptr;					// Next slot of __time
	volatile uint8_t __count;				// Times kept, up to PIF_ENCODER_DATA_SIZE
	volatile uint8_t __seq;
	volatile uint8_t __state;
	volatile BOOL __has_state;
	volatile int8_t __direction;			// 1, -1 or 0 before the first step
	BOOL __reverse;
	int32_t __offset;						// Added to the counts of __raw to give the position
	uint32_t __timeout;
	PifIssuerP __p_issuer;

#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[ENCODER_CSF_COUNT];
#endif

	// Private Event Function
	PifEvtEncoderStep __evt_step;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifEncoder_Init
 * @brief Initializes the encoder instance with the position at 0.
 * @param p_owner Pointer to the target object instance.
 * @param id Object identifier, or PIF_ID_AUTO.
 * @param resolution Counts per encoder cycle.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifEncoder_Init(PifEncoder* p_owner, PifId id, PifEncoderResolution resolution);

/**
 * @fn pifEncoder_Clear
 * @brief Releases the resources of the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifEncoder_Clear(PifEncoder* p_owner);

/**
 * @fn pifEncoder_SetReverse
 * @brief Makes phase B leading phase A count up instead.
 * @param p_owner Pointer to the target object instance.
 * @param reverse TRUE to reverse the direction.
 */
void pifEncoder_SetReverse(PifEncoder* p_owner, BOOL reverse);

/**
 * @fn pifEncoder_SetTimeout
 * @brief Makes the speed read as stopped once no step has come for timeout_us. The time is taken from
 *        pif_act_timer1us, which must run on the same clock as the time_us given to pifEncoder_sigState.
 * @param p_owner Pointer to the target object instance.
 * @param timeout_us Timeout in microseconds, or 0 to disable it.
 */
void pifEncoder_SetTimeout(PifEncoder* p_owner, uint32_t timeout_us);

/**
 * @fn pifEncoder_SetPosition
 * @brief Sets the current position. Call it from a context that the edge interrupt can preempt.
 * @param p_owner Pointer to the target object instance.
 * @param position New position in counts of the resolution.
 */
void pifEncoder_SetPosition(PifEncoder* p_owner, int32_t position);

/**
 * @fn pifEncoder_GetPosition
 * @brief Returns the position in counts of the resolution.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @return Position.
 */
int32_t pifEncoder_GetPosition(PifEncoder* p_owner);

/**
 * @fn pifEncoder_GetDirection
 * @brief Returns the direction of the last step.
 * @param p_owner Pointer to the target object instance.
 * @return 1, -1, or 0 before the first step.
 */
int8_t pifEncoder_GetDirection(PifEncoder* p_owner);

/**
 * @fn pifEncoder_GetErrorCount
 * @brief Returns how many times both phases changed at once, each of which lost a step.
 * @param p_owner Pointer to the target object instance.
 * @return Error count.
 */
uint32_t pifEncoder_GetErrorCount(PifEncoder* p_owner);

/**
 * @fn pifEncoder_ReadSpeed
 * @brief Reads the speed and tells why there is none.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @param p_speed Receives the speed in counts of the resolution per second, negative when counting down,
 *        or 0 if there is none. May be NULL.
 * @return ENCODER_R_OK, or the reason that there is no speed.
 */
PifEncoderResult pifEncoder_ReadSpeed(PifEncoder* p_owner, float* p_speed);

/**
 * @fn pifEncoder_GetSpeed
 * @brief Returns the speed as pifEncoder_ReadSpeed does, with 0 when there is none.
 * @param p_owner Pointer to the target object instance.
 * @return Speed in counts of the resolution per second, negative when counting down.
 */
float pifEncoder_GetSpeed(PifEncoder* p_owner);

/**
 * @fn pifEncoder_sigState
 * @brief Gives the levels of both phases after an edge of either.
 * @param p_owner Pointer to the target object instance.
 * @param state ENCODER_PHASE_A and ENCODER_PHASE_B bits of the phases that are high.
 * @param time_us Time of the edge in microseconds.
 * @return TRUE if a step was counted.
 */
BOOL pifEncoder_sigState(PifEncoder* p_owner, uint8_t state, uint32_t time_us);

/**
 * @fn pifEncoder_AttachEvtStep
 * @brief Attaches a callback that pifEncoder_sigState calls whenever the position changes.
 * @param p_owner Pointer to the target object instance.
 * @param evt_step Callback, or NULL to detach it.
 * @param p_issuer User context pointer passed to the callback.
 */
void pifEncoder_AttachEvtStep(PifEncoder* p_owner, PifEvtEncoderStep evt_step, PifIssuerP p_issuer);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifEncoder_SetCsFlag
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifEncoder_SetCsFlag(PifEncoder* p_owner, PifEncoderCsFlag flag);

/**
 * @fn pifEncoder_ResetCsFlag
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifEncoder_ResetCsFlag(PifEncoder* p_owner, PifEncoderCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_ENCODER_H
