// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_RC_PWM_H
#define PIF_RC_PWM_H


#include "rc/pif_rc.h"


#define PIF_RC_PWM_DEFAULT_TIMEOUT_MS	100


/**
 * @struct StPifRcPwm
 * @brief Runtime state for PWM decoding.
 */
typedef struct StPifRcPwm
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
	BOOL __frame_error;
	uint8_t __max_channel;
	uint16_t __valid_range_min;
	uint16_t __valid_range_max;		// 0 when no valid range is set
	uint32_t* __p_rising;			// Rising edge time per channel, 0 when none is pending
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
 * @param channel_count Number of channels to decode. A frame ends on the falling edge of the last channel.
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
 * @param max Maximum accepted pulse width in microseconds. 0 turns the range check off.
 * @return TRUE if the range is applied, FALSE if min is greater than max.
 */
BOOL pifRcPwm_SetValidRange(PifRcPwm* p_owner, uint16_t min, uint16_t max);

/**
 * @fn pifRcPwm_SetTimeout
 * @brief Sets how long without a good frame counts as a lost link for pifRc_CheckFailSafe().
 * @param p_owner Pointer to the PWM receiver object.
 * @param timeout_ms Time in milliseconds, or 0 for no timeout.
 */
void pifRcPwm_SetTimeout(PifRcPwm* p_owner, uint16_t timeout_ms);

/**
 * @fn pifRcPwm_ResetMeasureValue
 * @brief Discards pending rising edges and the error state of the current frame.
 * @param p_owner Pointer to the PWM receiver object.
 */
void pifRcPwm_ResetMeasureValue(PifRcPwm* p_owner);

/**
 * @fn pifRcPwm_sigEdge
 * @brief Processes one PWM edge event.
 * @details Usually called from an edge interrupt, so the receive event also runs in that interrupt.
 * @param p_owner Pointer to the PWM receiver object.
 * @param channel Zero-based channel index.
 * @param state Edge type (rising or falling).
 * @param time_us Edge timestamp in microseconds.
 * @return Decoded pulse width, or 0 when the edge does not end a valid pulse.
 */
uint16_t pifRcPwm_sigEdge(PifRcPwm* p_owner, uint8_t channel, PifPulseState state, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PWM_H
