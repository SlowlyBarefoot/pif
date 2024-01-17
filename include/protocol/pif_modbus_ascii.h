#ifndef PIF_MODBUS_ASCII_H
#define PIF_MODBUS_ASCII_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusAscii_AsciiToChar
 * @brief
 * @param p_ascii
 * @return
 */
short pifModbusAscii_AsciiToChar(uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_AsciiToShort
 * @brief
 * @param p_ascii
 * @return
 */
long pifModbusAscii_AsciiToShort(uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_CharToAscii
 * @brief
 * @param binary
 * @param p_ascii
 */
void pifModbusAscii_CharToAscii(uint8_t binary, uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_ShortToAscii
 * @brief
 * @param binary
 * @param p_ascii
 */
void pifModbusAscii_ShortToAscii(uint16_t binary, uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_CalcLrc
 * @brief
 * @param p_data
 * @param length
 * @return
 */
uint8_t pifModbusAscii_CalcLrc(uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_H
