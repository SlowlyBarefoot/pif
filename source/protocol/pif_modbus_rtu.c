#include "protocol/pif_modbus_rtu.h"


uint16_t pifModbusRtu_CalcCrc(const uint8_t *p_data, uint16_t length)
{
    uint16_t i, crc = 0xFFFF;

    for (i = 0; i < length; i++) {
        crc ^= (uint16_t)p_data[i];
        for (int j = 8; j != 0; j--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }

    return (uint16_t)(crc << 8) | (uint16_t)(crc >> 8);
}
