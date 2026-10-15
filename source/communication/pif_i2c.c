// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_i2c.h"
#ifndef PIF_NO_LOG
	#include "core/pif_log.h"
#endif


/**
 * @fn _setState
 * @brief Sets the state of a device and records it on its port for pifCollectSignal.
 * @param p_device Pointer to the device.
 * @param state New state.
 */
static void _setState(PifI2cDevice* p_device, PifDeviceRegState state)
{
	p_device->parent._state = state;
#ifdef COLLECT_SIGNAL_I2C
	if (state == DRS_RUN) pifCollectSignal_Put(&p_device->_p_port->__cs[I2_CSF_ADDR_IDX], p_device->addr);
	pifCollectSignal_Put(&p_device->_p_port->__cs[I2_CSF_STATE_IDX], state);
#endif
}

/**
 * @fn _pollTransfer
 * @brief Asks a port without a completion interrupt whether the running transfer is over.
 * @param p_device Pointer to the device whose transfer is running.
 */
static void _pollTransfer(PifI2cDevice* p_device)
{
	PifI2cPort* p_port = p_device->_p_port;

	if (!p_port->act_check || p_device->parent._state != DRS_RUN) return;

	switch ((*p_port->act_check)(p_device)) {
	case IR_WAIT:
		break;

	case IR_COMPLETE:
		_setState(p_device, DRS_COMPLETE);
		break;

	case IR_ERROR:
		_setState(p_device, DRS_ERROR);
		break;
	}
}

/**
 * @fn _timeoutTransfer
 * @brief Gives up on a transfer that ran past the device timeout and lets the port bring the bus
 *        back, since the transfer may still be holding it.
 * @param p_device Pointer to the device whose transfer timed out.
 */
static void _timeoutTransfer(PifI2cDevice* p_device)
{
	PifI2cPort* p_port = p_device->_p_port;

	pif_error = E_TIMEOUT;
	if (p_port->act_recover) (*p_port->act_recover)(p_device);
}

/**
 * @fn _transfer
 * @brief Runs a blocking read or write in pieces of max_transfer_size, each with its own internal
 *        address, and waits for each piece.
 * @return TRUE on success, otherwise FALSE with pif_error set.
 */
static BOOL _transfer(PifI2cDevice* p_device, BOOL write, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifI2cPort* p_port = p_device->_p_port;
	PifActI2cRead act = write ? p_port->act_write : p_port->act_read;
	size_t len, ptr;
	uint32_t timer1ms;
#ifndef PIF_NO_LOG
	int line;
#endif

	if (!act) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	// Blocking transfers give the port back before they return, and no task runs while one is
	// waiting, so a port that is taken here is held by a transfer from pifDeviceReg_StartRead().
	if (p_port->__use_device) {
		pif_error = E_INVALID_STATE;
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "I2C%c:%u Addr:%Xh Use Addr:%Xh", write ? 'W' : 'R', __LINE__, p_device->addr,
				p_port->__use_device->addr);
#endif
		return FALSE;
	}

	p_port->__use_device = p_device;
	ptr = 0;
	// A write runs at least once, so a write with no data still sends the internal address alone (a
	// command), or only the slave address when isize is 0 too.
	do {
		if (!size && !write) break;
		len = (p_device->parent.max_transfer_size && size > p_device->parent.max_transfer_size) ?
				p_device->parent.max_transfer_size : size;
		// Each piece runs on its own: the one before it left DRS_COMPLETE behind, which would end
		// the wait for this one at once.
		_setState(p_device, DRS_RUN);
		switch ((*act)(p_device, iaddr + ptr, isize, p_data + ptr, len)) {
		case IR_WAIT:
			timer1ms = pif_cumulative_timer1ms;
			while (p_device->parent._state == DRS_RUN) {
				_pollTransfer(p_device);
				if (p_device->parent._state != DRS_RUN) break;
				if (pif_cumulative_timer1ms - timer1ms > p_device->parent.timeout) {
					_timeoutTransfer(p_device);
#ifndef PIF_NO_LOG
					line = __LINE__;
#endif
					goto fail;
				}
			}
			if (p_device->parent._state == DRS_ERROR) {
#ifndef PIF_NO_LOG
				line = __LINE__;
#endif
				goto fail;
			}
			break;

		case IR_COMPLETE:
			break;

		case IR_ERROR:
#ifndef PIF_NO_LOG
			line = __LINE__;
#endif
			goto fail;
		}
		ptr += len;
		size -= len;
	} while (size);
	p_port->__use_device = NULL;
	_setState(p_device, DRS_IDLE);
	return TRUE;

fail:
#ifndef PIF_NO_LOG
	pifLog_Printf(LT_ERROR, "I2C%c:%u A:%Xh R:%Xh E:%u", write ? 'W' : 'R', line, p_device->addr, iaddr, pif_error);
#endif
	p_port->__use_device = NULL;
	p_port->error_count++;
	_setState(p_device, DRS_IDLE);
	return FALSE;
}

static BOOL _read(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	return _transfer((PifI2cDevice*)p_owner, FALSE, iaddr, isize, p_data, size);
}

static BOOL _write(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	return _transfer((PifI2cDevice*)p_owner, TRUE, iaddr, isize, p_data, size);
}

