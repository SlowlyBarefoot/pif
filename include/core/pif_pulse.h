#ifndef PIF_PULSE_H
#define PIF_PULSE_H


#include "core/pif_task.h"


#define PIF_PULSE_DATA_SIZE			4
#define PIF_PULSE_DATA_MASK			(PIF_PULSE_DATA_SIZE - 1)

#define PIF_PMM_PERIOD				1
#define PIF_PMM_COUNT				2
#define PIF_PMM_LOW_WIDTH			4
#define PIF_PMM_HIGH_WIDTH			8


typedef void (*PifEvtPulseEdge)(PifPulseState state, PifIssuerP p_issuer);


struct StPifPulse;
typedef struct StPifPulse PifPulse;


#ifdef PIF_COLLECT_SIGNAL

typedef enum EnPifGpioCsFlag
{
    PL_CSF_OFF			= 0,

	PL_CSF_STATE_IDX	= 0,

	PL_CSF_STATE_BIT	= 1,
	PL_CSF_ALL_BIT		= 1,

	PL_CSF_COUNT		= 1
} PifPulseCsFlag;

typedef struct
{
	PifPulse* p_owner;
    uint8_t flag;
    void* p_device[PL_CSF_COUNT];
} PifPulseColSig;

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
	uint8_t	_measure_mode;		// PIF_PMM_XXX

	// Private Member Variable
	PifPulseData __data[PIF_PULSE_DATA_SIZE];
	uint8_t __ptr, __last_ptr;
	uint8_t __count;
	struct {
		BOOL check;
		uint16_t min;
		uint16_t max;
	} __valid_range[3];
	PifIssuerP __p_issuer;

#ifdef PIF_COLLECT_SIGNAL
	PifPulseColSig* __p_colsig;
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
 * @brief Sets configuration or runtime state for the pulse based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode Measurement mode bitmask or selector.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_SetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_ResetMeasureMode
 * @brief Resets runtime state in the pulse to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode Measurement mode bitmask or selector.
 */
void pifPulse_ResetMeasureMode(PifPulse* p_owner, uint8_t measure_mode);

/**
 * @fn pifPulse_SetValidRange
 * @brief Sets configuration or runtime state for the pulse based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param measure_mode Measurement mode bitmask or selector.
 * @param min Minimum accepted measurement value.
 * @param max Maximum accepted measurement value.
 * @return TRUE on success, otherwise FALSE.
 */
BOOL pifPulse_SetValidRange(PifPulse* p_owner, uint8_t measure_mode, uint16_t min, uint16_t max);

/**
 * @fn pifPulse_ResetMeasureValue
 * @brief Resets runtime state in the pulse to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 */
void pifPulse_ResetMeasureValue(PifPulse* p_owner);

/**
 * @fn pifPulse_GetPeriod
 * @brief Retrieves the requested value or pointer from the pulse without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifPulse_GetPeriod(PifPulse* p_owner);

/**
 * @fn pifPulse_GetLowWidth
 * @brief Retrieves the requested value or pointer from the pulse without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifPulse_GetLowWidth(PifPulse* p_owner);

/**
 * @fn pifPulse_GetHighWidth
 * @brief Retrieves the requested value or pointer from the pulse without changing ownership.
 * @param p_owner Pointer to the target object instance.
 * @return Result value returned by this API.
 */
uint32_t pifPulse_GetHighWidth(PifPulse* p_owner);

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
 * @brief Sets configuration or runtime state for the pulse based on the provided parameters.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifPulse_SetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

/**
 * @fn pifPulse_ResetCsFlag
 * @brief Resets runtime state in the pulse to an initial or configured baseline.
 * @param p_owner Pointer to the target object instance.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifPulse_ResetCsFlag(PifPulse* p_owner, PifPulseCsFlag flag);

/**
 * @fn pifPulseColSig_Init
 * @brief 
 */
void pifPulseColSig_Init();

/**
 * @fn pifPulseColSig_Clear
 * @brief 
 */
void pifPulseColSig_Clear();

/**
 * @fn pifPulseColSig_SetFlag
 * @brief Sets configuration or runtime state for the pulse based on the provided parameters.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifPulseColSig_SetFlag(PifPulseCsFlag flag);

/**
 * @fn pifPulseColSig_ResetFlag
 * @brief Resets runtime state in the pulse to an initial or configured baseline.
 * @param flag Bit flag mask to set, clear, or query.
 */
void pifPulseColSig_ResetFlag(PifPulseCsFlag flag);

#endif	// PIF_COLLECT_SIGNAL

#ifdef __cplusplus
}
#endif


#endif  // PIF_PULSE_H
