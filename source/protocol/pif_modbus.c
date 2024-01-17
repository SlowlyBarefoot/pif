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
