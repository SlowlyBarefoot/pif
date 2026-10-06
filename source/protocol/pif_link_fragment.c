#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif
#include "protocol/pif_link_fragment.h"


// Entry of a large request in the request buffer: [0 (2)][data size (2)][PifLinkRequest *][uint8_t *data][dst_id].
// The leading 0 tells it from a request frame, whose length is never 0.
#define ENTRY_DATA_POS		(4 + sizeof(const PifLinkRequest *))
#define ENTRY_DST_ID_POS	(ENTRY_DATA_POS + sizeof(uint8_t *))
#define ENTRY_SIZE			(ENTRY_DST_ID_POS + 1)

// Fragment byte at the start of the data of a fragment.
#define FRAGMENT_LAST		0x80
#define FRAGMENT_SEQ_MASK	0x7F

// CRC and ETX after the data of a frame.
#define TAILER_SIZE			4


// seq of the fragment after one with seq. 0 is used only by the first fragment.
static uint8_t _nextSeq(uint8_t seq)
{
	return seq >= FRAGMENT_SEQ_MASK ? 1 : seq + 1;
}

static BOOL _isBroadcast(PifLink *p_owner, PifLinkPacket *p_packet)
{
	return p_owner->_type == LINK_T_MULTI && p_packet->dst_id == PIF_LINK_BROADCAST;
}

// Makes the frame of the current fragment of the large request in progress.
static BOOL _buildFragment(PifLink *p_owner)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;
	PifLinkTx *p_tx = &p_owner->__tx;
	const PifLinkRequest *p_request = p_tx->p_request;
	uint8_t flags, info;
	uint16_t size;
	BOOL last;

	// A fragment escapes every byte at worst, and the fragment byte is added to its data.
	size = p_owner->__header_size + 2 * (p_frag->tx_size + 1) + TAILER_SIZE + 1;
	if (p_owner->__p_uart->_frame_size > 1) size += p_owner->__p_uart->_frame_size - 1;
	if (p_frag->tx_buffer._size < size) {
		if (!pifRingBuffer_ResizeHeap(&p_frag->tx_buffer, size)) return FALSE;
		pifRingBuffer_SetName(&p_frag->tx_buffer, "FGB");
	}

	p_frag->tx_chunk = p_frag->tx_total - p_frag->tx_offset;
	if (p_frag->tx_chunk > p_frag->tx_size) p_frag->tx_chunk = p_frag->tx_size;
	last = p_frag->tx_offset + p_frag->tx_chunk >= p_frag->tx_total;

	// Every fragment but the last waits for its answer before the next one is sent.
	flags = LINK_F_TYPE_REQUEST | LINK_F_FRAGMENT | (p_request->flags & (LINK_F_LOG_PRINT_MASK | LINK_F_RLE_MASK));
	if (last) flags |= p_request->flags & LINK_F_RESPONSE_MASK;
	info = (last ? FRAGMENT_LAST : 0) | p_frag->tx_seq;
	p_tx->packet_id = pifLink_NewPacketId(p_owner);
	p_tx->flags = LINK_F_ALWAYS | flags;

	pifRingBuffer_Empty(&p_frag->tx_buffer);
	p_tx->length = pifLink_PutFrame(p_owner, &p_frag->tx_buffer, flags, p_request->command, p_tx->packet_id,
			p_tx->dst_id, &info, p_frag->p_tx_data + p_frag->tx_offset, p_frag->tx_chunk, &p_tx->crc_pos);

#ifndef PIF_NO_LOG
	if (flags & LINK_F_LOG_PRINT_MASK) {
		pifLog_Printf(LT_COMM, "LK(%u) Rq:%xh F:%xh P:%d S:%xh O:%u L:%d=%d", p_owner->_id, p_request->command,
				flags, p_tx->packet_id, info, p_frag->tx_offset, p_frag->tx_chunk, p_tx->length);
	}
#endif
	return p_tx->length != 0;
}