static BOOL _startRead(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifI2cDevice* p_device = (PifI2cDevice*)p_owner;
	PifI2cPort* p_port = p_device->_p_port;

	if (!p_port->act_read || !size || (p_owner->max_transfer_size && size > p_owner->max_transfer_size)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	if (p_port->__use_device) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	p_port->__use_device = p_device;
	_setState(p_device, DRS_RUN);
	p_owner->__start_time1ms = pif_cumulative_timer1ms;
	switch ((*p_port->act_read)(p_device, iaddr, isize, p_data, size)) {
	case IR_WAIT:
		break;

	case IR_COMPLETE:
		_setState(p_device, DRS_COMPLETE);
		break;

	case IR_ERROR:
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "I2CR:%u A:%Xh R:%Xh E:%u", __LINE__, p_device->addr, iaddr, pif_error);
#endif
		p_port->__use_device = NULL;
		p_port->error_count++;
		_setState(p_device, DRS_IDLE);
		return FALSE;
	}
	return TRUE;
}

static PifDeviceRegState _checkTransfer(PifDeviceReg* p_owner)
{
	PifI2cDevice* p_device = (PifI2cDevice*)p_owner;
	PifI2cPort* p_port = p_device->_p_port;
	PifDeviceRegState state;

	if (p_port->__use_device != p_device) return DRS_IDLE;

	_pollTransfer(p_device);
	state = p_owner->_state;
	if (state == DRS_RUN) {
		if (pif_cumulative_timer1ms - p_owner->__start_time1ms <= p_owner->timeout) return DRS_RUN;
		_timeoutTransfer(p_device);
		state = DRS_ERROR;
	}

	if (state == DRS_ERROR) {
#ifndef PIF_NO_LOG
		pifLog_Printf(LT_ERROR, "I2CC:%u A:%Xh E:%u", __LINE__, p_device->addr, pif_error);
#endif
		p_port->error_count++;
	}
	p_port->__use_device = NULL;
	_setState(p_device, DRS_IDLE);
	return state;
}

static BOOL _isBusy(PifDeviceReg* p_owner)
{
	return ((PifI2cDevice*)p_owner)->_p_port->__use_device != NULL;
}

static const PifDeviceRegOps c_device_ops = {
	_read,
	_write,
	_startRead,
	_checkTransfer,
	_isBusy
};


BOOL pifI2cPort_Init(PifI2cPort *p_owner, PifId id, uint8_t device_count)
{
	if (!p_owner || !device_count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifI2cPort));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    if (!pifObjArray_Init(&p_owner->__devices, sizeof(PifI2cDevice), device_count, NULL)) goto fail;
    return TRUE;

fail:
	pifI2cPort_Clear(p_owner);
	return FALSE;
}

void pifI2cPort_Clear(PifI2cPort* p_owner)
{
#ifdef COLLECT_SIGNAL_I2C
	pifI2cPort_ResetCsFlag(p_owner, I2_CSF_ALL_BIT);
#endif
	pifObjArray_Clear(&p_owner->__devices);
}

PifI2cDevice* pifI2cPort_AddDevice(PifI2cPort* p_owner, PifId id, uint8_t addr, void *p_client)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	PifObjArrayIterator it = pifObjArray_Add(&p_owner->__devices);
    if (!it) return NULL;

    PifI2cDevice* p_device = (PifI2cDevice*)it->data;
    pifDeviceReg_Init(&p_device->parent, id, p_client, &c_device_ops);
    p_device->_p_port = p_owner;
	p_device->addr = addr;
    return p_device;
}

BOOL pifI2cPort_RemoveDevice(PifI2cPort* p_owner, PifI2cDevice* p_device)
{
	if (!p_device) return TRUE;

	// A transfer still running may write into its buffer from an interrupt, and the port would be
	// left pointing at the removed device.
	if (p_owner->__use_device == p_device) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}

	pifObjArray_Remove(&p_owner->__devices, p_device);
	return TRUE;
}

PifI2cDevice* pifI2cPort_TemporaryDevice(PifI2cPort* p_owner, uint8_t addr, void *p_client)
{
	static PifI2cDevice device;

	// Every call hands out the same device, so it cannot be cleared under a transfer it started.
	if (p_owner->__use_device == &device) {
		pif_error = E_INVALID_STATE;
		return NULL;
	}

	memset(&device, 0, sizeof(PifI2cDevice));
	pifDeviceReg_Init(&device.parent, 0, p_client, &c_device_ops);
	device._p_port = p_owner;
	device.addr = addr;
	return &device;
}

void pifI2cPort_sigEndTransfer(PifI2cPort* p_owner, BOOL result)
{
	PifI2cDevice* p_device = (PifI2cDevice*)p_owner->__use_device;

	// Only a transfer still running is ended, so a second signal for one does not overwrite how
	// it ended.
	if (!p_device || p_device->parent._state != DRS_RUN) return;

	_setState(p_device, result ? DRS_COMPLETE : DRS_ERROR);
}

#ifdef COLLECT_SIGNAL_I2C

BOOL pifI2cPort_SetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag)
{
	if (flag & I2_CSF_STATE_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[I2_CSF_STATE_IDX], "I2S", p_owner->_id, CSVT_REG, 2,
				DRS_IDLE)) return FALSE;
	}
	if (flag & I2_CSF_ADDR_BIT) {
		if (!pifCollectSignal_AddChannel(&p_owner->__cs[I2_CSF_ADDR_IDX], "I2A", p_owner->_id, CSVT_REG, 7,
				0)) return FALSE;
	}
	return TRUE;
}

void pifI2cPort_ResetCsFlag(PifI2cPort* p_owner, PifI2cCsFlag flag)
{
	if (flag & I2_CSF_STATE_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[I2_CSF_STATE_IDX]);
	if (flag & I2_CSF_ADDR_BIT) pifCollectSignal_RemoveChannel(&p_owner->__cs[I2_CSF_ADDR_IDX]);
}

#endif	// COLLECT_SIGNAL_I2C
