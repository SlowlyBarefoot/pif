// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus_rtu.h
 * @brief CRC-16 of Modbus RTU frames.
 */
#ifndef PIF_MODBUS_RTU_H
#define PIF_MODBUS_RTU_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculates the CRC-16/MODBUS of a frame (polynomial 0xA001 reflected, starting from 0xFFFF).
 * @param p_data Pointer to a payload buffer.
 * @param length Number of bytes in the input buffer.
 * @return CRC, sent low byte first.
 */
uint16_t pifModbusRtu_CalcCrc(const uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_H