static BOOL _startRequest(PifLink *p_owner)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;
	PifLinkTx *p_tx = &p_owner->__tx;
	uint8_t entry[ENTRY_SIZE];

	p_tx->entry_size = ENTRY_SIZE;
	pifRingBuffer_CopyToArray(entry, ENTRY_SIZE, &p_tx->request_buffer, 0);
	p_frag->tx_total = pifLink_GetU16(entry + 2);
	memcpy(&p_frag->p_tx_data, entry + ENTRY_DATA_POS, sizeof(p_frag->p_tx_data));
	p_tx->dst_id = entry[ENTRY_DST_ID_POS];
	p_tx->p_frame_buffer = &p_frag->tx_buffer;
	p_tx->frame_base = 0;
	p_frag->tx_offset = 0;
	p_frag->tx_seq = 0;
	return _buildFragment(p_owner);
}

static int8_t _nextFragment(PifLink *p_owner)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;

	p_frag->tx_offset += p_frag->tx_chunk;
	if (p_frag->tx_offset >= p_frag->tx_total) return 0;

	p_frag->tx_seq = _nextSeq(p_frag->tx_seq);
	return _buildFragment(p_owner) ? 1 : -1;
}

// Checks a fragment and takes it according to the receive mode. Returns the packet to hand to evt_answer, or
// NULL when the fragment is already answered or is kept for reassembly.
static PifLinkPacket *_handleQuestion(PifLink *p_owner, PifLinkPacket *p_packet, BOOL supported)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;
	PifLinkError error = LINK_E_NONE;
	uint8_t info = 0, seq = 0;
	uint16_t size = 0;

	// A broadcast fragment cannot be answered, so the sender would never send the next one.
	if (_isBroadcast(p_owner, p_packet)) return NULL;

	if (!supported || p_frag->rx_mode == LINK_FM_NONE) {
		error = LINK_E_UNSUPPORTED;
	}
	else if (!p_packet->data_count) {
		error = LINK_E_INVALID_PARAM;
	}
	else {
		info = p_packet->p_data[0];
		seq = info & FRAGMENT_SEQ_MASK;
		size = p_packet->data_count - 1;
		if (!seq) {
			p_frag->rx_active = TRUE;
			p_frag->rx_command = p_packet->command;
			p_frag->rx_src_id = p_packet->src_id;
			p_frag->rx_offset = 0;
		}
		else if (!p_frag->rx_active || p_frag->rx_command != p_packet->command ||
				p_frag->rx_src_id != p_packet->src_id || seq != p_frag->rx_seq) {
			error = LINK_E_INVALID_PARAM;
		}
		if (!error && p_frag->rx_mode == LINK_FM_REASSEMBLE && size > p_frag->message_size - p_frag->rx_offset) {
			error = LINK_E_INVALID_PARAM;
		}
	}
	if (error) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_WARN, "LK(%u) Fragment %xh:%d seq:%d rejected: %d", p_owner->_id, p_packet->command,
				p_packet->packet_id, seq, error);
#endif
		p_frag->rx_active = FALSE;
		if ((p_packet->flags & LINK_F_ANSWER_MASK) == LINK_F_ANSWER_YES) {
			pifLink_MakeError(p_owner, p_packet, p_packet->flags & LINK_F_LOG_PRINT_MASK, error);
		}
		return NULL;
	}

	p_frag->rx_seq = _nextSeq(seq);
	if (info & FRAGMENT_LAST) p_frag->rx_active = FALSE;

	if (p_frag->rx_mode == LINK_FM_STREAM) {
		p_packet->p_data++;
		p_packet->length--;
		p_packet->data_count--;
		p_packet->offset = p_frag->rx_offset;
		p_packet->more = !(info & FRAGMENT_LAST);
		p_frag->rx_offset += size;
		return p_packet;
	}

	memcpy(p_frag->p_message + p_frag->rx_offset, p_packet->p_data + 1, size);
	p_frag->rx_offset += size;
	if (!(info & FRAGMENT_LAST)) {
		if ((p_packet->flags & LINK_F_ANSWER_MASK) == LINK_F_ANSWER_YES) {
			pifLink_MakeAnswer(p_owner, p_packet, p_packet->flags & LINK_F_LOG_PRINT_MASK, NULL, 0);
		}
		return NULL;
	}
	p_frag->message = *p_packet;
	p_frag->message.p_data = p_frag->p_message;
	p_frag->message.length = p_frag->rx_offset;
	p_frag->message.data_count = p_frag->rx_offset;
	return &p_frag->message;
}

