// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_ring_buffer.h"

#include <string.h>

// Byte ring buffer with overflow policies and transactional put support.

static void _lock(PifRingBuffer* p_owner, BOOL lock)
{
	if (p_owner->__act_lock) (*p_owner->__act_lock)(p_owner, lock);
}

static void _publish(PifRingBuffer* p_owner)
{
	// Outside a put session every byte becomes readable as soon as it is written.
	if (!p_owner->_bt.putting) p_owner->__head = p_owner->__put_head;
}

static BOOL _chopOff(PifRingBuffer* p_owner, uint16_t count)
{
	uint16_t length;
	uint16_t size, tail;
	BOOL rtn = FALSE;

	// Drop old data according to the configured overflow policy. Only committed bytes, those before
	// __head, can be dropped. The tail belongs to the reader, so it moves under the lock.
	// Claimed bytes are the oldest, so nothing can be dropped while the reader holds any.
	_lock(p_owner, TRUE);
	switch (p_owner->__claim ? RB_CHOP_OFF_NONE : p_owner->_bt.chop_off) {
	case RB_CHOP_OFF_CHAR:
		size = 0;
		tail = p_owner->__tail;
		while (tail != p_owner->__head) {
			if (p_owner->__p_buffer[tail] == p_owner->__ui.chop_off_char && size >= count) {
				p_owner->__tail = tail;
				rtn = TRUE;
				break;
			}
			tail++;
			if (tail >= p_owner->_size) tail -= p_owner->_size;
			size++;
		}
		break;

	case RB_CHOP_OFF_LENGTH:
		if (!p_owner->__ui.chop_off_length) break;
		length = pifRingBuffer_GetFillSize(p_owner);
		size = p_owner->__ui.chop_off_length;
		while (count > size) {
			size += p_owner->__ui.chop_off_length;
		}
		if (size < length) {
			tail = p_owner->__tail;
			tail += size;
			if (tail >= p_owner->_size) tail -= p_owner->_size;
			p_owner->__tail = tail;
			rtn = TRUE;
		}
		else if (count <= length) {
			p_owner->__tail = p_owner->__head;
			rtn = TRUE;
		}
		break;
	}
	_lock(p_owner, FALSE);
	return rtn;
}

PifRingBuffer* pifRingBuffer_CreateHeap(PifId id, uint16_t size)
{
	PifRingBuffer* p_owner = calloc(sizeof(PifRingBuffer), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifRingBuffer_InitHeap(p_owner, id, size)) {
		pifRingBuffer_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

PifRingBuffer* pifRingBuffer_CreateStatic(PifId id, uint16_t size, uint8_t* p_buffer)
{
	PifRingBuffer* p_owner = calloc(sizeof(PifRingBuffer), 1);
	if (!p_owner) {
		pif_error = E_OUT_OF_HEAP;
		return NULL;
	}

	if (!pifRingBuffer_InitStatic(p_owner, id, size, p_buffer)) {
		pifRingBuffer_Destroy(&p_owner);
		return NULL;
	}
    return p_owner;
}

void pifRingBuffer_Destroy(PifRingBuffer** pp_owner)
{
	if (pp_owner) {
		pifRingBuffer_Clear(*pp_owner);
		free(*pp_owner);
		*pp_owner = NULL;
	}
}

BOOL pifRingBuffer_InitHeap(PifRingBuffer* p_owner, PifId id, uint16_t size)
{
    if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRingBuffer));

	p_owner->__p_buffer = calloc(sizeof(uint8_t), size);
	if (!p_owner->__p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		goto fail;
	}

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
    p_owner->_size = size;
    return TRUE;

fail:
	pifRingBuffer_Clear(p_owner);
    return FALSE;
}

BOOL pifRingBuffer_InitStatic(PifRingBuffer* p_owner, PifId id, uint16_t size, uint8_t* p_buffer)
{
    if (!p_owner || !size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifRingBuffer));

	p_owner->__p_buffer = p_buffer;

	if (id == PIF_ID_AUTO) id = pif_id++;
	p_owner->_id = id;
    p_owner->_bt.is_static = TRUE;
    p_owner->_size = size;
    return TRUE;
}

