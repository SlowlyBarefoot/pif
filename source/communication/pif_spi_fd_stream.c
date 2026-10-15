// SPDX-License-Identifier: BSD-3-Clause
#include "communication/pif_spi_fd_stream.h"


#define DEFAULT_IDLE_BYTE	0xFF


/**
 * @brief Starts a transfer that sends what the client wrote, filled up with the idle byte, and reads what
 *        the device sends meanwhile.
 * @return FALSE when there is nothing to send or to read, or the transfer could not start.
 */
static BOOL _startTransfer(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream)
{
	PifSpiFdProtocol *p_owner = (PifSpiFdProtocol *)p_protocol;
	PifDeviceReg *p_device = p_stream->_p_device;
	uint16_t size, sent;

	if (!p_stream->__evt_parsing && !p_stream->__tx_len) return FALSE;

	size = p_owner->__out_size;
	if (size > p_stream->__rx_size) size = p_stream->__rx_size;
	if (p_device->max_transfer_size && size > p_device->max_transfer_size) size = p_device->max_transfer_size;
	sent = p_stream->__tx_len < size ? p_stream->__tx_len : size;
	memcpy(p_owner->__p_out, p_stream->__p_tx, sent);
	memset(p_owner->__p_out + sent, p_owner->__idle_byte, size - sent);

	// Bytes that could not go out stay for the next poll.
	if (!pifSpiDevice_StartTransfer((PifSpiDevice *)p_device, p_owner->__p_out, p_stream->__p_rx, size)) return FALSE;
	p_owner->__sent = sent;
	p_owner->__size = size;
	return TRUE;
}

static PifDeviceStreamNext _finishTransfer(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream, BOOL ok)
{
	PifSpiFdProtocol *p_owner = (PifSpiFdProtocol *)p_protocol;
	BOOL received = FALSE;
	uint16_t i;

	// What the device sent is lost, and what was sent to it may not have arrived, so it goes again.
	if (!ok) {
		pifDeviceStream_AbortRx(p_stream);
		return DSN_WAIT;
	}

	pifDeviceStream_TakeTx(p_stream, p_owner->__sent);
	for (i = 0; i < p_owner->__size; i++) {
		if (p_stream->__p_rx[i] != p_owner->__idle_byte) {
			received = TRUE;
			break;
		}
	}
	if (received) pifDeviceStream_SetRx(p_stream, p_owner->__size);
	// A device that sent something may have more, so it is polled again at once until it answers empty.
	return received || p_stream->__tx_len ? DSN_NEXT_RUN : DSN_WAIT;
}

// Bytes to send go out with the reads, so there is no write, and nothing is kept between transfers.
static const PifDeviceStreamProtocolOps c_protocol_ops = {
	_startTransfer,
	_finishTransfer,
	NULL,
	NULL
};

static BOOL _initProtocol(PifSpiFdProtocol *p_owner, uint16_t transfer_size)
{
	memset(p_owner, 0, sizeof(PifSpiFdProtocol));

	p_owner->__p_out = calloc(sizeof(uint8_t), transfer_size);
	if (!p_owner->__p_out) {
		pif_error = E_OUT_OF_HEAP;
		return FALSE;
	}
	p_owner->__out_size = transfer_size;
	pifDeviceStreamProtocol_Init(&p_owner->parent, &c_protocol_ops);
	p_owner->__idle_byte = DEFAULT_IDLE_BYTE;
	return TRUE;
}


BOOL pifSpiFdStream_Init(PifSpiFdStream *p_owner, PifId id, PifSpiPort *p_port, uint16_t transfer_size,
		uint16_t rx_size, uint16_t tx_size, uint16_t period1ms)
{
	if (!p_owner || !p_port || !transfer_size) {
		pif_error = E_INVALID_PARAM;
		return FALSE;
	}

	memset(p_owner, 0, sizeof(PifSpiFdStream));

	if (!_initProtocol(&p_owner->__protocol, transfer_size)) return FALSE;

	p_owner->_p_device = pifSpiPort_AddDevice(p_port, PIF_ID_AUTO, NULL);
	if (!p_owner->_p_device) goto fail;

	if (!pifDeviceStream_Init(&p_owner->stream, id, &p_owner->_p_device->parent, &p_owner->__protocol.parent, rx_size,
			tx_size, period1ms, "SpiFd")) goto fail;
	return TRUE;

fail:
	if (p_owner->_p_device) pifSpiPort_RemoveDevice(p_port, p_owner->_p_device);
	free(p_owner->__protocol.__p_out);
	memset(p_owner, 0, sizeof(PifSpiFdStream));
	return FALSE;
}

void pifSpiFdStream_SetIdleByte(PifSpiFdStream *p_owner, uint8_t idle_byte)
{
	p_owner->__protocol.__idle_byte = idle_byte;
}

void pifSpiFdStream_Clear(PifSpiFdStream *p_owner)
{
	if (!p_owner->_p_device) return;

	pifDeviceStream_Clear(&p_owner->stream);
	pifSpiPort_RemoveDevice(p_owner->_p_device->_p_port, p_owner->_p_device);
	free(p_owner->__protocol.__p_out);
	memset(p_owner, 0, sizeof(PifSpiFdStream));
}
