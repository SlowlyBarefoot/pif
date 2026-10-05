// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_HUFFMAN_H
#define PIF_HUFFMAN_H


#include "core/pif.h"


#define PIF_HUFFMAN_SYMBOLS			256		// One code per byte value
#define PIF_HUFFMAN_MAX_LENGTH		16


/**
 * @class StPifHuffmanCode
 * @brief Code of one byte value. The table passed to the encoder and the decoder
 *        holds PIF_HUFFMAN_SYMBOLS entries indexed by byte value and must be prefix-free.
 *
 * tools/gen_huffman_table.py generates such a table from sample data.
 */
typedef struct StPifHuffmanCode
{
	uint16_t code;				// Code bits, right aligned; sent most significant bit first
	uint8_t length;				// Number of bits, 1 to PIF_HUFFMAN_MAX_LENGTH
} PifHuffmanCode;

/**
 * @class StPifHuffmanEncoder
 * @brief Huffman encoder that writes into a caller-provided buffer and can be fed in pieces.
 *
 * Bits are packed most significant bit first. The unused low bits of the last
 * byte are 0, so the decoder must be told how many symbols to read.
 */
typedef struct StPifHuffmanEncoder
{
	// Public Member Variable

	// Read-only Member Variable
	uint16_t _bytes;			// Bytes written so far, including a partly filled last byte
	BOOL _overflow;				// Set when a code did not fit; later input is ignored

	// Private Member Variable
	const PifHuffmanCode* __p_table;
	uint8_t* __p_out;
	uint16_t __out_size;
	uint8_t __bit_pos;			// Bits used in the last byte, 0 when it is full
} PifHuffmanEncoder;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifHuffman_InitEncoder
 * @brief Starts encoding into a buffer.
 * @param p_owner Pointer to the target object instance.
 * @param p_table Code table of PIF_HUFFMAN_SYMBOLS entries. It must stay valid while encoding.
 * @param p_out Destination buffer.
 * @param out_size Size of the destination buffer in bytes.
 */
void pifHuffman_InitEncoder(PifHuffmanEncoder* p_owner, const PifHuffmanCode* p_table, uint8_t* p_out, uint16_t out_size);

/**
 * @fn pifHuffman_Encode
 * @brief Encodes bytes and appends their codes to the output.
 * @param p_owner Pointer to the target object instance.
 * @param p_in Bytes to encode.
 * @param in_len Number of bytes.
 * @return TRUE if every code fit, FALSE if the output buffer is full.
 */
BOOL pifHuffman_Encode(PifHuffmanEncoder* p_owner, const uint8_t* p_in, uint16_t in_len);

/**
 * @fn pifHuffman_Decode
 * @brief Decodes up to out_count bytes from a bit stream written by pifHuffman_Encode().
 *
 * Each symbol is found by comparing the bits read so far with every code of the
 * same length, so this is meant for tools and tests rather than hot paths.
 * @param p_table Code table of PIF_HUFFMAN_SYMBOLS entries.
 * @param p_in Encoded bytes.
 * @param in_len Number of encoded bytes.
 * @param p_out Destination of the decoded bytes.
 * @param out_count Number of bytes to decode.
 * @return Number of bytes decoded. Less than out_count if the input ran out or held a code not in the table.
 */
uint16_t pifHuffman_Decode(const PifHuffmanCode* p_table, const uint8_t* p_in, uint16_t in_len, uint8_t* p_out, uint16_t out_count);

#ifdef __cplusplus
}
#endif


#endif  // PIF_HUFFMAN_H
