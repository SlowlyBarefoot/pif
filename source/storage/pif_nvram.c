// SPDX-License-Identifier: BSD-3-Clause
#include "core/pif_task_manager.h"
#include "storage/pif_nvram.h"


#define RECORD_UNKNOWN		0xFE	// Neither copy has been looked at yet
#define RECORD_NONE			0xFF	// Neither copy is intact

#define OP_RECORD_NONE		0xFFFF	// The transfer is not a record write


typedef enum EnCopyCheck
{
	CC_INTACT,
	CC_BROKEN,
	CC_ACCESS_FAILED
} CopyCheck;


/**
 * @fn _startWriteCycle
 * @brief Notes when the write cycle that has just begun will be over. Nothing is waited for here:
 *        the data is already in the device, and the cycle only has to have passed by the time the
 *        device is touched again.
 * @param p_owner Pointer to the NVRAM instance.
 */
static void _startWriteCycle(PifNvram* p_owner)
{
	if (!p_owner->__write_delay_ms) return;

	p_owner->__ready_time = pif_cumulative_timer1ms + p_owner->__write_delay_ms;
	p_owner->__write_pending = TRUE;
}

/**
 * @fn _waitWriteCycle
 * @brief Gives back whatever is left of the write cycle of the previous write, holding the CPU
 *        for that remainder alone. Whatever the caller did between the two accesses counts
 *        towards the cycle, and usually all of it does, which leaves nothing to wait for.
 * @param p_owner Pointer to the NVRAM instance.
 */
static void _waitWriteCycle(PifNvram* p_owner)
{
	int32_t remain;

	if (!p_owner->__write_pending) return;

	// Read signed, so that a cycle whose end is already behind the current time reads as expired
	// rather than as most of a timer period away.
	remain = (int32_t)(p_owner->__ready_time - pif_cumulative_timer1ms);
	if (remain > 0) pif_Delay1ms((uint32_t)remain);
	p_owner->__write_pending = FALSE;
}

/**
 * @fn _chunkTicks
 * @brief How long to leave the media before the next page of the transfer, in ticks of the
 *        timer manager.
 * @param p_owner Pointer to the NVRAM instance.
 * @return Ticks to wait, at least one.
 */
static uint32_t _chunkTicks(PifNvram* p_owner)
{
	uint32_t period = p_owner->__p_timer_manager->_period1us;
	uint32_t wait;

	// A write has to leave the device alone for its cycle, which is what the timer is really for.
	// A read has nothing to wait for and asks for the shortest tick there is, only so that the
	// pages land in separate releases instead of all in this one.
	wait = p_owner->__op_write ? p_owner->__write_delay_ms * 1000UL : 0UL;
	if (!wait) return 1UL;

	// Rounded up: a tick too many only leaves the device alone a little longer, while a tick too
	// few would touch it before its cycle is over.
	return (wait + period - 1) / period;
}

/**
 * @fn _clipStep
 * @brief Clips one step of a transfer so that it never crosses a page boundary and never has to
 *        be split by the port.
 * @param p_owner Pointer to the NVRAM instance.
 * @param pos Where in the media the step begins.
 * @param len Bytes that are left to move.
 * @return Bytes the step moves.
 */
static size_t _clipStep(PifNvram* p_owner, uint32_t pos, size_t len)
{
	size_t room;

	// As far as the end of the page this position falls in. A device that pages its writes wraps
	// within the page rather than carrying on into the next one, so a step that crossed the
	// boundary would put the tail of it back over the head.
	if (p_owner->__page_size) {
		room = p_owner->__page_size - pos % p_owner->__page_size;
		if (len > room) len = room;
	}
	// A port splits a longer transfer into pieces that follow one another at once, and on a
	// device with a write cycle every piece but the first would land inside the cycle of the one
	// before it. Taken as steps of their own, each waits for the cycle as a page does.
	if (p_owner->_p_device && p_owner->_p_device->max_transfer_size && len > p_owner->_p_device->max_transfer_size) {
		len = p_owner->_p_device->max_transfer_size;
	}
	return len;
}