static void _clear(PifLink *p_owner)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;

	if (!p_frag) return;
	pifRingBuffer_Clear(&p_frag->tx_buffer);
	if (p_frag->p_message) free(p_frag->p_message);
	free(p_frag);
	p_owner->__p_fragment = NULL;
}

static const PifLinkFragmentOps kFragmentOps = {
	_handleQuestion,
	_startRequest,
	_nextFragment,
	_clear
};

BOOL pifLink_EnableFragment(PifLink *p_owner, PifLinkFragmentMode rx_mode, uint16_t message_size)
{
	PifLinkFragment *p_frag = p_owner->__p_fragment;
	uint8_t *p_message = NULL;

	if ((rx_mode != LINK_FM_NONE && rx_mode != LINK_FM_STREAM && rx_mode != LINK_FM_REASSEMBLE) ||
			(rx_mode == LINK_FM_REASSEMBLE && !message_size)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (rx_mode == LINK_FM_REASSEMBLE) {
		p_message = malloc(message_size);
		if (!p_message) {
			pif_error = E_OUT_OF_HEAP;
			return FALSE;
		}
	}
	if (!p_frag) {
		p_frag = calloc(1, sizeof(PifLinkFragment));
		if (!p_frag) {
			if (p_message) free(p_message);
			pif_error = E_OUT_OF_HEAP;
			return FALSE;
		}
		p_frag->tx_size = PIF_LINK_TX_FRAGMENT_SIZE;
		p_owner->__p_fragment = p_frag;
		p_owner->__p_fragment_ops = &kFragmentOps;
	}

	if (p_frag->p_message) free(p_frag->p_message);
	p_frag->p_message = p_message;
	p_frag->message_size = p_message ? message_size : 0;
	p_frag->rx_mode = rx_mode;
	p_frag->rx_active = FALSE;
	return TRUE;
}

BOOL pifLink_SetTxFragmentSize(PifLink *p_owner, uint16_t fragment_size)
{
	if (!p_owner->__p_fragment || p_owner->__tx.large) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (!fragment_size || fragment_size >= PIF_LINK_MAX_DATA_SIZE) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_owner->__p_fragment->tx_size = fragment_size;
	return TRUE;
}

BOOL pifLink_MakeLargeRequest(PifLink *p_owner, uint8_t dst_id, const PifLinkRequest *p_request, uint8_t *p_data,
		uint16_t data_size)
{
	uint8_t entry[ENTRY_SIZE];

	if (!p_owner->__p_fragment || !p_owner->__p_uart) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	if (!p_request || p_request->command < 0x20 || !p_data || !data_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_type == LINK_T_MULTI) {
		if (dst_id >= PIF_LINK_BROADCAST || dst_id == p_owner->_address) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}
	}
	else {
		dst_id = 0;
	}

	pifLink_PutU16(entry, 0);
	pifLink_PutU16(entry + 2, data_size);
	memcpy(entry + 4, &p_request, sizeof(p_request));
	memcpy(entry + ENTRY_DATA_POS, &p_data, sizeof(p_data));
	entry[ENTRY_DST_ID_POS] = dst_id;
	return pifRingBuffer_PutData(&p_owner->__tx.request_buffer, entry, ENTRY_SIZE);
}
