#ifndef PIF_MODBUS_RTU_H
#define PIF_MODBUS_RTU_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusRtu_CalcCrc
 * @brief Calculates a checksum or CRC value.
 * @param p_data Pointer to a payload buffer.
 * @param length Number of bytes in the input buffer.
 * @return Computed or decoded 16-bit value.
 */
uint16_t pifModbusRtu_CalcCrc(const uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_H