/**
 * @fn _stepTransfer
 * @brief Moves one step of the transfer: the header of a record first, if there is one, then
 *        the data.
 * @param p_owner Pointer to the NVRAM instance.
 * @return TRUE when the step went through, otherwise FALSE.
 */
static BOOL _stepTransfer(PifNvram* p_owner)
{
	uint8_t* p_data;
	size_t len;

	if (p_owner->__op_head_remain) {
		p_data = p_owner->__op_head + (PIF_NVRAM_RECORD_HEADER - p_owner->__op_head_remain);
		len = _clipStep(p_owner, p_owner->__op_pos, p_owner->__op_head_remain);
	}
	else {
		p_data = p_owner->__op_buffer;
		len = _clipStep(p_owner, p_owner->__op_pos, p_owner->__op_remain);
	}

	if (p_owner->__op_write) {
		if (!(*p_owner->__act_write)(p_owner, p_owner->__op_pos, p_data, len)) return FALSE;
	}
	else {
		if (!(*p_owner->__act_read)(p_owner, p_data, p_owner->__op_pos, len)) return FALSE;
	}

	p_owner->__op_pos += len;
	if (p_owner->__op_head_remain) {
		p_owner->__op_head_remain -= len;
	}
	else {
		p_owner->__op_buffer += len;
		p_owner->__op_remain -= len;
	}
	return TRUE;
}

/**
 * @fn _isTransferLeft
 * @param p_owner Pointer to the NVRAM instance.
 * @return TRUE while some of the transfer has not been moved yet.
 */
static BOOL _isTransferLeft(PifNvram* p_owner)
{
	return p_owner->__op_head_remain || p_owner->__op_remain;
}

/**
 * @fn _endTransfer
 * @brief Notes what a transfer that has ended did to the record it was writing, if any.
 * @param p_owner Pointer to the NVRAM instance.
 * @param result TRUE when every step went through, FALSE when one of them failed.
 */
static void _endTransfer(PifNvram* p_owner, BOOL result)
{
	PifNvramRecord* p_record;

	if (p_owner->__op_record == OP_RECORD_NONE) return;

	p_record = &p_owner->__p_record[p_owner->__op_record];
	if (result) {
		p_record->seq = p_owner->__op_seq;
		p_record->slot = p_owner->__op_slot;
	}
	else {
		// The copy that was being written is broken now and the other one is as it was, which a
		// look at both of them finds out again.
		p_record->slot = RECORD_UNKNOWN;
	}
	p_owner->__op_record = OP_RECORD_NONE;
}

/**
 * @fn _finishTransfer
 * @brief Ends a transfer the timer was carrying and reports it.
 * @param p_owner Pointer to the NVRAM instance.
 * @param result TRUE when every step went through, FALSE when one of them failed.
 */
static void _finishTransfer(PifNvram* p_owner, BOOL result)
{
	// Left before the event so that the next transfer may be started from inside it.
	p_owner->_state = NS_IDLE;
	_endTransfer(p_owner, result);
	if (p_owner->__evt_done) (*p_owner->__evt_done)(p_owner, result);
}

/**
 * @fn _evtTimerFinish
 * @brief Moves the transfer on by one step once the media has been left alone long enough.
 * @param p_issuer Issuer pointer castable to PifNvram.
 */
static void _evtTimerFinish(PifIssuerP p_issuer)
{
	PifNvram* p_owner = (PifNvram*)p_issuer;

	// Reached from the timer process of the task manager, which runs at the start of a loop with
	// the CPU to itself, so the transfer below is in the same context as one inside any task.
	if (!_stepTransfer(p_owner)) {
		_finishTransfer(p_owner, FALSE);
		return;
	}
	if (!_isTransferLeft(p_owner)) {
		_finishTransfer(p_owner, TRUE);
		return;
	}
	if (!pifTimer_Start(p_owner->__p_timer, _chunkTicks(p_owner))) {
		_finishTransfer(p_owner, FALSE);
	}
}

