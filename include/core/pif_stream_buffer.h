// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_stream_buffer.h
 * @brief Cursor over a byte buffer for building and parsing packets field by field.
 *
 * Writes and reads of 8 to 32 bit values in either byte order move the cursor and stop at the end of the buffer,
 * so a packet is checked once for overflow instead of after every field.
 */
#ifndef PIF_STREAM_BUFFER_H
#define PIF_STREAM_BUFFER_H


#include "core/pif.h"


/**
 * @class StPifStreamBuffer
 * @brief Cursor over a caller-provided byte buffer for building and parsing packets.
 *
 * Writes and reads move the cursor. One that would pass the end of the buffer
 * does nothing, sets _overflow and, for a read, returns 0. Check _overflow once
 * after a whole packet instead of after every field.
 */
typedef struct StPifStreamBuffer
{
	// Public Member Variable

	// Read-only Member Variable
	uint8_t* _p_ptr;			///< Next byte to write or read
	uint8_t* _p_end;			///< One past the last usable byte
	BOOL _overflow;				///< Set when a write or a read would have passed the end
} PifStreamBuffer;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts a stream over size bytes at p_buffer.
 * @param p_owner Pointer to the target object instance.
 * @param p_buffer Pointer to the caller-provided buffer.
 * @param size Size of the buffer in bytes.
 */
void pifStreamBuffer_Init(PifStreamBuffer* p_owner, uint8_t* p_buffer, uint16_t size);

/**
 * @brief Turns a written stream into a reader of what was written.
 *
 * The end moves to the current position and the cursor goes back to p_base.
 * @param p_owner Pointer to the target object instance.
 * @param p_base Start of the written data, normally the buffer passed to pifStreamBuffer_Init().
 */
void pifStreamBuffer_SwitchToReader(PifStreamBuffer* p_owner, uint8_t* p_base);

/**
 * @brief Returns the number of bytes between the cursor and the end.
 * @param p_owner Pointer to the target object instance.
 * @return Remaining bytes.
 */
uint16_t pifStreamBuffer_Remaining(const PifStreamBuffer* p_owner);

/**
 * @brief Moves the cursor forward without touching the bytes, for example after filling them in place.
 * @param p_owner Pointer to the target object instance.
 * @param size Number of bytes to skip.
 * @return TRUE if successful, FALSE if fewer than size bytes remain.
 */
BOOL pifStreamBuffer_Advance(PifStreamBuffer* p_owner, uint16_t size);

/**
 * @brief Writes one byte.
 * @param p_owner Pointer to the target object instance.
 * @param value Value to write.
 */
void pifStreamBuffer_WriteU8(PifStreamBuffer* p_owner, uint8_t value);

/**
 * @brief Writes a 16-bit value, little endian.
 * @param p_owner Pointer to the target object instance.
 * @param value Value to write.
 */
void pifStreamBuffer_WriteU16(PifStreamBuffer* p_owner, uint16_t value);

/**
 * @brief Writes a 32-bit value, little endian.
 * @param p_owner Pointer to the target object instance.
 * @param value Value to write.
 */
void pifStreamBuffer_WriteU32(PifStreamBuffer* p_owner, uint32_t value);

/**
 * @brief Writes a 16-bit value, big endian.
 * @param p_owner Pointer to the target object instance.
 * @param value Value to write.
 */
void pifStreamBuffer_WriteU16Be(PifStreamBuffer* p_owner, uint16_t value);

/**
 * @brief Writes a 32-bit value, big endian.
 * @param p_owner Pointer to the target object instance.
 * @param value Value to write.
 */
void pifStreamBuffer_WriteU32Be(PifStreamBuffer* p_owner, uint32_t value);

/**
 * @brief Writes the same byte size times.
 * @param p_owner Pointer to the target object instance.
 * @param value Byte to write.
 * @param size Number of bytes.
 */
void pifStreamBuffer_Fill(PifStreamBuffer* p_owner, uint8_t value, uint16_t size);

/**
 * @brief Copies a block of bytes into the stream.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Pointer to the bytes to write.
 * @param size Number of bytes.
 */
void pifStreamBuffer_WriteData(PifStreamBuffer* p_owner, const void* p_data, uint16_t size);

/**
 * @brief Writes the characters of a string.
 * @param p_owner Pointer to the target object instance.
 * @param p_string NUL-terminated string.
 * @param terminator TRUE to write the terminating NUL as well.
 */
void pifStreamBuffer_WriteString(PifStreamBuffer* p_owner, const char* p_string, BOOL terminator);

/**
 * @brief Reads one byte.
 * @param p_owner Pointer to the target object instance.
 * @return Value read, or 0 on overflow.
 */
uint8_t pifStreamBuffer_ReadU8(PifStreamBuffer* p_owner);

/**
 * @brief Reads a 16-bit value, little endian.
 * @param p_owner Pointer to the target object instance.
 * @return Value read, or 0 on overflow.
 */
uint16_t pifStreamBuffer_ReadU16(PifStreamBuffer* p_owner);

/**
 * @brief Reads a 32-bit value, little endian.
 * @param p_owner Pointer to the target object instance.
 * @return Value read, or 0 on overflow.
 */
uint32_t pifStreamBuffer_ReadU32(PifStreamBuffer* p_owner);

/**
 * @brief Reads a 16-bit value, big endian.
 * @param p_owner Pointer to the target object instance.
 * @return Value read, or 0 on overflow.
 */
uint16_t pifStreamBuffer_ReadU16Be(PifStreamBuffer* p_owner);

/**
 * @brief Reads a 32-bit value, big endian.
 * @param p_owner Pointer to the target object instance.
 * @return Value read, or 0 on overflow.
 */
uint32_t pifStreamBuffer_ReadU32Be(PifStreamBuffer* p_owner);

/**
 * @brief Copies a block of bytes out of the stream.
 * @param p_owner Pointer to the target object instance.
 * @param p_data Destination buffer. Left unchanged on overflow.
 * @param size Number of bytes.
 * @return TRUE if successful, FALSE on overflow.
 */
BOOL pifStreamBuffer_ReadData(PifStreamBuffer* p_owner, void* p_data, uint16_t size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_STREAM_BUFFER_H
