// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_modbus_ascii.h
 * @brief Encoding helpers of Modbus ASCII: hex characters and the LRC.
 */
#ifndef PIF_MODBUS_ASCII_H
#define PIF_MODBUS_ASCII_H


#include "core/pif.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decodes two hex characters into a byte.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 * @return Converted numeric value, or -1 when a character is not a hex digit.
 */
short pifModbusAscii_AsciiToChar(uint8_t *p_ascii);

/**
 * @brief Decodes four hex characters into a 16 bit value.
 * @param p_ascii Pointer to a 4-byte ASCII hexadecimal buffer.
 * @return Converted numeric value, or -1 when a character is not a hex digit.
 */
long pifModbusAscii_AsciiToShort(uint8_t *p_ascii);

/**
 * @brief Encodes a byte as two uppercase hex characters.
 * @param binary Binary value to convert.
 * @param p_ascii Pointer to a 2-byte ASCII hexadecimal buffer.
 */
void pifModbusAscii_CharToAscii(uint8_t binary, uint8_t *p_ascii);

/**
 * @brief Encodes a 16 bit value as four uppercase hex characters.
 * @param binary Binary value to convert.
 * @param p_ascii Pointer to a 4-byte ASCII hexadecimal buffer.
 */
void pifModbusAscii_ShortToAscii(uint16_t binary, uint8_t *p_ascii);

/**
 * @brief Calculates the LRC of a Modbus ASCII frame: the two's complement of the sum of the bytes.
 * @param p_data Pointer to a payload buffer.
 * @param length Number of bytes in the input buffer.
 * @return LRC byte.
 */
uint8_t pifModbusAscii_CalcLrc(uint8_t *p_data, uint16_t length);

/**
 * @brief Calculates the LRC of a frame still held as hex digits, over the bytes they stand for.
 * @param p_ascii Pointer to the hex digits, two per byte.
 * @param length Number of hex digits.
 * @return LRC of the bytes.
 */
uint8_t pifModbusAscii_CalcLrcOfAscii(uint8_t *p_ascii, uint16_t length);

/**
 * @brief Turns hex digits into the bytes they stand for, in place.
 * @param p_buffer Hex digits in, bytes out.
 * @param length Number of hex digits.
 * @return Number of bytes, or -1 when the length is odd or a character is not a hex digit.
 */
int16_t pifModbusAscii_Decode(uint8_t *p_buffer, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MODBUS_ASCII_H