/**
 * @fn _startTransfer
 * @brief Starts the transfer the op fields describe.
 *        The first step goes at once, because there is nothing to wait for before it. What is
 *        left is carried by the timer if there is one, and inside this call if there is not.
 * @param p_owner Pointer to the NVRAM instance.
 * @return NS_START_DONE, NS_START_TIMED or NS_START_FAILURE.
 */
static PifNvramStart _startTransfer(PifNvram* p_owner)
{
	// A transfer that fits in one page is over without the timer being involved at all. That is
	// the common case, a settings record small enough for a single page.
	if (!_stepTransfer(p_owner)) goto fail;
	if (!_isTransferLeft(p_owner)) goto done;

	if (!p_owner->__p_timer) {
		// Nothing to space the pages out with, so the rest is done here and now. Each write cycle
		// is paid for inside this call, which is what pifNvram_AttachTimer() removes.
		while (_isTransferLeft(p_owner)) {
			if (!_stepTransfer(p_owner)) goto fail;
		}
		goto done;
	}

	p_owner->_state = NS_TRANSFER;
	if (!pifTimer_Start(p_owner->__p_timer, _chunkTicks(p_owner))) {
		p_owner->_state = NS_IDLE;
		goto fail;
	}
	return NS_START_TIMED;

done:
	_endTransfer(p_owner, TRUE);
	return NS_START_DONE;

fail:
	_endTransfer(p_owner, FALSE);
	return NS_START_FAILURE;
}

/**
 * @fn _canAccess
 * @brief Checks what every access needs: the mode it is for, a slot or record in range, the
 *        callback it uses, and no transfer on its way.
 * @param p_owner Pointer to the NVRAM instance.
 * @param mode Mode the access is for.
 * @param id Slot number in NM_RAW mode, record number in NM_RECORD mode.
 * @param write TRUE when the access writes.
 * @return TRUE when the access may go ahead, otherwise FALSE with pif_error set.
 */
static BOOL _canAccess(PifNvram* p_owner, PifNvramMode mode, uint16_t id, BOOL write)
{
	uint16_t count;

	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!p_owner->_slot_count || p_owner->_mode != mode) {
		pif_error = E_CANNOT_USE;
		return FALSE;
	}
	count = mode == NM_RECORD ? p_owner->_slot_count / 2 : p_owner->_slot_count;
	if (id >= count) {
		pif_error = E_INVALID_ID;
		return FALSE;
	}
	// A record is looked at before it is written, so a write needs both.
	if (!p_owner->__act_read || (write && !p_owner->__act_write)) {
		pif_error = E_CANNOT_FOUND;
		return FALSE;
	}
	// There is one set of transfer fields, so the one on its way has to end before the next starts.
	if (p_owner->_state != NS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	return TRUE;
}

/**
 * @fn _slotSize
 * @brief Size of a slot in NM_RAW mode, or of each copy of a record in NM_RECORD mode.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Slot or record number.
 * @return Size in bytes.
 */
static uint32_t _slotSize(PifNvram* p_owner, uint16_t id)
{
	uint32_t size;

	if (!p_owner->__p_offset) return p_owner->_slot_size;

	// A record takes room for both of its copies.
	size = p_owner->__p_offset[id + 1] - p_owner->__p_offset[id];
	return p_owner->_mode == NM_RECORD ? size / 2 : size;
}

/**
 * @fn _slotPos
 * @brief Where a slot, or a copy of a record, begins in the media.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Slot or record number.
 * @param copy Which copy of a record, 0 or 1. 0 in NM_RAW mode.
 * @return Position in bytes.
 */
static uint32_t _slotPos(PifNvram* p_owner, uint16_t id, uint8_t copy)
{
	if (!p_owner->__p_offset) {
		return (uint32_t)(p_owner->_mode == NM_RECORD ? id * 2 + copy : id) * p_owner->_slot_size;
	}
	return p_owner->__p_offset[id] + copy * _slotSize(p_owner, id);
}

