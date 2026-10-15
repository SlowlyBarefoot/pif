// SPDX-License-Identifier: GPL-3.0-or-later
#include "gps/pif_gps_nmea.h"


/**
 * @brief Transport receive callback that feeds bytes into the NMEA parser.
 * @param p_client Pointer to `PifGpsNmea`.
 * @param act_receive Transport receive function pointer.
 * @return `TRUE` if at least one byte was consumed, otherwise `FALSE`.
 */
static BOOL _evtParsing(void *p_client, PifActTransportReceive act_receive)
{
	PifGpsNmea *p_owner = (PifGpsNmea *)p_client;
	uint8_t c;
	BOOL rtn = FALSE;

	while ((*act_receive)(p_owner->__p_transport, &c, 1)) {
		rtn = TRUE;
		if (pifGps_ParsingNmea(&p_owner->_gps, c)) break;
	}
	return rtn;
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
	pifGpsNmea_DetachTransport(p_owner);
	pifGps_Clear(&p_owner->_gps);
}

BOOL pifGpsNmea_AttachTransport(PifGpsNmea *p_owner, PifTransport *p_transport)
{
	if (!p_owner || !p_transport) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	pifGpsNmea_DetachTransport(p_owner);
	p_owner->__p_transport = p_transport;
	pifTransport_AttachClient(p_transport, p_owner, _evtParsing, NULL, NULL);
	return TRUE;
}

void pifGpsNmea_DetachTransport(PifGpsNmea *p_owner)
{
	if (!p_owner || !p_owner->__p_transport) return;

	pifTransport_DetachClient(p_owner->__p_transport);
	p_owner->__p_transport = NULL;
}
