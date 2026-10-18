// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rc.h
 * @brief Base of the RC receivers: channels, failsafe and frame statistics.
 *
 * Each receiver protocol (PPM, PWM, SBUS, iBus, CRSF, SUMD, Spektrum) embeds a PifRc as its first member, so the
 * application handles every receiver the same way.
 */
#ifndef PIF_RC_H
#define PIF_RC_H


#include "core/pif.h"


struct StPifRc;
typedef struct StPifRc PifRc;

/**
 * @brief Callback invoked when a complete RC frame has been decoded.
 * @param p_owner Pointer to the RC base object that produced the frame.
 * @param channel Pointer to decoded channel values.
 * @param p_issuer User-defined issuer context forwarded from the RC object.
 */
typedef void (*PifEvtRcReceive)(PifRc* p_owner, uint16_t* channel, PifIssuerP p_issuer);


/**
 * @class StPifRc
 * @brief Base RC receiver data shared by protocol-specific receiver implementations.
 */
struct StPifRc
{
	// Read-only Member Variable
    PifId _id;	///< Instance ID
	uint8_t _channel_count;	///< Number of channels in a frame

	BOOL _failsafe;	///< TRUE while no frame came within _max_frame_period
	uint16_t _max_frame_period;	///< Time in ms without a frame that means failsafe
    uint32_t _last_frame_time;	///< Time in ms of the last good frame, from pif_cumulative_timer1ms

	uint32_t _good_frames;	///< Frames decoded
	uint32_t _error_frames;	///< Frames dropped as invalid
	uint32_t _lost_frames;	///< Frames the receiver reported as lost

    // Private Event Function
	PifEvtRcReceive __evt_receive;
	PifIssuerP __p_issuer;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the RC frame receive callback and callback issuer context.
 * @param p_owner Pointer to the RC object.
 * @param evt_receive Callback function called when a valid frame is received.
 * @param p_issuer User-defined issuer context passed to evt_receive.
 */
void pifRc_AttachEvtReceive(PifRc* p_owner, PifEvtRcReceive evt_receive, PifIssuerP p_issuer);

/**
 * @brief Calculates frame loss percentage using good, lost, and error frame counters.
 * @param p_owner Pointer to the RC object.
 * @return Frame loss ratio in percent.
 */
int pifRc_GetFrameLoss(PifRc* p_owner);

/**
 * @brief Checks whether failsafe is active due to protocol state or frame timeout.
 * @param p_owner Pointer to the RC object.
 * @return TRUE if failsafe is active, otherwise FALSE.
 */
BOOL pifRc_CheckFailSafe(PifRc* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_H
