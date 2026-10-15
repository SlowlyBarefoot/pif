// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_transport.h"


void pifTransport_Init(PifTransport *p_owner, const PifTransportOps *p_ops)
{
	p_owner->__p_ops = p_ops;
}

void pifTransport_AttachClient(PifTransport *p_owner, void *p_client, PifEvtTransportParsing evt_parsing,
		PifEvtTransportSending evt_sending, PifEvtTransportAbortRx evt_abort_rx)
{
	(*p_owner->__p_ops->attach_client)(p_owner, p_client, evt_parsing, evt_sending, evt_abort_rx);
}

void pifTransport_DetachClient(PifTransport *p_owner)
{
	(*p_owner->__p_ops->detach_client)(p_owner);
}

void pifTransport_TriggerSend(PifTransport *p_owner)
{
	if (p_owner->__p_ops->trigger_send) (*p_owner->__p_ops->trigger_send)(p_owner);
}

SWITCH pifTransport_GetFlowState(PifTransport *p_owner)
{
	if (!p_owner->__p_ops->get_flow_state) return ON;
	return (*p_owner->__p_ops->get_flow_state)(p_owner);
}

uint8_t pifTransport_GetFrameSize(PifTransport *p_owner)
{
	if (!p_owner->__p_ops->get_frame_size) return 1;
	return (*p_owner->__p_ops->get_frame_size)(p_owner);
}

uint16_t pifTransport_GetByteTime(PifTransport *p_owner)
{
	if (!p_owner->__p_ops->get_byte_time) return 0;
	return (*p_owner->__p_ops->get_byte_time)(p_owner);
}
