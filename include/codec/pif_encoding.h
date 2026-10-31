// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_encoding.h
 * @brief Encodings of numbers in a byte stream: zigzag, unsigned varint and the bit pattern of a float.
 *        Also fixed width values in either byte order, and COBS, which frames a byte stream with 0x00
 *        delimiters.
 */
#ifndef PIF_ENCODING_H
#define PIF_ENCODING_H


#include "core/pif.h"


/// Largest number of bytes pifEncoding_UvarintEncode() writes for a uint32_t.
#define PIF_UVARINT32_MAX_SIZE		5

/// Largest number of bytes pifEncoding_CobsEncode() writes for LEN bytes, the delimiter not included.
#define PIF_COBS_MAX_ENCODED(LEN)	((LEN) + (LEN) / 254 + 1)

/** @name Results of pifEncoding_CobsDecoderPut() besides a frame length
 *  @{
 */
#define PIF_COBS_MORE				(-1)	///< In a frame, or between frames
#define PIF_COBS_ERROR				(-2)	///< The frame just ended is broken or did not fit, and is dropped
/** @} */


/**
 * @class StPifCobsDecoder
 * @brief Decodes a COBS stream byte by byte, as it comes from a UART. A 0x00 ends each frame.
 *
 * A broken frame is dropped up to the next 0x00, so the decoder finds the next frame by itself.
 */
typedef struct StPifCobsDecoder
{
	// Private Member Variable
	uint8_t *__p_buffer;
	uint16_t __size;
	uint16_t __length;			///< Bytes decoded in the current frame
	uint8_t __code;				///< Code of the current block, 0 before the first byte of a frame
	uint8_t __remain;			///< Data bytes left in the current block
	BOOL __error;
} PifCobsDecoder;


/** @name Fixed width values in a byte buffer
 *  Little endian unless the name ends in Be. Defined here so they inline: they are as fast as writing the
 *  shifts in place, without each module keeping its own copy.
 *  @{
 */

/** @brief Writes a 16-bit value, little endian. */
PIF_STATIC_INLINE void pifEncoding_PutU16(uint8_t *p_buffer, uint16_t value)
{
	p_buffer[0] = (uint8_t)value;
	p_buffer[1] = (uint8_t)(value >> 8);
}

/** @brief Writes a 32-bit value, little endian. */
PIF_STATIC_INLINE void pifEncoding_PutU32(uint8_t *p_buffer, uint32_t value)
{
	p_buffer[0] = (uint8_t)value;
	p_buffer[1] = (uint8_t)(value >> 8);
	p_buffer[2] = (uint8_t)(value >> 16);
	p_buffer[3] = (uint8_t)(value >> 24);
}

/** @brief Writes a 16-bit value, big endian. */
PIF_STATIC_INLINE void pifEncoding_PutU16Be(uint8_t *p_buffer, uint16_t value)
{
	p_buffer[0] = (uint8_t)(value >> 8);
	p_buffer[1] = (uint8_t)value;
}

/** @brief Writes a 32-bit value, big endian. */
PIF_STATIC_INLINE void pifEncoding_PutU32Be(uint8_t *p_buffer, uint32_t value)
{
	p_buffer[0] = (uint8_t)(value >> 24);
	p_buffer[1] = (uint8_t)(value >> 16);
	p_buffer[2] = (uint8_t)(value >> 8);
	p_buffer[3] = (uint8_t)value;
}

/** @brief Reads a 16-bit value, little endian. */
PIF_STATIC_INLINE uint16_t pifEncoding_GetU16(const uint8_t *p_buffer)
{
	return (uint16_t)(p_buffer[0] | ((uint16_t)p_buffer[1] << 8));
}

/** @brief Reads a 32-bit value, little endian. */
PIF_STATIC_INLINE uint32_t pifEncoding_GetU32(const uint8_t *p_buffer)
{
	return (uint32_t)p_buffer[0] | ((uint32_t)p_buffer[1] << 8) |
			((uint32_t)p_buffer[2] << 16) | ((uint32_t)p_buffer[3] << 24);
}

/** @brief Reads a 16-bit value, big endian. */
PIF_STATIC_INLINE uint16_t pifEncoding_GetU16Be(const uint8_t *p_buffer)
{
	return (uint16_t)(((uint16_t)p_buffer[0] << 8) | p_buffer[1]);
}

/** @brief Reads a 32-bit value, big endian. */
PIF_STATIC_INLINE uint32_t pifEncoding_GetU32Be(const uint8_t *p_buffer)
{
	return ((uint32_t)p_buffer[0] << 24) | ((uint32_t)p_buffer[1] << 16) |
			((uint32_t)p_buffer[2] << 8) | (uint32_t)p_buffer[3];
}

