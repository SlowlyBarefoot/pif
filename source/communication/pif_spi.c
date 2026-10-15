// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_spi.h"


/**
 * @brief Takes the port for a transfer of the device.
 * @return FALSE with pif_error set to E_INVALID_STATE while a background transfer holds it.
 */
static BOOL _takePort(PifSpiDevice* p_device)
{
	PifSpiPort* p_port = p_device->_p_port;

	if (p_port->__use_device) {
		pif_error = E_INVALID_STATE;
		return FALSE;
	}
	p_port->__use_device = p_device;
	return TRUE;
}

/**
 * @brief Marks a background transfer of the device as running and the port as held until
 *        pifDeviceReg_CheckTransfer() reports it over.
 */
static void _beginBackground(PifSpiDevice* p_device)
{
	p_device->parent._state = DRS_RUN;
	p_device->parent.__start_time1ms = pif_cumulative_timer1ms;
}

static void _failBackground(PifSpiDevice* p_device)
{
	p_device->_p_port->__use_device = NULL;
	p_device->_p_port->error_count++;
	p_device->parent._state = DRS_IDLE;
	pif_error = E_TRANSFER_FAILED;
}

/**
 * @brief Runs a blocking read or write in pieces of max_transfer_size, each a transfer of its own
 *        with its own internal address.
 * @return TRUE on success, otherwise FALSE with pif_error set.
 */
static BOOL _transfer(PifSpiDevice* p_device, BOOL write, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiPort* p_port = p_device->_p_port;
	PifActSpiRead act = write ? p_port->act_write : p_port->act_read;
	uint16_t max_size = p_device->parent.max_transfer_size;
	size_t len, ptr;

	if (!act) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_takePort(p_device)) return FALSE;

	// Each piece sends its own internal address: a port ends the chip select with every call, so
	// a piece without an address would start the device over somewhere else.
	ptr = 0;
	while (size) {
		len = (max_size && size > max_size) ? max_size : size;
		if (!(*act)(p_device, iaddr + ptr, isize, p_data + ptr, len)) {
			p_port->__use_device = NULL;
			p_port->error_count++;
			pif_error = E_TRANSFER_FAILED;
			return FALSE;
		}
		ptr += len;
		size -= len;
	}
	p_port->__use_device = NULL;
	return TRUE;
}

static BOOL _read(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	return _transfer((PifSpiDevice*)p_owner, FALSE, iaddr, isize, p_data, size);
}

static BOOL _write(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	return _transfer((PifSpiDevice*)p_owner, TRUE, iaddr, isize, p_data, size);
}

static BOOL _startRead(PifDeviceReg* p_owner, uint32_t iaddr, uint8_t isize, uint8_t* p_data, size_t size)
{
	PifSpiDevice* p_device = (PifSpiDevice*)p_owner;
	PifSpiPort* p_port = p_device->_p_port;

	if ((!p_port->act_start_read && !p_port->act_read) || !size
			|| (p_owner->max_transfer_size && size > p_owner->max_transfer_size)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_takePort(p_device)) return FALSE;

	_beginBackground(p_device);
	if (p_port->act_start_read) {
		if (!(*p_port->act_start_read)(p_device, iaddr, isize, p_data, size)) {
			_failBackground(p_device);
			return FALSE;
		}
	}
	else {
		if (!(*p_port->act_read)(p_device, iaddr, isize, p_data, size)) {
			_failBackground(p_device);
			return FALSE;
		}
		p_owner->_state = DRS_COMPLETE;
	}
	return TRUE;
}

static PifDeviceRegState _checkTransfer(PifDeviceReg* p_owner)
{
	PifSpiDevice* p_device = (PifSpiDevice*)p_owner;
	PifSpiPort* p_port = p_device->_p_port;
	PifDeviceRegState state;

	if (p_port->__use_device != p_device) return DRS_IDLE;

	if (p_owner->_state == DRS_RUN && p_port->act_is_busy && !(*p_port->act_is_busy)(p_device)) {
		p_owner->_state = DRS_COMPLETE;
	}
	state = p_owner->_state;
	if (state == DRS_RUN) {
		if (pif_cumulative_timer1ms - p_owner->__start_time1ms <= p_owner->timeout) return DRS_RUN;
		pif_error = E_TIMEOUT;
		state = DRS_ERROR;
	}

	if (state == DRS_ERROR) p_port->error_count++;
	p_port->__use_device = NULL;
	p_owner->_state = DRS_IDLE;
	return state;
}

