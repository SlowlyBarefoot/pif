// SPDX-License-Identifier: BSD-3-Clause
#include "codec/pif_huffman.h"

// Table-driven Huffman encoder and decoder for byte streams.

void pifHuffman_InitEncoder(PifHuffmanEncoder* p_owner, const PifHuffmanCode* p_table, uint8_t* p_out, uint16_t out_size)
{
	p_owner->__p_table = p_table;
	p_owner->__p_out = p_out;
	p_owner->__out_size = out_size;
	p_owner->_bytes = 0;
	p_owner->__bit_pos = 0;
	p_owner->_overflow = FALSE;
}

BOOL pifHuffman_Encode(PifHuffmanEncoder* p_owner, const uint8_t* p_in, uint16_t in_len)
{
	const PifHuffmanCode* p_code;
	uint16_t i;
	uint8_t bit;

	if (p_owner->_overflow) return FALSE;

	for (i = 0; i < in_len; i++) {
		p_code = &p_owner->__p_table[p_in[i]];
		for (bit = p_code->length; bit > 0; bit--) {
			// Open a new, zeroed byte when the last one is full.
			if (!p_owner->__bit_pos) {
				if (p_owner->_bytes >= p_owner->__out_size) {
					p_owner->_overflow = TRUE;
					return FALSE;
				}
				p_owner->__p_out[p_owner->_bytes++] = 0;
			}
			if ((p_code->code >> (bit - 1)) & 1) {
				p_owner->__p_out[p_owner->_bytes - 1] |= (uint8_t)(0x80 >> p_owner->__bit_pos);
			}
			p_owner->__bit_pos = (p_owner->__bit_pos + 1) & 7;
		}
	}
	return TRUE;
}

uint16_t pifHuffman_Decode(const PifHuffmanCode* p_table, const uint8_t* p_in, uint16_t in_len, uint8_t* p_out, uint16_t out_count)
{
	uint32_t bit_index = 0, bit_total = (uint32_t)in_len * 8;
	uint16_t decoded = 0, code, symbol;
	uint8_t length;

	while (decoded < out_count) {
		code = 0;
		for (length = 1; length <= PIF_HUFFMAN_MAX_LENGTH; length++) {
			if (bit_index >= bit_total) return decoded;
			code = (uint16_t)((code << 1) | ((p_in[bit_index >> 3] >> (7 - (bit_index & 7))) & 1));
			bit_index++;

			for (symbol = 0; symbol < PIF_HUFFMAN_SYMBOLS; symbol++) {
				if (p_table[symbol].length == length && p_table[symbol].code == code) break;
			}
			if (symbol < PIF_HUFFMAN_SYMBOLS) {
				p_out[decoded++] = (uint8_t)symbol;
				break;
			}
		}
		if (length > PIF_HUFFMAN_MAX_LENGTH) break;
	}
	return decoded;
}
