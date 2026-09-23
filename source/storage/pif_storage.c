#include "core/pif_task_manager.h"
#include "storage/pif_storage.h"


/**
 * @fn _startWriteCycle
 * @brief Notes when the write cycle that has just begun will be over. Nothing is waited for here:
 *        the data is already in the device, and the cycle only has to have passed by the time the
 *        device is touched again.
 * @param p_owner Pointer to the storage instance.
 */
static void _startWriteCycle(PifStorage* p_owner)
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
 * @param p_owner Pointer to the storage instance.
 */
static void _waitWriteCycle(PifStorage* p_owner)
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
 * @brief How long to leave the media before the next sector of the transfer, in ticks of the
 *        timer manager.
 * @param p_owner Pointer to the storage instance.
 * @return Ticks to wait, at least one.
 */
static uint32_t _chunkTicks(PifStorage* p_owner)
{
	uint32_t period = p_owner->__p_timer_manager->_period1us;
	uint32_t wait;

	// A write has to leave the device alone for its cycle, which is what the timer is really for.
	// A read has nothing to wait for and asks for the shortest tick there is, only so that the
	// sectors land in separate releases instead of all in this one.
	wait = p_owner->__op_write ? p_owner->__write_delay_ms * 1000UL : 0UL;
	if (!wait) return 1UL;

	// Rounded up: a tick too many only leaves the device alone a little longer, while a tick too
	// few would touch it before its cycle is over.
	return (wait + period - 1) / period;
}

/**
 * @fn _stepTransfer
 * @brief Moves one sector of the transfer, never crossing a sector boundary.
 * @param p_owner Pointer to the storage instance.
 * @return TRUE when the sector went through, otherwise FALSE.
 */
static BOOL _stepTransfer(PifStorage* p_owner)
{
	// As far as the end of the sector this position falls in. A device that pages its writes wraps
	// within the page rather than carrying on into the next one, so a step that crossed the
	// boundary would put the tail of it back over the head.
	size_t len = p_owner->__op_sector_size - p_owner->__op_pos % p_owner->__op_sector_size;

	if (len > p_owner->__op_remain) len = p_owner->__op_remain;

	if (p_owner->__op_write) {
		if (!(*p_owner->__act_write)(p_owner, p_owner->__op_pos, p_owner->__op_buffer, len)) return FALSE;
	}
	else {
		if (!(*p_owner->__act_read)(p_owner, p_owner->__op_buffer, p_owner->__op_pos, len)) return FALSE;
	}

	p_owner->__op_pos += len;
	p_owner->__op_buffer += len;
	p_owner->__op_remain -= len;
	return TRUE;
}

/**
 * @fn _finishTransfer
 * @brief Ends a transfer the timer was carrying and reports it.
 * @param p_owner Pointer to the storage instance.
 * @param result TRUE when every sector went through, FALSE when one of them failed.
 */
static void _finishTransfer(PifStorage* p_owner, BOOL result)
{
	// Left before the event so that the next transfer may be started from inside it.
	p_owner->_state = SS_IDLE;
	if (p_owner->__evt_done) (*p_owner->__evt_done)(p_owner, result);
}

/**
 * @fn _evtTimerFinish
 * @brief Moves the transfer on by one sector once the media has been left alone long enough.
 * @param p_issuer Issuer pointer castable to PifStorage.
 */
static void _evtTimerFinish(PifIssuerP p_issuer)
{
	PifStorage* p_owner = (PifStorage*)p_issuer;

	// Reached from the timer process of the task manager, which runs at the start of a loop with
	// the CPU to itself, so the transfer below is in the same context as one inside any task.
	if (!_stepTransfer(p_owner)) {
		_finishTransfer(p_owner, FALSE);
		return;
	}
	if (!p_owner->__op_remain) {
		_finishTransfer(p_owner, TRUE);
		return;
	}
	if (!pifTimer_Start(p_owner->__p_timer, _chunkTicks(p_owner))) {
		_finishTransfer(p_owner, FALSE);
	}
}