void pifRingBuffer_Clear(PifRingBuffer* p_owner)
{
	if (p_owner->_bt.is_static == FALSE && p_owner->__p_buffer) {
        free(p_owner->__p_buffer);
    }
    p_owner->__p_buffer = NULL;
}

void pifRingBuffer_Empty(PifRingBuffer* p_owner)
{
	// Make the buffer empty without clearing allocated memory.
	_lock(p_owner, TRUE);
	p_owner->__tail = p_owner->__head;
	p_owner->__claim = 0;
	_lock(p_owner, FALSE);
}

BOOL pifRingBuffer_ResizeHeap(PifRingBuffer* p_owner, uint16_t size)
{
	uint8_t* p_buffer;

    if (p_owner->_bt.is_static) {
		pif_error = E_INVALID_STATE;
    	return FALSE;
    }
    if (!size) {
		pif_error = E_INVALID_PARAM;
    	return FALSE;
    }

	// Allocate first so a failure leaves the current buffer intact.
	p_buffer = calloc(sizeof(uint8_t), size);
	if (!p_buffer) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
    if (p_owner->__p_buffer) free(p_owner->__p_buffer);
	p_owner->__p_buffer = p_buffer;
    p_owner->_size = size;
	p_owner->__head = 0;
	p_owner->__put_head = 0;
	p_owner->__tail = 0;
	p_owner->__claim = 0;
	p_owner->_bt.putting = FALSE;
	return TRUE;
}

void pifRingBuffer_SetName(PifRingBuffer* p_owner, const char* p_name)
{
	p_owner->__p_name = p_name;
}

void pifRingBuffer_AttachActLock(PifRingBuffer* p_owner, PifActRingBufferLock act_lock)
{
	p_owner->__act_lock = act_lock;
}

uint8_t* pifRingBuffer_GetBuffer(PifRingBuffer* p_owner)
{
	return p_owner->__p_buffer;
}

uint8_t *pifRingBuffer_GetTailPointer(PifRingBuffer* p_owner, uint16_t pos)
{
	return &p_owner->__p_buffer[(p_owner->__tail + pos) % p_owner->_size];
}

uint8_t* pifRingBuffer_GetReadPointer(PifRingBuffer* p_owner, uint16_t pos, uint16_t* p_length)
{
	uint16_t fill, tail, length = 0;

	// The pointer and the length come from the same tail, and the claim is set before the lock is left,
	// so a chop-off in between cannot make them disagree or drop the bytes.
	_lock(p_owner, TRUE);
	fill = pifRingBuffer_GetFillSize(p_owner);
	tail = (p_owner->__tail + pos) % p_owner->_size;
	if (pos < fill) {
		length = fill - pos;
		if (length > p_owner->_size - tail) length = p_owner->_size - tail;
		if (p_owner->__claim < pos + length) p_owner->__claim = pos + length;
	}
	_lock(p_owner, FALSE);
	*p_length = length;
	return &p_owner->__p_buffer[tail];
}

BOOL pifRingBuffer_MoveHeadForLinear(PifRingBuffer* p_owner, uint16_t size)
{
	BOOL rtn = TRUE;

	if (size >= p_owner->_size || p_owner->_bt.putting) return FALSE;

	// Rewinding an empty buffer moves the reader's tail too, so it is done under the lock.
	_lock(p_owner, TRUE);
	if (p_owner->__put_head != p_owner->__tail) {
		rtn = FALSE;
	}
	else if (p_owner->_size - p_owner->__put_head < size) {
		p_owner->__head = p_owner->__put_head = p_owner->__tail = 0;
		p_owner->__claim = 0;
	}
	_lock(p_owner, FALSE);
	return rtn;
}