static BOOL _isBusy(PifDeviceReg* p_owner)
{
	return ((PifSpiDevice*)p_owner)->_p_port->__use_device != NULL;
}

static const PifDeviceRegOps c_device_ops = {
	_read,
	_write,
	_startRead,
	_checkTransfer,
	_isBusy
};


BOOL pifSpiPort_Init(PifSpiPort* p_owner, PifId id, uint8_t device_count)
{
	if (!p_owner || !device_count) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

    memset(p_owner, 0, sizeof(PifSpiPort));

    if (id == PIF_ID_AUTO) id = pif_id++;
    p_owner->_id = id;
    if (!pifObjArray_Init(&p_owner->__devices, sizeof(PifSpiDevice), device_count, NULL)) goto fail;
    return TRUE;

fail:
	pifSpiPort_Clear(p_owner);
	return FALSE;
}

void pifSpiPort_Clear(PifSpiPort* p_owner)
{
	pifObjArray_Clear(&p_owner->__devices);
}

PifSpiDevice* pifSpiPort_AddDevice(PifSpiPort* p_owner, PifId id, void *p_client)
{
	if (!p_owner) {
		pif_error = E_INVALID_PARAM;
		return NULL;
	}

	PifObjArrayIterator it = pifObjArray_Add(&p_owner->__devices);
    if (!it) return NULL;

    PifSpiDevice* p_device = (PifSpiDevice*)it->data;
    pifDeviceReg_Init(&p_device->parent, id, p_client, &c_device_ops);
    p_device->_p_port = p_owner;
    return p_device;
}

BOOL pifSpiPort_RemoveDevice(PifSpiPort* p_owner, PifSpiDevice* p_device)
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

PifSpiDevice* pifSpiPort_TemporaryDevice(PifSpiPort* p_owner, void *p_client)
{
	static PifSpiDevice device;

	// Every call hands out the same device, so it cannot be cleared under a transfer it started.
	if (p_owner->__use_device == &device) {
		pif_error = E_INVALID_STATE;
		return NULL;
	}

	memset(&device, 0, sizeof(PifSpiDevice));
	pifDeviceReg_Init(&device.parent, 0, p_client, &c_device_ops);
	device._p_port = p_owner;
	return &device;
}

BOOL pifSpiDevice_Transfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
	PifSpiPort* p_port;

	// A transfer is one chip select, so it cannot be split by max_transfer_size.
	if (!p_owner || !p_owner->_p_port->act_transfer || !size || (!p_write && !p_read)
			|| (p_owner->parent.max_transfer_size && size > p_owner->parent.max_transfer_size)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_takePort(p_owner)) return FALSE;

	p_port = p_owner->_p_port;
	(*p_port->act_transfer)(p_owner, p_write, p_read, size);
	p_port->__use_device = NULL;
	return TRUE;
}

BOOL pifSpiDevice_StartTransfer(PifSpiDevice* p_owner, uint8_t* p_write, uint8_t* p_read, size_t size)
{
	PifSpiPort* p_port;

	if (!p_owner || (!p_owner->_p_port->act_start_transfer && !p_owner->_p_port->act_transfer)
			|| !size || (!p_write && !p_read)
			|| (p_owner->parent.max_transfer_size && size > p_owner->parent.max_transfer_size)) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}
	if (!_takePort(p_owner)) return FALSE;

	p_port = p_owner->_p_port;
	_beginBackground(p_owner);
	if (!p_port->act_start_transfer) {
		(*p_port->act_transfer)(p_owner, p_write, p_read, size);
		p_owner->parent._state = DRS_COMPLETE;
		return TRUE;
	}

	if (!(*p_port->act_start_transfer)(p_owner, p_write, p_read, size)) {
		_failBackground(p_owner);
		return FALSE;
	}
	return TRUE;
}

void pifSpiPort_sigEndTransfer(PifSpiPort* p_owner, BOOL result)
{
	PifSpiDevice* p_device = (PifSpiDevice*)p_owner->__use_device;

	// Only a transfer still running is ended, so a second signal for one does not overwrite how
	// it ended.
	if (!p_device || p_device->parent._state != DRS_RUN) return;

	p_device->parent._state = result ? DRS_COMPLETE : DRS_ERROR;
}
