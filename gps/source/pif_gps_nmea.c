#include "pif_gps_nmea.h"

static void _evtParsing(void* p_client, PifActCommReceiveData act_receive_data)
{
	PifGpsNmea *p_owner = (PifGpsNmea *)p_client;
	uint8_t c;

	while ((*act_receive_data)(p_owner->__p_comm, &c)) {
		pifGps_ParsingNmea(&p_owner->_gps, c);
	}
}

BOOL pifGpsNmea_Init(PifGpsNmea* p_owner, PifId id)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
	    return FALSE;
	}

	memset(p_owner, 0, sizeof(PifGpsNmea));

    if (!pifGps_Init(&p_owner->_gps, id)) goto fail;
    return TRUE;

fail:
	pifGpsNmea_Clear(p_owner);
    return FALSE;
}

void pifGpsNmea_Clear(PifGpsNmea* p_owner)
{
	pifGps_Clear(&p_owner->_gps);
}

void pifGpsNmea_AttachComm(PifGpsNmea* p_owner, PifComm* p_comm)
{
	p_owner->__p_comm = p_comm;
	pifComm_AttachClient(p_comm, p_owner, _evtParsing, NULL);
}

void pifGpsNmea_DetachComm(PifGpsNmea* p_owner)
{
	pifComm_DetachClient(p_owner->__p_comm);
	p_owner->__p_comm = NULL;
}