BOOL pifRingBuffer_SetHead(PifRingBuffer* p_owner, uint16_t pos)
{
	uint16_t head = p_owner->__put_head;
	uint16_t written, remain, tail;

	// The bytes are already in the buffer memory; only the head catches up. No division, since this
	// runs in the DMA interrupt.
	if (pos >= p_owner->_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (pos == head) return TRUE;

	written = pos > head ? pos - head : p_owner->_size - head + pos;
	remain = pifRingBuffer_GetRemainSize(p_owner);
	p_owner->__put_head = pos;
	if (written > remain) {
		// The writer ran over the oldest bytes, so drop them.
		tail = pos + 1;
		if (tail >= p_owner->_size) tail = 0;
		_lock(p_owner, TRUE);
		p_owner->__tail = tail;
		_publish(p_owner);
		_lock(p_owner, FALSE);
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	_publish(p_owner);
	return TRUE;
}

void pifRingBuffer_ChopsOffNone(PifRingBuffer* p_owner)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_NONE;
}

void pifRingBuffer_ChopsOffChar(PifRingBuffer* p_owner, char ch)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_CHAR;
	p_owner->__ui.chop_off_char = ch;
}

void pifRingBuffer_ChopsOffLength(PifRingBuffer* p_owner, uint16_t length)
{
	p_owner->_bt.chop_off = RB_CHOP_OFF_LENGTH;
	p_owner->__ui.chop_off_length = length;
}

BOOL pifRingBuffer_IsBuffer(PifRingBuffer* p_owner)
{
	return p_owner->__p_buffer != NULL;
}

BOOL pifRingBuffer_IsEmpty(PifRingBuffer* p_owner)
{
	return p_owner->__head == p_owner->__tail;
}

uint16_t pifRingBuffer_GetFillSize(PifRingBuffer* p_owner)
{
	// Read head and tail once each, since an interrupt may move either of them.
	uint16_t head = p_owner->__head;
	uint16_t tail = p_owner->__tail;

    if (head >= tail) {
    	return head - tail;
    }
    else {
    	return p_owner->_size - tail + head;
    }
}

uint16_t pifRingBuffer_GetRemainSize(PifRingBuffer* p_owner)
{
	// Room for the writer, so it counts from the write cursor, which includes uncommitted bytes.
	uint16_t head = p_owner->__put_head;
	uint16_t tail = p_owner->__tail;

    if (head < tail) {
    	return tail - head - 1;
    }
    else {
    	return p_owner->_size - head + tail - 1;
    }
}

void pifRingBuffer_BeginPutting(PifRingBuffer* p_owner)
{
	// Start a put session. Bytes put from now on stay hidden from the reader until the commit, since
	// __head stays where the session started. An unfinished earlier session is dropped.
	p_owner->__put_head = p_owner->__head;
	p_owner->_bt.putting = TRUE;
}

void pifRingBuffer_CommitPutting(PifRingBuffer* p_owner)
{
	// Hand every byte of the session to the reader at once.
	p_owner->_bt.putting = FALSE;
	p_owner->__head = p_owner->__put_head;
}

void pifRingBuffer_RollbackPutting(PifRingBuffer* p_owner)
{
	// The reader never saw the session, so going back to __head discards it.
	p_owner->__put_head = p_owner->__head;
	p_owner->_bt.putting = FALSE;
}

uint8_t* pifRingBuffer_GetPointerPutting(PifRingBuffer* p_owner, uint16_t pos)
{
	// __head is where the session started while it is open.
	return &p_owner->__p_buffer[(p_owner->__head + pos) % p_owner->_size];
}

