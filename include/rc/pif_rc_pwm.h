#ifndef PIF_RC_PWM_H
#define PIF_RC_PWM_H


#include "rc/pif_rc.h"


#define PIF_RC_PWM_DATA_SIZE			4
#define PIF_RC_PWM_DATA_MASK			(PIF_RC_PWM_DATA_SIZE - 1)


/**
 * @struct StPifRcPwmPulse
 * @brief One captured PWM pulse edge pair.
 */
typedef struct StPifRcPwmPulse
{
	uint32_t rising;
	uint32_t falling;
} PifRcPwmPulse;

/**
 * @struct StPifRcPwmData
 * @brief Per-channel PWM measurement history.
 */
typedef struct StPifRcPwmData
{
	PifRcPwmPulse pulse[PIF_RC_PWM_DATA_SIZE];
	uint8_t ptr, last_ptr;
} PifRcPwmData;

/**
 * @struct StPifRcPwm
 * @brief Runtime state for PWM decoding.
 */
typedef struct StPifRcPwm
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
	struct {
		BOOL check		: 1;
		uint16_t min	: 15;
		uint16_t max;
	} __valid_range;
	uint8_t __max_channel;
	uint8_t __process_step;
	PifRcPwmData* __p_data;
	uint16_t* __p_channel;
} PifRcPwm;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcPwm_Init
 * @brief Initializes a PWM receiver instance.
 * @param p_owner Pointer to the PWM receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @param channel_count Number of channels to decode.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcPwm_Init(PifRcPwm* p_owner, PifId id, uint8_t channel_count);

/**
 * @fn pifRcPwm_Clear
 * @brief Releases resources allocated by the PWM receiver instance.
 * @param p_owner Pointer to the PWM receiver object.
 */
void pifRcPwm_Clear(PifRcPwm* p_owner);

/**
 * @fn pifRcPwm_SetValidRange
 * @brief Sets the valid pulse width range for decoded PWM values.
 * @param p_owner Pointer to the PWM receiver object.
 * @param min Minimum accepted pulse width in microseconds.
 * @param max Maximum accepted pulse width in microseconds.
 * @return TRUE after the range is applied.
 */
BOOL pifRcPwm_SetValidRange(PifRcPwm* p_owner, uint32_t min, uint32_t max);

/**
 * @fn pifRcPwm_ResetMeasureValue
 * @brief Resets buffered edge measurements for all channels.
 * @param p_owner Pointer to the PWM receiver object.
 */
void pifRcPwm_ResetMeasureValue(PifRcPwm* p_owner);

/**
 * @fn pifRcPwm_sigEdge
 * @brief Processes one PWM edge event.
 * @param p_owner Pointer to the PWM receiver object.
 * @param channel Zero-based channel index.
 * @param state Edge type (rising or falling).
 * @param time_us Edge timestamp in microseconds.
 * @return Decoded pulse width value, or 0 when no complete pulse is available.
 */
uint16_t pifRcPwm_sigEdge(PifRcPwm* p_owner, uint8_t channel, PifPulseState state, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PWM_H
