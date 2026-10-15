// SPDX-License-Identifier: BSD-3-Clause
#include "gps/pif_ubx_ddc_stream.h"


#define COUNT_REG	0xFD

enum {
	UDP_COUNT,							// Reading the count registers.
	UDP_DATA							// Reading the bytes.
};


/**
 * @brief Starts reading the bytes the receiver holds, or its count when none are known.
 * @return FALSE when no read was started.
 */
static BOOL _startRead(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream)
{
	PifUbxDdcProtocol *p_owner = (PifUbxDdcProtocol *)p_protocol;
	PifDeviceReg *p_device = p_stream->_p_device;
	uint16_t size;

	// Bytes are read only for a client that takes them.
	if (!p_stream->__evt_parsing) return FALSE;

	if (!p_owner->__pending) {
		if (!pifDeviceReg_StartRead(p_device, p_owner->__count_reg, 1, p_owner->__count, 2)) return FALSE;
		p_owner->__phase = UDP_COUNT;
		return TRUE;
	}

	size = p_owner->__pending;
	if (size > p_stream->__rx_size) size = p_stream->__rx_size;
	if (p_device->max_transfer_size && size > p_device->max_transfer_size) size = p_device->max_transfer_size;
	// The receiver sends the bytes from where its address was left after the count, so none is given.
	if (!pifDeviceReg_StartRead(p_device, 0, 0, p_stream->__p_rx, size)) {
		// A failed read may have taken bytes, so the count is asked for again.
		p_owner->__pending = 0;
		pifDeviceStream_AbortRx(p_stream);
		return FALSE;
	}
	p_owner->__read_size = size;
	p_owner->__phase = UDP_DATA;
	return TRUE;
}

static PifDeviceStreamNext _finishRead(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream, BOOL ok)
{
	PifUbxDdcProtocol *p_owner = (PifUbxDdcProtocol *)p_protocol;
	uint16_t count;

	if (p_owner->__phase == UDP_COUNT) {
		if (!ok) return DSN_WAIT;
		count = (p_owner->__count[0] << 8) | p_owner->__count[1];
		// A receiver with nothing to send may answer 0xFFFF, and no real count has the top bit set.
		if (count & 0x8000) count = 0;
		p_owner->__pending = count;
		return count ? DSN_NOW : DSN_WAIT;
	}

	if (!ok) {
		p_owner->__pending = 0;
		pifDeviceStream_AbortRx(p_stream);
		return DSN_WAIT;
	}
	pifDeviceStream_SetRx(p_stream, p_owner->__read_size);
	p_owner->__pending -= p_owner->__read_size;
	return p_owner->__pending ? DSN_NEXT_RUN : DSN_WAIT;
}

static void _writeBytes(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream)
{
	// Bytes the receiver refuses are dropped.
	pifDeviceReg_Write(p_stream->_p_device, 0, 0, p_stream->__p_tx, p_stream->__tx_len);
}

static void _reset(PifDeviceStreamProtocol *p_protocol)
{
	((PifUbxDdcProtocol *)p_protocol)->__pending = 0;
}

static const PifDeviceStreamProtocolOps c_protocol_ops = {
	_startRead,
	_finishRead,
	_writeBytes,
	_reset
};

static void _initProtocol(PifUbxDdcProtocol *p_owner)
{
	memset(p_owner, 0, sizeof(PifUbxDdcProtocol));
	pifDeviceStreamProtocol_Init(&p_owner->parent, &c_protocol_ops);
	p_owner->__count_reg = COUNT_REG;
}


BOOL pifUbxDdcStream_Init(PifUbxDdcStream *p_owner, PifId id, PifI2cPort *p_port, uint8_t addr, uint16_t rx_size,
		uint16_t tx_size, uint16_t period1ms)
{
	if (!p_owner || !p_port) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifUbxDdcStream));

	p_owner->_p_device = pifI2cPort_AddDevice(p_port, PIF_ID_AUTO, addr, NULL);
	if (!p_owner->_p_device) return FALSE;

	_initProtocol(&p_owner->__protocol);
	if (!pifDeviceStream_Init(&p_owner->stream, id, &p_owner->_p_device->parent, &p_owner->__protocol.parent, rx_size,
			tx_size, period1ms, "UbxDdc")) {
		pifI2cPort_RemoveDevice(p_port, p_owner->_p_device);
		memset(p_owner, 0, sizeof(PifUbxDdcStream));
		return FALSE;
	}
	return TRUE;
}

void pifUbxDdcStream_Clear(PifUbxDdcStream *p_owner)
{
	if (!p_owner->_p_device) return;

	pifDeviceStream_Clear(&p_owner->stream);
	pifI2cPort_RemoveDevice(p_owner->_p_device->_p_port, p_owner->_p_device);
	memset(p_owner, 0, sizeof(PifUbxDdcStream));
}