/** @} */


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maps a signed value to an unsigned one so that small magnitudes stay small (0, -1, 1, -2 -> 0, 1, 2, 3).
 * @param value Signed value.
 * @return Zigzag code.
 */
uint32_t pifEncoding_ZigzagEncode(int32_t value);

/**
 * @brief Inverse of pifEncoding_ZigzagEncode().
 * @param value Zigzag code.
 * @return Signed value.
 */
int32_t pifEncoding_ZigzagDecode(uint32_t value);

/**
 * @brief Writes an unsigned varint: 7 bits per byte, least significant group first,
 *        bit 7 set on every byte but the last (the Protocol Buffers base-128 varint).
 * @param value Value to encode.
 * @param p_buffer Destination buffer.
 * @param size Size of the destination buffer in bytes.
 * @return Number of bytes written, 1 to PIF_UVARINT32_MAX_SIZE, or 0 if the buffer is too small.
 */
uint8_t pifEncoding_UvarintEncode(uint32_t value, uint8_t* p_buffer, uint16_t size);

/**
 * @brief Reads an unsigned varint written by pifEncoding_UvarintEncode().
 * @param p_value Destination of the decoded value. Left unchanged on failure.
 * @param p_buffer Source buffer.
 * @param size Number of bytes available in the source buffer.
 * @return Number of bytes consumed, or 0 if the varint is truncated or does not fit in 32 bits.
 */
uint8_t pifEncoding_UvarintDecode(uint32_t* p_value, const uint8_t* p_buffer, uint16_t size);

/**
 * @brief Returns the IEEE 754 bit pattern of a float.
 * @param value Float value.
 * @return Bit pattern.
 */
uint32_t pifEncoding_FloatToBits(float value);

/**
 * @brief Returns the float with an IEEE 754 bit pattern.
 * @param bits Bit pattern.
 * @return Float value.
 */
float pifEncoding_BitsToFloat(uint32_t bits);

/**
 * @brief Encodes a frame with COBS (Consistent Overhead Byte Stuffing), so it holds no 0x00 and a 0x00 can
 *        end it on the wire. The delimiter is not written.
 * @param p_src Frame.
 * @param length Bytes of the frame, 0 allowed.
 * @param p_dst Destination. It must not overlap p_src.
 * @param size Size of the destination. PIF_COBS_MAX_ENCODED(length) is always enough.
 * @return Bytes written, at least 1, or 0 if the destination is too small.
 */
uint16_t pifEncoding_CobsEncode(const uint8_t *p_src, uint16_t length, uint8_t *p_dst, uint16_t size);

/**
 * @brief Decodes a COBS frame written by pifEncoding_CobsEncode().
 * @param p_src Encoded frame without its delimiter.
 * @param length Bytes of the encoded frame.
 * @param p_dst Destination. It may be p_src: decoding never writes ahead of what it has read.
 * @param size Size of the destination.
 * @param p_length Destination of the decoded length. Left unchanged on failure.
 * @return TRUE on success, FALSE if the frame is broken or the destination is too small.
 */
BOOL pifEncoding_CobsDecode(const uint8_t *p_src, uint16_t length, uint8_t *p_dst, uint16_t size,
		uint16_t *p_length);

/**
 * @brief Sets up a byte by byte COBS decoder.
 * @param p_owner The decoder.
 * @param p_buffer Buffer for the decoded frame. It must stay valid while the decoder is used.
 * @param size Size of p_buffer, the longest frame it takes.
 */
void pifEncoding_CobsDecoderInit(PifCobsDecoder *p_owner, uint8_t *p_buffer, uint16_t size);

/**
 * @brief Forgets the frame being decoded. The bytes up to the next 0x00 are taken as a new frame.
 * @param p_owner The decoder.
 */
void pifEncoding_CobsDecoderReset(PifCobsDecoder *p_owner);

/**
 * @brief Takes the next byte of the stream.
 * @param p_owner The decoder.
 * @param data Byte received.
 * @return The length of the frame in the buffer when data was the 0x00 that ends it, PIF_COBS_MORE
 *         otherwise, or PIF_COBS_ERROR when the frame it ended is dropped. The frame stays in the buffer
 *         until the next byte is put. A 0x00 with no frame before it gives PIF_COBS_MORE.
 */
int pifEncoding_CobsDecoderPut(PifCobsDecoder *p_owner, uint8_t data);

#ifdef __cplusplus
}
#endif


#endif  // PIF_ENCODING_H