BOOL pifRingBuffer_PutByte(PifRingBuffer* p_owner, uint8_t data)
{
    uint16_t next;

    // Keep one slot empty to distinguish full vs empty state.
    next = p_owner->__put_head + 1;
	if (next >= p_owner->_size) next = 0;
    if (next == p_owner->__tail) {
    	if (!_chopOff(p_owner, 1)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    p_owner->__p_buffer[p_owner->__put_head] = data;
    p_owner->__put_head = next;
    _publish(p_owner);
    return TRUE;
}

BOOL pifRingBuffer_PutData(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t remain = pifRingBuffer_GetRemainSize(p_owner);

    if (length > remain) {
    	if (!_chopOff(p_owner, length - remain)) {
    		pif_error = E_OVERFLOW_BUFFER;
    		return FALSE;
    	}
    }

    // Store the head once per byte, and only with a wrapped value, since a
    // reader in an interrupt may be looking at it.
    uint16_t head = p_owner->__put_head;
    for (uint16_t i = 0; i < length; i++) {
    	p_owner->__p_buffer[head] = p_data[i];
    	head++;
    	if (head >= p_owner->_size) head = 0;
    	p_owner->__put_head = head;
    	_publish(p_owner);
    }
    return TRUE;
}

BOOL pifRingBuffer_PutString(PifRingBuffer* p_owner, char* p_string)
{
	size_t length = strlen(p_string);

	if (length >= p_owner->_size) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}
	return pifRingBuffer_PutData(p_owner, (uint8_t*)p_string, length);
}

BOOL pifRingBuffer_GetByte(PifRingBuffer* p_owner, uint8_t* p_data)
{
	uint16_t tail;
	BOOL rtn = FALSE;

	// The writer may move the tail to drop old data, so the read and the store are done together.
	_lock(p_owner, TRUE);
	tail = p_owner->__tail;
	if (tail != p_owner->__head) {
		*p_data = p_owner->__p_buffer[tail];
		tail++;
		if (tail >= p_owner->_size) tail = 0;
		p_owner->__tail = tail;
		if (p_owner->__claim) p_owner->__claim--;
		rtn = TRUE;
	}
	_lock(p_owner, FALSE);
	return rtn;
}

uint16_t pifRingBuffer_GetBytes(PifRingBuffer* p_owner, uint8_t* p_data, uint16_t length)
{
	uint16_t i, tail;

	_lock(p_owner, TRUE);
	tail = p_owner->__tail;
	for (i = 0; i < length; i++) {
		if (tail == p_owner->__head) break;

		p_data[i] = p_owner->__p_buffer[tail];
		tail++;
		if (tail >= p_owner->_size) tail = 0;
	}
	p_owner->__tail = tail;
	p_owner->__claim = p_owner->__claim > i ? p_owner->__claim - i : 0;
	_lock(p_owner, FALSE);
	return i;
}

uint16_t pifRingBuffer_CopyToArray(uint8_t* p_dst, uint16_t count, PifRingBuffer* p_src, uint16_t pos)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_src);

	if (pos >= fill) return 0;
	if (count > fill - pos) count = fill - pos;

	uint16_t tail = (p_src->__tail + pos) % p_src->_size;
	for (uint16_t i = 0; i < count; i++) {
		p_dst[i] = p_src->__p_buffer[tail];
		tail++;
		if (tail >= p_src->_size) tail = 0;
	}
	return count;
}

uint16_t pifRingBuffer_CopyAll(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_src);
	uint16_t remain = pifRingBuffer_GetRemainSize(p_dst);

    if (pos >= fill) return 0;

    fill -= pos;
	uint16_t length = remain < fill ? remain : fill;
	uint16_t tail = p_src->__tail + pos;
	if (tail >= p_src->_size) tail -= p_src->_size;
	for (uint16_t i = 0; i < length; i++) {
		pifRingBuffer_PutByte(p_dst, p_src->__p_buffer[tail]);
		tail++;
		if (tail >= p_src->_size) tail = 0;
	}
	return length;
}

BOOL pifRingBuffer_CopyLength(PifRingBuffer* p_dst, PifRingBuffer* p_src, uint16_t pos, uint16_t length)
{
	uint16_t fill = pifRingBuffer_GetFillSize(p_src);

	if (pos + length > fill) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (pifRingBuffer_GetRemainSize(p_dst) < length) {
		pif_error = E_OVERFLOW_BUFFER;
		return FALSE;
	}

	// Start copying from the source offset after range validation.
	uint16_t usTail = (p_src->__tail + pos) % p_src->_size;
	for (uint16_t i = 0; i < length; i++) {
		pifRingBuffer_PutByte(p_dst, p_src->__p_buffer[usTail]);
		usTail++;
		if (usTail >= p_src->_size) usTail = 0;
	}
	return TRUE;
}

void pifRingBuffer_Remove(PifRingBuffer* p_owner, uint16_t size)
{
	uint16_t fill;

	_lock(p_owner, TRUE);
	fill = pifRingBuffer_GetFillSize(p_owner);
	if (size >= fill) {
		p_owner->__tail = p_owner->__head;
	}
	else {
		p_owner->__tail = (p_owner->__tail + size) % p_owner->_size;
	}
	// The reader is done with what it was handed, whether it sent all of it or not.
	p_owner->__claim = 0;
	_lock(p_owner, FALSE);
}