/**
 * @fn _setUp
 * @brief Takes the arrangement of the media worked out by pifNvram_SetMedia() or pifNvram_SetLayout().
 * @param p_owner Pointer to the NVRAM instance.
 * @param capacity Size of the media in bytes.
 * @param page_size Size of a write page, or 0.
 * @param slot_size Size of every slot, or 0 when p_offset gives them.
 * @param slot_count Number of slots, two per record in NM_RECORD mode.
 * @param p_offset Where each slot or record begins, and after the last one where it ends, or NULL. Taken over.
 * @param mode NM_RAW or NM_RECORD.
 * @return TRUE on success, otherwise FALSE with pif_error set, and p_offset freed.
 */
static BOOL _setUp(PifNvram* p_owner, uint32_t capacity, uint16_t page_size, uint32_t slot_size, uint16_t slot_count,
		uint32_t* p_offset, PifNvramMode mode)
{
	PifNvramRecord* p_record = NULL;
	uint16_t i;

	if (mode == NM_RECORD) {
		p_record = calloc(slot_count / 2, sizeof(PifNvramRecord));
		if (!p_record) {
			if (p_offset) free(p_offset);
			pif_error = E_OUT_OF_HEAP;
			return FALSE;
		}
		for (i = 0; i < slot_count / 2; i++) p_record[i].slot = RECORD_UNKNOWN;
	}

	if (p_owner->__p_record) free(p_owner->__p_record);
	if (p_owner->__p_offset) free(p_owner->__p_offset);
	p_owner->__p_record = p_record;
	p_owner->__p_offset = p_offset;
	p_owner->_capacity = capacity;
	p_owner->__page_size = page_size;
	p_owner->_slot_size = slot_size;
	p_owner->_slot_count = slot_count;
	p_owner->_mode = mode;
	return TRUE;
}

/**
 * @fn _writeNow
 * @brief Writes bytes in steps as a transfer does, all of them before it returns.
 * @param p_owner Pointer to the NVRAM instance.
 * @param pos Where in the media they go.
 * @param p_src The bytes.
 * @param size Number of bytes.
 * @return TRUE on success, otherwise FALSE.
 */
static BOOL _writeNow(PifNvram* p_owner, uint32_t pos, uint8_t* p_src, size_t size)
{
	size_t len;

	while (size) {
		len = _clipStep(p_owner, pos, size);
		if (!(*p_owner->__act_write)(p_owner, pos, p_src, len)) return FALSE;
		pos += len;
		p_src += len;
		size -= len;
	}
	return TRUE;
}

/**
 * @fn _checkCopy
 * @brief Reads one copy of a record and checks it against its CRC, a few bytes at a time, without
 *        putting it anywhere.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Record number.
 * @param copy Which copy, 0 or 1.
 * @param p_seq Where its sequence number goes when it is intact.
 * @param p_size Where its length goes when it is intact.
 * @return CC_INTACT, CC_BROKEN, or CC_ACCESS_FAILED when the media could not be read, which says
 *         nothing about the copy.
 */
static CopyCheck _checkCopy(PifNvram* p_owner, uint16_t id, uint8_t copy, uint16_t* p_seq, uint16_t* p_size)
{
	uint8_t buffer[16];
	uint32_t pos = _slotPos(p_owner, id, copy);
	uint16_t crc, stored_crc, size, remain, len;

	if (!(*p_owner->__act_read)(p_owner, buffer, pos, PIF_NVRAM_RECORD_HEADER)) return CC_ACCESS_FAILED;

	size = buffer[2] | (buffer[3] << 8);
	// Erased or never written media reads as 0xFF or 0x00 throughout. 0xFFFF is a length no slot
	// has room for, and a header of zeros fails the CRC, which starts from 0xFFFF for that reason.
	if (size > _slotSize(p_owner, id) - PIF_NVRAM_RECORD_HEADER) return CC_BROKEN;

	*p_seq = buffer[0] | (buffer[1] << 8);
	*p_size = size;
	stored_crc = buffer[4] | (buffer[5] << 8);
	crc = pifCrc16_Update(0xFFFF, buffer, 4);

	// The CRC goes over the data as well, read through the small buffer a piece at a time.
	pos += PIF_NVRAM_RECORD_HEADER;
	remain = size;
	while (remain) {
		len = remain > sizeof(buffer) ? sizeof(buffer) : remain;
		if (!(*p_owner->__act_read)(p_owner, buffer, pos, len)) return CC_ACCESS_FAILED;
		crc = pifCrc16_Update(crc, buffer, len);
		pos += len;
		remain -= len;
	}
	return crc == stored_crc ? CC_INTACT : CC_BROKEN;
}

