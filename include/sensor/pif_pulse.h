// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "core/pif_task.h"
#ifdef PIF_COLLECT_SIGNAL
	#include "core/pif_collect_signal.h"
#endif


// Number of falling edges kept for pifPulse_GetAveragePeriod. A power of two, at least 2.
#ifndef PIF_PULSE_DATA_SIZE
	#define PIF_PULSE_DATA_SIZE		4
#endif
#define PIF_PULSE_DATA_MASK			(PIF_PULSE_DATA_SIZE - 1)

#if PIF_PULSE_DATA_SIZE < 2 || (PIF_PULSE_DATA_SIZE & PIF_PULSE_DATA_MASK)
	#error "PIF_PULSE_DATA_SIZE must be a power of two and at least 2"
#endif

#define PULSE_PMM_PERIOD			1
#define PULSE_PMM_COUNT				2
#define PULSE_PMM_LOW_WIDTH			4
#define PULSE_PMM_HIGH_WIDTH		8


/**
 * @enum EnPifPulseValue
 * @brief Selects the measurement that pifPulse_Read returns.
 */
typedef enum EnPifPulseValue
{
	PULSE_V_PERIOD			= 0,	// Time between the last two falling edges
	PULSE_V_AVERAGE_PERIOD	= 1,	// Period averaged over the falling edges kept
	PULSE_V_LOW_WIDTH		= 2,	// Low time before the last pulse
	PULSE_V_HIGH_WIDTH		= 3		// High time of the last pulse
} PifPulseValue;

/**
 * @enum EnPifPulseResult
 * @brief Tells why pifPulse_Read has or has not a value.
 */
typedef enum EnPifPulseResult
{
	PULSE_R_OK				= 0,	// The value is valid
	PULSE_R_DISABLED		= 1,	// The PULSE_PMM_XXX of the value is not set
	PULSE_R_NO_DATA			= 2,	// Not enough edges came yet
	PULSE_R_TIMEOUT			= 3,	// No falling edge came within the timeout
	PULSE_R_OUT_OF_RANGE	= 4		// The value is outside the valid range
} PifPulseResult;


typedef void (*PifEvtPulseEdge)(PifPulseState state, PifIssuerP p_issuer);


struct StPifPulse;
typedef struct StPifPulse PifPulse;


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifPulseCsFlag
{
    PULSE_CSF_OFF		= 0,

	PULSE_CSF_STATE_IDX	= 0,

	PULSE_CSF_STATE_BIT	= 1,
	PULSE_CSF_ALL_BIT	= 1,

	PULSE_CSF_COUNT		= 1
} PifPulseCsFlag;

#endif	// PIF_COLLECT_SIGNAL

/**
 * @struct StPifPulseData
 * @brief Represents the pulse data data structure used by this module.
 */
typedef struct StPifPulseData
{
	uint32_t rising;
	uint32_t falling;
} PifPulseData;

/**
 * @struct StPifPulse
 * @brief Represents the pulse data structure used by this module.
 */
struct StPifPulse
{
	// Public Member Variable
	volatile uint32_t falling_count;

	// Read-only Member Variable
	PifId _id;
	uint8_t	_measure_mode;		// PULSE_PMM_XXX

	// Private Member Variable
	// Written by pifPulse_sigEdge, usually from an interrupt. __seq changes after every falling edge so that
	// a reader can tell that an edge came while it copied them.
	volatile PifPulseData __data[PIF_PULSE_DATA_SIZE];
	volatile uint8_t __ptr, __last_ptr;
	volatile uint8_t __count;			// Falling edges in __data, up to PIF_PULSE_DATA_SIZE
	volatile uint8_t __seq;
	volatile BOOL __rising_pending;		// A rising edge came after the last falling edge
	volatile BOOL __last_has_rising;	// The slot at __last_ptr holds a rising edge
	volatile uint32_t __last_edge_time;
	uint32_t __timeout;
	struct {
		BOOL check;
		uint32_t min;
		uint32_t max;
	} __valid_range[3];
	PifIssuerP __p_issuer;

#ifdef PIF_COLLECT_SIGNAL
	PifCollectSignalChannel __cs[PULSE_CSF_COUNT];
#endif

	// Private Event Function
	PifEvtPulseEdge __evt_edge;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifPulse_Init
 * @brief Initializes the pulse instance and prepares all internal fields for safe use.
 * @param p_owner Pointer to the target object instance.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_Init(PifPulse* p_owner, PifId id);

/**
 * @fn pifPulse_Clear
 * @brief Clears the pulse state and releases resources currently owned by the instance.
 * @param p_owner Pointer to the target object instance.
 */
void pifPulse_Clear(PifPulse* p_owner);

/**
 * @fn pifPulse_SetMeasureMode
 * @brief Enables measurements. A getter returns 0 unless its mode is set, and falling_count only counts
 *        with PULSE_PMM_COUNT.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode Bitmask of PULSE_PMM_XXX.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_ResetMeasureMode
 * @brief Disables the given measurements.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode Bitmask of PULSE_PMM_XXX.
 */
void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_SetValidRange
 * @brief Limits the values that a getter returns. A value outside [min, max] reads as 0.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode One of PULSE_PMM_PERIOD, PULSE_PMM_LOW_WIDTH or PULSE_PMM_HIGH_WIDTH.
 * @param min Minimum accepted measurement value in microseconds.
 * @param max Maximum accepted measurement value in microseconds.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint32_t min, uint32_t max);

