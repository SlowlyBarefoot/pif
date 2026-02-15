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
    PifId _id;
	uint8_t _channel_count;

	BOOL _failsafe;
	uint16_t _max_frame_period;
    uint32_t _last_frame_time;

	uint32_t _good_frames;
	uint32_t _error_frames;
	uint32_t _lost_frames;

    // Private Event Function
	PifEvtRcReceive __evt_receive;
	PifIssuerP* __p_issuer;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
v * @brief Registers the RC frame receive callback and callback issuer context.
 * @param p_owner Pointer to the RC object.
 * @param evt_receive Callback function called when a valid frame is received.
 * @param p_issuer User-defined issuer context passed to evt_receive.
 */
void pifRc_AttachEvtReceive(PifRc* p_owner, PifEvtRcReceive evt_receive, PifIssuerP p_issuer);

/**
 * @fn pifRc_GetFrameLoss
 * @brief Calculates frame loss percentage using good, lost, and error frame counters.
 * @param p_owner Pointer to the RC object.
 * @return Frame loss ratio in percent.
 */
int pifRc_GetFrameLoss(PifRc* p_owner);

/**
 * @fn pifRc_CheckFailSafe
 * @brief Checks whether failsafe is active due to protocol state or frame timeout.
 * @param p_owner Pointer to the RC object.
 * @return TRUE if failsafe is active, otherwise FALSE.
 */
BOOL pifRc_CheckFailSafe(PifRc* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_RC_H
