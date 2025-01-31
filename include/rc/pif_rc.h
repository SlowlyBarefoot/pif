#ifndef PIF_RC_H
#define PIF_RC_H


#include "core/pif.h"


struct StPifRc;
typedef struct StPifRc PifRc;

typedef void (*PifEvtRcReceive)(PifRc* p_owner, uint16_t* channel, PifIssuerP p_issuer);


/**
 * @class StPifRc
 * @brief
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
 * @fn pifRc_AttachEvtReceive
 * @brief
 * @param p_owner PifRc 포인터
 * @param evt_receive
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifRc_AttachEvtReceive(PifRc* p_owner, PifEvtRcReceive evt_receive, PifIssuerP p_issuer);

/**
 * @fn pifRc_GetFrameLoss
 * @brief
 * @param p_owner
 * @return
 */
int pifRc_GetFrameLoss(PifRc* p_owner);

/**
 * @fn pifRc_CheckFailSafe
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifRc_CheckFailSafe(PifRc* p_owner);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_H
