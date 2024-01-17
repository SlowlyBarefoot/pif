#ifndef PIF_MODBUS_RTU_H
#define PIF_MODBUS_RTU_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusRtu_CalcCrc
 * @brief
 * @param p_data
 * @param length
 * @return
 */
uint16_t pifModbusRtu_CalcCrc(const uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_RTU_H
