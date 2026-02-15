#ifndef PIF_MODBUS_ASCII_H
#define PIF_MODBUS_ASCII_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifModbusAscii_AsciiToChar
 * @brief Converts data between wire format and native format.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 * @return Converted numeric value.
 */
short pifModbusAscii_AsciiToChar(uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_AsciiToShort
 * @brief Converts data between wire format and native format.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 * @return Converted numeric value.
 */
long pifModbusAscii_AsciiToShort(uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_CharToAscii
 * @brief Converts data between wire format and native format.
 * @param binary Binary value to convert.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 */
void pifModbusAscii_CharToAscii(uint8_t binary, uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_ShortToAscii
 * @brief Converts data between wire format and native format.
 * @param binary Binary value to convert.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 */
void pifModbusAscii_ShortToAscii(uint16_t binary, uint8_t *p_ascii);

/**
 * @fn pifModbusAscii_CalcLrc
 * @brief Calculates a checksum or CRC value.
 * @param p_data Pointer to a payload buffer.
 * @param length Number of bytes in the input buffer.
 * @return Computed or decoded 8-bit value.
 */
uint8_t pifModbusAscii_CalcLrc(uint8_t *p_data, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_H
