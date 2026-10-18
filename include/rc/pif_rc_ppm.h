// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rc_ppm.h
 * @brief PPM (CPPM) receiver decoded from the edges of one input pin.
 *
 * The platform gives the time of the same edge of every pulse with pifRcPpm_sigTick(), usually from an edge
 * interrupt. After the sync gap, the channels of the frame go to the evt_receive of PifRc.
 */
#ifndef PIF_RC_PPM_H
#define PIF_RC_PPM_H


#include "rc/pif_rc.h"


/// _channel is an int8_t that may sit one past the last channel, so this is the most channels allowed.
#define PIF_RC_PPM_MAX_CHANNEL			(INT8_MAX - 1)

/// A receiver that stops sending frames for this long has lost the link.
#define PIF_RC_PPM_DEFAULT_TIMEOUT_MS	100


/**
 * @struct StPifRcPpm
 * @brief Runtime state for PPM decoding.
 */
typedef struct StPifRcPpm
{
	/// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Read-only Member Variable
	int8_t _channel;				///< Index of the last pulse in the current frame, -1 right after a sync

	// Private Member Variable
	BOOL __time_valid;
	BOOL __frame_error;
	uint8_t __max_channel;
	uint8_t __process_step;
	uint8_t __learn_count;
	uint16_t __valid_range_min;
	uint16_t __valid_range_max;		///< 0 when no valid range is set
	uint16_t __threshold_1us;
	uint32_t __last_time;
	uint16_t* __p_channel;			///< Channels of the last good frame, given to evt_receive
	uint16_t* __p_work;				///< Channels of the frame being received
} PifRcPpm;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a PPM receiver instance.
 * @param p_owner Pointer to the PPM receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @param channel_count Number of channels to decode, from 1 to PIF_RC_PPM_MAX_CHANNEL.
 *        Channels the transmitter sends beyond this are ignored.
 * @param threshold_1us Frame separation threshold in microseconds.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcPpm_Init(PifRcPpm* p_owner, PifId id, uint8_t channel_count, uint16_t threshold_1us);

/**
 * @brief Releases resources allocated by the PPM receiver instance.
 * @param p_owner Pointer to the PPM receiver object.
 */
void pifRcPpm_Clear(PifRcPpm* p_owner);

/**
 * @brief Sets the valid pulse width range for decoded channel values.
 *        A frame with any channel outside the range is counted as an error frame and not delivered.
 * @param p_owner Pointer to the PPM receiver object.
 * @param min Minimum accepted pulse width in microseconds.
 * @param max Maximum accepted pulse width in microseconds. 0 turns the range check off.
 * @return TRUE if the range is applied, FALSE if min is greater than max.
 */
BOOL pifRcPpm_SetValidRange(PifRcPpm* p_owner, uint16_t min, uint16_t max);

/**
 * @brief Sets how long without a good frame counts as a lost link for pifRc_CheckFailSafe().
 * @param p_owner Pointer to the PPM receiver object.
 * @param timeout_ms Time in milliseconds, or 0 for no timeout.
 */
void pifRcPpm_SetTimeout(PifRcPpm* p_owner, uint16_t timeout_ms);

/**
 * @brief Restarts decoding from the next sync and learns the channel count again.
 * @param p_owner Pointer to the PPM receiver object.
 */
void pifRcPpm_ResetMeasureValue(PifRcPpm* p_owner);

/**
 * @brief Processes one captured pulse timestamp.
 *        It is usually called from an edge interrupt, and evt_receive is then called in that interrupt.
 *        The channel count is learned from two frames in a row with the same count before frames are delivered.
 * @param p_owner Pointer to the PPM receiver object.
 * @param time_us Timestamp in microseconds of the same edge of every pulse.
 * @return Decoded pulse width value, or 0 when no channel value is available.
 */
uint16_t pifRcPpm_sigTick(PifRcPpm* p_owner, uint32_t time_us);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_PPM_H
