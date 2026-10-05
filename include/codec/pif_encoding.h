// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_ENCODING_H
#define PIF_ENCODING_H


#include "core/pif.h"


// Largest number of bytes pifEncoding_UvarintEncode() writes for a uint32_t.
#define PIF_UVARINT32_MAX_SIZE		5


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifEncoding_ZigzagEncode
 * @brief Maps a signed value to an unsigned one so that small magnitudes stay small (0, -1, 1, -2 -> 0, 1, 2, 3).
 * @param value Signed value.
 * @return Zigzag code.
 */
uint32_t pifEncoding_ZigzagEncode(int32_t value);

/**
 * @fn pifEncoding_ZigzagDecode
 * @brief Inverse of pifEncoding_ZigzagEncode().
 * @param value Zigzag code.
 * @return Signed value.
 */
int32_t pifEncoding_ZigzagDecode(uint32_t value);

/**
 * @fn pifEncoding_UvarintEncode
 * @brief Writes an unsigned varint: 7 bits per byte, least significant group first,
 *        bit 7 set on every byte but the last (the Protocol Buffers base-128 varint).
 * @param value Value to encode.
 * @param p_buffer Destination buffer.
 * @param size Size of the destination buffer in bytes.
 * @return Number of bytes written, 1 to PIF_UVARINT32_MAX_SIZE, or 0 if the buffer is too small.
 */
uint8_t pifEncoding_UvarintEncode(uint32_t value, uint8_t* p_buffer, uint16_t size);

/**
 * @fn pifEncoding_UvarintDecode
 * @brief Reads an unsigned varint written by pifEncoding_UvarintEncode().
 * @param p_value Destination of the decoded value. Left unchanged on failure.
 * @param p_buffer Source buffer.
 * @param size Number of bytes available in the source buffer.
 * @return Number of bytes consumed, or 0 if the varint is truncated or does not fit in 32 bits.
 */
uint8_t pifEncoding_UvarintDecode(uint32_t* p_value, const uint8_t* p_buffer, uint16_t size);

/**
 * @fn pifEncoding_FloatToBits
 * @brief Returns the IEEE 754 bit pattern of a float.
 * @param value Float value.
 * @return Bit pattern.
 */
uint32_t pifEncoding_FloatToBits(float value);

/**
 * @fn pifEncoding_BitsToFloat
 * @brief Returns the float with an IEEE 754 bit pattern.
 * @param bits Bit pattern.
 * @return Float value.
 */
float pifEncoding_BitsToFloat(uint32_t bits);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ENCODING_H