/**
 * @fn _findRecord
 * @brief Looks at both copies of a record and notes which one is the latest intact copy.
 * @param p_owner Pointer to the NVRAM instance.
 * @param id Record number.
 * @param p_size Where the length of that copy goes, when there is one.
 * @return TRUE when both could be looked at, whatever was found, otherwise FALSE with pif_error set.
 */
static BOOL _findRecord(PifNvram* p_owner, uint16_t id, uint16_t* p_size)
{
	PifNvramRecord* p_record = &p_owner->__p_record[id];
	uint16_t seq[2], size[2];
	CopyCheck check[2];
	uint8_t i;

	for (i = 0; i < 2; i++) {
		check[i] = _checkCopy(p_owner, id, i, &seq[i], &size[i]);
		if (check[i] == CC_ACCESS_FAILED) {
			// Taken for broken, a copy that could not be read would let the next write go over the
			// latest one.
			pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}

	if (check[0] == CC_INTACT && check[1] == CC_INTACT) {
		// Read signed, so that the newer one is still told apart once the numbers wrap around.
		i = (int16_t)(seq[1] - seq[0]) > 0 ? 1 : 0;
	}
	else if (check[0] == CC_INTACT) i = 0;
	else if (check[1] == CC_INTACT) i = 1;
	else {
		p_record->slot = RECORD_NONE;
		return TRUE;
	}

	p_record->seq = seq[i];
	p_record->slot = i;
	*p_size = size[i];
	return TRUE;
}

/**
 * @fn _actNvramI2c_Read_1
 * @brief Reads bytes through I2C using a 1-byte internal address.
 * @param p_owner Pointer to the NVRAM instance.
 * @param p_dst Destination buffer.
 * @param src Source offset in the media.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actNvramI2c_Read_1(PifNvram* p_owner, uint8_t* p_dst, uint32_t src, size_t size)
{
	_waitWriteCycle(p_owner);
	((PifI2cDevice *)p_owner->_p_device)->addr = p_owner->__addr | (src >> 8);
	if (!pifDeviceReg_Read(p_owner->_p_device, src & 0xFF, 1, p_dst, size)) return FALSE;
	return TRUE;
}

/**
 * @fn _actNvramI2c_Write_1
 * @brief Writes bytes through I2C using a 1-byte internal address.
 * @param p_owner Pointer to the NVRAM instance.
 * @param dst Destination offset in the media.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actNvramI2c_Write_1(PifNvram* p_owner, uint32_t dst, uint8_t* p_src, size_t size)
{
	_waitWriteCycle(p_owner);
	((PifI2cDevice *)p_owner->_p_device)->addr = p_owner->__addr | (dst >> 8);
	if (!pifDeviceReg_Write(p_owner->_p_device, dst & 0xFF, 1, p_src, size)) return FALSE;
	_startWriteCycle(p_owner);
	return TRUE;
}

/**
 * @fn _actNvramI2c_Read_2
 * @brief Reads bytes through I2C using a 2-byte internal address.
 * @param p_owner Pointer to the NVRAM instance.
 * @param p_dst Destination buffer.
 * @param src Source offset in the media.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actNvramI2c_Read_2(PifNvram* p_owner, uint8_t* p_dst, uint32_t src, size_t size)
{
	_waitWriteCycle(p_owner);
	((PifI2cDevice *)p_owner->_p_device)->addr = p_owner->__addr | (src >> 16);
	if (!pifDeviceReg_Read(p_owner->_p_device, src & 0xFFFF, 2, p_dst, size)) return FALSE;
	return TRUE;
}

/**
 * @fn _actNvramI2c_Write_2
 * @brief Writes bytes through I2C using a 2-byte internal address.
 * @param p_owner Pointer to the NVRAM instance.
 * @param dst Destination offset in the media.
 * @param p_src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actNvramI2c_Write_2(PifNvram* p_owner, uint32_t dst, uint8_t* p_src, size_t size)
{
	_waitWriteCycle(p_owner);
	((PifI2cDevice *)p_owner->_p_device)->addr = p_owner->__addr | (dst >> 16);
	if (!pifDeviceReg_Write(p_owner->_p_device, dst & 0xFFFF, 2, p_src, size)) return FALSE;
	_startWriteCycle(p_owner);
	return TRUE;
}

BOOL pifNvram_Init(PifNvram* p_owner, PifId id)
{
    if (!p_owner) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	memset(p_owner, 0, sizeof(PifNvram));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    p_owner->__op_record = OP_RECORD_NONE;
	return TRUE;
}

void pifNvram_Clear(PifNvram* p_owner)
{
	if (!p_owner) return;

	pifNvram_DetachTimer(p_owner);
	pifNvram_DetachI2c(p_owner);
	if (p_owner->__p_record) {
		free(p_owner->__p_record);
		p_owner->__p_record = NULL;
	}
	if (p_owner->__p_offset) {
		free(p_owner->__p_offset);
		p_owner->__p_offset = NULL;
	}
	p_owner->_slot_count = 0;
}

BOOL pifNvram_SetMedia(PifNvram* p_owner, uint32_t capacity, uint16_t page_size, uint32_t slot_size, PifNvramMode mode)
{
	uint32_t count;

	if (!p_owner || !slot_size || capacity < slot_size
			|| (mode != NM_RAW && mode != NM_RECORD)
			|| (mode == NM_RECORD && slot_size <= PIF_NVRAM_RECORD_HEADER)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_state != NS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	count = capacity / slot_size;
	if (count > 0xFFFF) count = 0xFFFF;
	if (mode == NM_RECORD) {
		if (count < 2) {
			pif_error = E_INVALID_PARAM;
			return FALSE;
		}
		// A record is two slots, and a slot left over has no use.
		count &= ~1UL;
	}
	return _setUp(p_owner, capacity, page_size, slot_size, count, NULL, mode);
}

BOOL pifNvram_SetLayout(PifNvram* p_owner, uint32_t capacity, uint16_t page_size, const uint16_t* p_size, uint16_t count,
		PifNvramMode mode)
{
	uint32_t* p_offset;
	uint32_t pos, slot;
	uint16_t i;

	if (!p_owner || !p_size || !count || (mode != NM_RAW && mode != NM_RECORD)
			|| (mode == NM_RECORD && count > 0x7FFF)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->_state != NS_IDLE) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_offset = malloc((count + 1) * sizeof(uint32_t));
	if (!p_offset) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}

	// One after the other from the start of the media, a record with room for its header in both copies.
	pos = 0;
	for (i = 0; i < count; i++) {
		p_offset[i] = pos;
		slot = p_size[i];
		if (mode == NM_RECORD) slot = 2 * (slot + PIF_NVRAM_RECORD_HEADER);
		if (!slot || slot > capacity - pos) {
			free(p_offset);
			pif_error = slot ? E_OVERFLOW_BUFFER : E_INVALID_PARAM;
			return FALSE;
		}
		pos += slot;
	}
	p_offset[count] = pos;

	return _setUp(p_owner, capacity, page_size, 0, mode == NM_RECORD ? count * 2 : count, p_offset, mode);
}

BOOL pifNvram_AttachActMedia(PifNvram* p_owner, PifActNvramRead act_read, PifActNvramWrite act_write)
{
    if (!p_owner || !act_read || !act_write) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;
	return TRUE;
}

BOOL pifNvram_AttachI2c(PifNvram* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifNvramI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
	PifI2cDevice *p_device;

    if (!p_owner || !p_port || (i_addr_size != NIC_I_ADDR_SIZE_1 && i_addr_size != NIC_I_ADDR_SIZE_2)) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_device = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr, p_client);
    if (!p_device) return FALSE;
	p_owner->_p_device = &p_device->parent;

    p_owner->__addr = addr;
    p_owner->__write_delay_ms = write_delay_ms;

    if (i_addr_size == NIC_I_ADDR_SIZE_1) {
		p_owner->__act_read = _actNvramI2c_Read_1;
		p_owner->__act_write = _actNvramI2c_Write_1;
    }
    else {
		p_owner->__act_read = _actNvramI2c_Read_2;
		p_owner->__act_write = _actNvramI2c_Write_2;
    }
	return TRUE;
}

void pifNvram_DetachI2c(PifNvram* p_owner)
{
	if (p_owner->_p_device) {
		PifI2cDevice *p_device = (PifI2cDevice *)p_owner->_p_device;

		pifI2cPort_RemoveDevice(p_device->_p_port, p_device);
    	p_owner->_p_device = NULL;
    	p_owner->__act_read = NULL;
    	p_owner->__act_write = NULL;
	}
	// Nothing is left to keep off a device that is no longer attached.
	p_owner->__write_pending = FALSE;
}

BOOL pifNvram_AttachTimer(PifNvram* p_owner, PifTimerManager* p_timer_manager, PifEvtNvramDone evt_done)
{
	if (!p_owner || !p_timer_manager) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (p_owner->__p_timer) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_owner->__p_timer = pifTimerManager_Add(p_timer_manager, TT_ONCE);
	if (!p_owner->__p_timer) return FALSE;

	// Not event_into_int: that callback is called from inside pifTimerManager_sigTick(),
	// and a page of the media is a transfer that has no business in a tick interrupt.
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner, FALSE);

	p_owner->__p_timer_manager = p_timer_manager;
	p_owner->__evt_done = evt_done;
	p_owner->_state = NS_IDLE;
	return TRUE;
}

void pifNvram_DetachTimer(PifNvram* p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
	if (p_owner->_state != NS_IDLE) _endTransfer(p_owner, FALSE);
	p_owner->__p_timer_manager = NULL;
	p_owner->_state = NS_IDLE;
}

PifNvramStart pifNvram_Read(PifNvram* p_owner, uint16_t id, uint8_t* p_dst, size_t size)
{
	if (!_canAccess(p_owner, NM_RAW, id, FALSE)) return NS_START_FAILURE;
	if (!p_dst || !size || size > _slotSize(p_owner, id)) {
		pif_error = E_INVALID_PARAM;
		return NS_START_FAILURE;
	}

	p_owner->__op_write = FALSE;
	p_owner->__op_pos = _slotPos(p_owner, id, 0);
	p_owner->__op_buffer = p_dst;
	p_owner->__op_remain = size;
	p_owner->__op_head_remain = 0;
	return _startTransfer(p_owner);
}

PifNvramStart pifNvram_Write(PifNvram* p_owner, uint16_t id, uint8_t* p_src, size_t size)
{
	if (!_canAccess(p_owner, NM_RAW, id, TRUE)) return NS_START_FAILURE;
	if (!p_src || !size || size > _slotSize(p_owner, id)) {
		pif_error = E_INVALID_PARAM;
		return NS_START_FAILURE;
	}

	p_owner->__op_write = TRUE;
	p_owner->__op_pos = _slotPos(p_owner, id, 0);
	p_owner->__op_buffer = p_src;
	p_owner->__op_remain = size;
	p_owner->__op_head_remain = 0;
	return _startTransfer(p_owner);
}

BOOL pifNvram_ReadRecord(PifNvram* p_owner, uint16_t id, uint8_t* p_dst, size_t size, uint16_t* p_size)
{
	PifNvramRecord* p_record;
	uint16_t seq, length = 0;

	if (!_canAccess(p_owner, NM_RECORD, id, FALSE)) return FALSE;
	if (!p_dst && size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	p_record = &p_owner->__p_record[id];
	if (p_record->slot < 2) {
		// Known from before, but checked again: what is in the media may have changed since.
		switch (_checkCopy(p_owner, id, p_record->slot, &seq, &length)) {
		case CC_INTACT:
			if (seq == p_record->seq) break;
			// fall through
		case CC_BROKEN:
			p_record->slot = RECORD_UNKNOWN;
			break;
		default:
			pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}
	if (p_record->slot == RECORD_UNKNOWN) {
		if (!_findRecord(p_owner, id, &length)) return FALSE;
	}
	if (p_record->slot == RECORD_NONE) {
		pif_error = E_CANNOT_FOUND;
		return FALSE;
	}

	if (size > length) size = length;
	if (size) {
		if (!(*p_owner->__act_read)(p_owner, p_dst, _slotPos(p_owner, id, p_record->slot) + PIF_NVRAM_RECORD_HEADER, size)) {
			pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}
	if (p_size) *p_size = length;
	return TRUE;
}

PifNvramStart pifNvram_WriteRecord(PifNvram* p_owner, uint16_t id, uint8_t* p_src, size_t size)
{
	PifNvramRecord* p_record;
	uint16_t length, crc;
	uint8_t* p_head;

	if (!_canAccess(p_owner, NM_RECORD, id, TRUE)) return NS_START_FAILURE;
	if ((!p_src && size) || size > _slotSize(p_owner, id) - PIF_NVRAM_RECORD_HEADER || size > 0xFFFF) {
		pif_error = E_INVALID_PARAM;
		return NS_START_FAILURE;
	}

	p_record = &p_owner->__p_record[id];
	if (p_record->slot == RECORD_UNKNOWN) {
		if (!_findRecord(p_owner, id, &length)) return NS_START_FAILURE;
	}

	// Over the copy that is not the latest, one step on in the sequence.
	if (p_record->slot < 2) {
		p_owner->__op_slot = p_record->slot ^ 1;
		p_owner->__op_seq = p_record->seq + 1;
	}
	else {
		p_owner->__op_slot = 0;
		p_owner->__op_seq = 0;
	}

	p_head = p_owner->__op_head;
	p_head[0] = p_owner->__op_seq & 0xFF;
	p_head[1] = p_owner->__op_seq >> 8;
	p_head[2] = size & 0xFF;
	p_head[3] = size >> 8;
	crc = pifCrc16_Update(0xFFFF, p_head, 4);
	if (size) crc = pifCrc16_Update(crc, p_src, size);
	p_head[4] = crc & 0xFF;
	p_head[5] = crc >> 8;

	p_owner->__op_write = TRUE;
	p_owner->__op_pos = _slotPos(p_owner, id, p_owner->__op_slot);
	p_owner->__op_head_remain = PIF_NVRAM_RECORD_HEADER;
	p_owner->__op_buffer = p_src;
	p_owner->__op_remain = size;
	p_owner->__op_record = id;
	return _startTransfer(p_owner);
}

BOOL pifNvram_DeleteRecord(PifNvram* p_owner, uint16_t id)
{
	uint8_t head[PIF_NVRAM_RECORD_HEADER];
	uint8_t i;

	if (!_canAccess(p_owner, NM_RECORD, id, TRUE)) return FALSE;

	// A length of 0xFFFF is more than any slot holds, which no intact copy has.
	memset(head, 0xFF, sizeof(head));
	for (i = 0; i < 2; i++) {
		if (!_writeNow(p_owner, _slotPos(p_owner, id, i), head, sizeof(head))) {
			p_owner->__p_record[id].slot = RECORD_UNKNOWN;
			pif_error = E_ACCESS_FAILED;
			return FALSE;
		}
	}
	p_owner->__p_record[id].slot = RECORD_NONE;
	return TRUE;
}
