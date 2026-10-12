// SPDX-License-Identifier: BSD-3-Clause
#include "protocol/pif_modbus.h"


uint16_t pifModbus_StreamToShort(uint8_t *p_stream)
{
	return (p_stream[0] << 8) + p_stream[1];
}

void pifModbus_ShortToStream(uint16_t value, uint8_t *p_stream)
{
	p_stream[0] = value >> 8;
	p_stream[1] = value & 0xFF;
}

void pifModbus_StartTimeout(PifModbusTimeout *p_timeout, uint32_t time)
{
	p_timeout->start = (*pif_act_timer1us)();
	p_timeout->time = time ? time : 1;
}

void pifModbus_StopTimeout(PifModbusTimeout *p_timeout)
{
	p_timeout->time = 0;
}

BOOL pifModbus_IsExpired(PifModbusTimeout *p_timeout)
{
	if (!p_timeout->time || (*pif_act_timer1us)() - p_timeout->start < p_timeout->time) return FALSE;
	p_timeout->time = 0;
	return TRUE;
}

PifModbusError pifModbus_CheckResponse(const uint8_t *p_request, const uint8_t *p_response, uint16_t length,
		PifModbusResultKind kind, uint16_t quantity)
{
	uint16_t count;

	if (length < 3) return MBE_INVALID_RESPONSE;

	if (p_response[0] != p_request[0]) return MBE_INVALID_UNIT_ID;

	if (p_response[1] == (p_request[1] | 0x80)) {
		if (length != 3 || !p_response[2]) return MBE_INVALID_RESPONSE;
		return (PifModbusError)p_response[2];
	}

	if (p_response[1] != p_request[1]) return MBE_INVALID_RESPONSE;

	switch (kind) {
	case MBRK_BITS:
		count = (quantity + 7) / 8;
		if (p_response[2] != count || length != 3 + count) return MBE_INVALID_RESPONSE;
		break;

	case MBRK_REGISTERS:
		count = 2 * quantity;
		if (p_response[2] != count || length != 3 + count) return MBE_INVALID_RESPONSE;
		break;

	default:
		// A write is answered with the address and the value or quantity it was given.
		if (length != 6 || memcmp(&p_response[2], &p_request[2], 4)) return MBE_INVALID_RESPONSE;
		break;
	}
	return MBE_NONE;
}