/**
 * @fn pifPulse_SetTimeout
 * @brief Makes the getters return 0 once no falling edge has come for timeout_us, so that a signal that
 *        stops reads as 0 instead of its last value. The time is taken from pif_act_timer1us, which must
 *        run on the same clock as the time_us given to pifPulse_sigEdge.
 * @param p_owner Pointer to the target object instance.
 * @param timeout_us Timeout in microseconds, or 0 to disable it.
 */
void pifPulse_SetTimeout(PifPulse* p_owner, uint32_t timeout_us);

/**
 * @fn pifPulse_ResetMeasureValue
 * @brief Discards the edges measured so far and clears falling_count.
 * @param p_owner Pointer to the target object instance.
 */
void pifPulse_ResetMeasureValue(PifPulse* p_owner);

/**
 * @fn pifPulse_Read
 * @brief Reads a measurement and tells why there is none. The getters below return the same value, but
 *        report every failure as 0.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @param value Measurement to read.
 * @param p_value Receives the value in microseconds: the measured one with PULSE_R_OK and PULSE_R_OUT_OF_RANGE,
 *        otherwise 0. May be NULL.
 * @return PULSE_R_OK, or the reason that there is no valid value.
 */
PifPulseResult pifPulse_Read(PifPulse* p_owner, PifPulseValue value, uint32_t* p_value);

/**
 * @fn pifPulse_GetPeriod
 * @brief Returns the time between the last two falling edges in microseconds.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @return The value, or 0 when PULSE_PMM_PERIOD is not set, there is no complete measurement yet, the value
 *         is outside the valid range or the timeout has passed. pifPulse_Read tells these apart.
 */
uint32_t pifPulse_GetPeriod(PifPulse* p_owner);

/**
 * @fn pifPulse_GetAveragePeriod
 * @brief Returns the period averaged over the falling edges kept (up to PIF_PULSE_DATA_SIZE - 1 periods).
 *        Follows PULSE_PMM_PERIOD, its valid range and the timeout like pifPulse_GetPeriod.
 * @param p_owner Pointer to the target object instance.
 * @return The average period in microseconds, or 0 under the same conditions as pifPulse_GetPeriod.
 */
uint32_t pifPulse_GetAveragePeriod(PifPulse* p_owner);

/**
 * @fn pifPulse_GetLowWidth
 * @brief Returns the low time before the last pulse in microseconds.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @return The value, or 0 when PULSE_PMM_LOW_WIDTH is not set, there is no complete measurement yet, the
 *         value is outside the valid range or the timeout has passed.
 */
uint32_t pifPulse_GetLowWidth(PifPulse* p_owner);

/**
 * @fn pifPulse_GetHighWidth
 * @brief Returns the high time of the last pulse in microseconds.
 *        Call it from a context that the edge interrupt can preempt, such as a task.
 * @param p_owner Pointer to the target object instance.
 * @return The value, or 0 when PULSE_PMM_HIGH_WIDTH is not set, there is no complete measurement yet, the
 *         value is outside the valid range or the timeout has passed.
 */
uint32_t pifPulse_GetHighWidth(PifPulse* p_owner);

/**
 * @fn pifPulse_GetLastEdgeTime
 * @brief Returns the time_us of the last edge given to pifPulse_sigEdge.
 * @param p_owner Pointer to the target object instance.
 * @return Time of the last edge in microseconds, or 0 if none came yet.
 */
uint32_t pifPulse_GetLastEdgeTime(PifPulse* p_owner);

/**
 * @fn pifPulse_sigEdge
 * @param p_owner Pointer to the target object instance.
 * @param state Current edge state value captured from the pulse input.
 * @param time_us Timestamp or interval value in microseconds.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_sigEdge(PifPulse* p_owner, PifPulseState state, uint32_t time_us);

/**
 * @fn pifPulse_AttachEvtEdge
 * @brief Attaches a callback, device, or external resource to the pulse for integration.
 * @param p_owner Pointer to the target object instance.
 * @param evt_edge Callback invoked on detected signal edge events.
 * @param p_issuer User context pointer passed to callbacks.
 */
void pifPulse_AttachEvtEdge(PifPulse* p_owner, PifEvtPulseEdge evt_edge, PifIssuerP p_issuer);

#ifdef PIF_COLLECT_SIGNAL

/**
 * @fn pifPulse_SetCsFlag
 * @brief Adds the selected signals of the instance to pifCollectSignal as channels.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to add.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

/**
 * @fn pifPulse_ResetCsFlag
 * @brief Removes the selected signals of the instance from pifCollectSignal.
 * @param p_owner Pointer to the owner instance.
 * @param flag Bit mask of the signals to remove.
 */
void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
