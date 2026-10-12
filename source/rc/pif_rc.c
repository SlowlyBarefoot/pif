// SPDX-License-Identifier: BSD-3-Clause
#include "rc/pif_rc.h"


void pifRc_AttachEvtReceive(PifRc* p_owner, PifEvtRcReceive evt_receive, PifIssuerP p_issuer)
{
	p_owner->__evt_receive = evt_receive;
	p_owner->__p_issuer = p_issuer;
}

int pifRc_GetFrameLoss(PifRc* p_owner)
{
	uint32_t loss_frames = p_owner->_lost_frames + p_owner->_error_frames;
	uint32_t total_frames = p_owner->_good_frames + loss_frames;

	if (!total_frames) return 0;
	return (int)(loss_frames * 100 / total_frames);
}

BOOL pifRc_CheckFailSafe(PifRc* p_owner) 
{
	BOOL timeout = FALSE;

	if (p_owner->_max_frame_period) {
		timeout = (pif_cumulative_timer1ms - p_owner->_last_frame_time) > p_owner->_max_frame_period;
	}
	return p_owner->_failsafe || timeout;
}
