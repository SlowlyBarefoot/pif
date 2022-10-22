#ifndef PIF_RC_H
#define PIF_RC_H


#include "core/pif.h"


#define FAILSAFE_INACTIVE 	0
#define FAILSAFE_ACTIVE   	1


/**
 * @class StPifRc
 * @brief
 */
typedef struct StPifRc
{
    PifId id;
	uint8_t channel_count;

	BOOL failsafe;
	uint16_t max_frame_period;
    uint32_t last_frame_time;

	uint32_t good_frames;
	uint32_t error_frames;
	uint32_t lost_frames;
} PifRc;


typedef void (*PifEvtRcReceive)(PifRc* p_owner, uint16_t* channel);


#ifdef __cplusplus
extern "C" {
#endif

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