/**
 * @fn _actStorageI2c_Read_1
 * @brief Reads bytes through I2C using a 1-byte internal address.
 * @param p_owner Pointer to the storage instance.
 * @param dst Destination buffer.
 * @param src Source offset in storage media.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actStorageI2c_Read_1(PifStorage* p_owner, uint8_t* dst, uint32_t src, size_t size)
{
	_waitWriteCycle(p_owner);
	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 8);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFF, 1, dst, size)) return FALSE;
	return TRUE;
}

/**
 * @fn _actStorageI2c_Write_1
 * @brief Writes bytes through I2C using a 1-byte internal address.
 * @param p_owner Pointer to the storage instance.
 * @param dst Destination offset in storage media.
 * @param src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actStorageI2c_Write_1(PifStorage* p_owner, uint32_t dst, uint8_t* src, size_t size)
{
	_waitWriteCycle(p_owner);
	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 8);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFF, 1, src, size)) return FALSE;
	_startWriteCycle(p_owner);
	return TRUE;
}

/**
 * @fn _actStorageI2c_Read_2
 * @brief Reads bytes through I2C using a 2-byte internal address.
 * @param p_owner Pointer to the storage instance.
 * @param dst Destination buffer.
 * @param src Source offset in storage media.
 * @param size Number of bytes to read.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actStorageI2c_Read_2(PifStorage* p_owner, uint8_t* dst, uint32_t src, size_t size)
{
	_waitWriteCycle(p_owner);
	p_owner->_p_i2c->addr = p_owner->__addr | (src >> 16);
	if (!pifI2cDevice_Read(p_owner->_p_i2c, src & 0xFFFF, 2, dst, size)) return FALSE;
	return TRUE;
}

/**
 * @fn _actStorageI2c_Write_2
 * @brief Writes bytes through I2C using a 2-byte internal address.
 * @param p_owner Pointer to the storage instance.
 * @param dst Destination offset in storage media.
 * @param src Source buffer.
 * @param size Number of bytes to write.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
static BOOL _actStorageI2c_Write_2(PifStorage* p_owner, uint32_t dst, uint8_t* src, size_t size)
{
	_waitWriteCycle(p_owner);
	p_owner->_p_i2c->addr = p_owner->__addr | (dst >> 16);
	if (!pifI2cDevice_Write(p_owner->_p_i2c, dst & 0xFFFF, 2, src, size)) return FALSE;
	_startWriteCycle(p_owner);
	return TRUE;
}

BOOL pifStorage_AttachActStorage(PifStorage* p_owner, PifActStorageRead act_read, PifActStorageWrite act_write)
{
    if (!p_owner || !act_read || !act_write) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->__act_read = act_read;
	p_owner->__act_write = act_write;
	return TRUE;
}

BOOL pifStorage_AttachI2c(PifStorage* p_owner, PifI2cPort* p_port, uint8_t addr, void *p_client, PifStorageI2cIAddrSize i_addr_size, uint8_t write_delay_ms)
{
    if (!p_owner || !p_port) {
    	pif_error = E_INVALID_PARAM;
	    return FALSE;
    }

	p_owner->_p_i2c = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr, p_client);
    if (!p_owner->_p_i2c) return FALSE;

    p_owner->__addr = addr;
    p_owner->__write_delay_ms = write_delay_ms;

    switch (i_addr_size) {
    case SIC_I_ADDR_SIZE_1:
		p_owner->__act_read = _actStorageI2c_Read_1;
		p_owner->__act_write = _actStorageI2c_Write_1;
    	break;

    case SIC_I_ADDR_SIZE_2:
		p_owner->__act_read = _actStorageI2c_Read_2;
		p_owner->__act_write = _actStorageI2c_Write_2;
    	break;

    default:
    	goto fail;
    }
	return TRUE;

fail:
	pifStorage_DetachI2c(p_owner);
	return FALSE;
}

void pifStorage_DetachI2c(PifStorage* p_owner)
{
	if (p_owner->_p_i2c) {
		pifI2cPort_RemoveDevice(p_owner->_p_i2c->_p_port, p_owner->_p_i2c);
    	p_owner->_p_i2c = NULL;
	}
	p_owner->__act_read = NULL;
	p_owner->__act_write = NULL;
	// Nothing is left to keep off a device that is no longer attached.
	p_owner->__write_pending = FALSE;
}

BOOL pifStorage_AttachTimer(PifStorage* p_owner, PifTimerManager* p_timer_manager, PifEvtStorageDone evt_done)
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

	// Not pifTimer_AttachEvtIntFinish(): that one is called from inside pifTimerManager_sigTick(),
	// and a sector of the media is a transfer that has no business in a tick interrupt.
	pifTimer_AttachEvtFinish(p_owner->__p_timer, _evtTimerFinish, p_owner);

	p_owner->__p_timer_manager = p_timer_manager;
	p_owner->__evt_done = evt_done;
	p_owner->_state = SS_IDLE;
	return TRUE;
}

void pifStorage_DetachTimer(PifStorage* p_owner)
{
	if (p_owner->__p_timer) {
		pifTimerManager_Remove(p_owner->__p_timer);
		p_owner->__p_timer = NULL;
	}
	p_owner->__p_timer_manager = NULL;
	p_owner->_state = SS_IDLE;
}

PifStorageStart pifStorage_StartTransfer(PifStorage* p_owner, BOOL write, uint32_t pos, uint8_t* p_buffer, size_t size, uint16_t sector_size)
{
	if (!p_buffer || !size || !sector_size) {
		pif_error = E_INVALID_PARAM;
		return SS_START_FAILURE;
	}
	if (!(write ? p_owner->__act_write != NULL : p_owner->__act_read != NULL)) {
		pif_error = E_CANNOT_FOUND;
		return SS_START_FAILURE;
	}
	// There is one set of transfer fields, so the one on its way has to end before the next starts.
	if (p_owner->_state != SS_IDLE) {
		pif_error = E_INVALID_STATE;
		return SS_START_FAILURE;
	}

	p_owner->__op_write = write;
	p_owner->__op_pos = pos;
	p_owner->__op_buffer = p_buffer;
	p_owner->__op_remain = size;
	p_owner->__op_sector_size = sector_size;

	// The first sector goes now: nothing has to be left before it, and a transfer that fits in one
	// sector is over without the timer being involved at all. That is the common case, a settings
	// record small enough for a single sector.
	if (!_stepTransfer(p_owner)) return SS_START_FAILURE;
	if (!p_owner->__op_remain) return SS_START_DONE;

	if (!p_owner->__p_timer) {
		// Nothing to space the sectors out with, so the rest is done here and now. Each write
		// cycle is paid for inside this call, which is what pifStorage_AttachTimer() removes.
		while (p_owner->__op_remain) {
			if (!_stepTransfer(p_owner)) return SS_START_FAILURE;
		}
		return SS_START_DONE;
	}

	p_owner->_state = SS_TRANSFER;
	if (!pifTimer_Start(p_owner->__p_timer, _chunkTicks(p_owner))) {
		p_owner->_state = SS_IDLE;
		return SS_START_FAILURE;
	}
	return SS_START_TIMED;
}

PIF_INLINE BOOL pifStorage_IsFormat(PifStorage* p_owner)
{
	return (*p_owner->__fn_is_format)(p_owner);
}

PIF_INLINE BOOL pifStorage_Format(PifStorage* p_owner)
{
	return (*p_owner->__fn_format)(p_owner);
}

PIF_INLINE PifStorageDataInfoP pifStorage_Create(PifStorage* p_owner, uint16_t id, uint16_t size)
{
	return (*p_owner->__fn_create)(p_owner, id, size);
}

PIF_INLINE BOOL pifStorage_Delete(PifStorage* p_owner, uint16_t id)
{
	return (*p_owner->__fn_delete)(p_owner, id);
}

PIF_INLINE PifStorageDataInfoP pifStorage_Open(PifStorage* p_owner, uint16_t id)
{
	return (*p_owner->__fn_open)(p_owner, id);
}

PIF_INLINE PifStorageStart pifStorage_Read(PifStorage* p_owner, uint8_t* p_dst, PifStorageDataInfoP p_src, size_t size)
{
	return (*p_owner->__fn_read)(p_owner, p_dst, p_src, size);
}

PIF_INLINE PifStorageStart pifStorage_Write(PifStorage* p_owner, PifStorageDataInfoP p_dst, uint8_t* p_src, size_t size)
{
	return (*p_owner->__fn_write)(p_owner, p_dst, p_src, size);
}
