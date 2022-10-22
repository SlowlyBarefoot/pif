#include "protocol/pif_rc.h"


int pifRc_GetFrameLoss(PifRc* p_owner)
{
	long loss_frames = p_owner->lost_frames + p_owner->error_frames;
	return (int)(loss_frames * 100 / (p_owner->good_frames + loss_frames));
}

BOOL pifRc_CheckFailSafe(PifRc* p_owner) 
{
	BOOL timeout = FALSE;

	if (p_owner->max_frame_period) {
		timeout = (pif_cumulative_timer1ms - p_owner->last_frame_time) > p_owner->max_frame_period;
	}
	return p_owner->failsafe || timeout;
}
