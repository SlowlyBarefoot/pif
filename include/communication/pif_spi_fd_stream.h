// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_spi_fd_stream.h
 * @brief Byte stream exchanged with a device on SPI in full duplex, as a PifTransport.
 *
 * Every poll is one full-duplex transfer: the bytes the client wrote go out first, the idle byte fills the
 * rest, and what the device sends back at the same time is read. A device with nothing to send answers with
 * the idle byte, so a transfer that brings back only idle bytes counts as empty. The idle bytes in a transfer
 * that also brought data reach the client, since the stream cannot tell them from data: a client parses them
 * as bytes outside its frames. The SPI port of u-blox receivers works this way, with the default idle byte.
 *
 * PifSpiFdStream puts together what that takes: the SPI device, a PifDeviceStream and the full-duplex protocol
 * it runs. Attach a client to &stream.stream.parent:
 *
 *     pifSpiFdStream_Init(&s_gps_spi, PIF_ID_AUTO, &g_spi_port, 64, 128, 64, 100);
 *     pifGpsUblox_AttachTransport(&s_gps_ublox, &s_gps_spi.stream.parent);
 */
#ifndef PIF_SPI_FD_STREAM_H
#define PIF_SPI_FD_STREAM_H


#include "communication/pif_device_stream.h"
#include "communication/pif_spi.h"


/**
 * @class StPifSpiFdProtocol
 * @brief The full-duplex protocol a PifSpiFdStream runs. Only for the stream: a client does not use it.
 */
typedef struct StPifSpiFdProtocol
{
	PifDeviceStreamProtocol parent;	///< Protocol part of the stream. Must stay the first member.

	// Private Member Variable
	uint8_t __idle_byte;
	uint8_t *__p_out;					///< What the transfer that is running sends.
	uint16_t __out_size;
	uint16_t __sent;					///< Bytes of the client in it.
	uint16_t __size;					///< Its length.
} PifSpiFdProtocol;

/**
 * @class StPifSpiFdStream
 * @brief Byte stream exchanged with a device on SPI in full duplex. Attach a client to &stream.parent.
 */
typedef struct StPifSpiFdStream
{
	// Public Member Variable
	PifDeviceStream stream;				///< Its task is stream._p_task, whose period is the poll period.

	// Read-only Member Variable
	PifSpiDevice *_p_device;			///< Set its parent.timeout and parent.max_transfer_size as the port needs.

	// Private Member Variable
	PifSpiFdProtocol __protocol;
} PifSpiFdStream;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Adds the device to the SPI port and initializes the stream, whose task waits until a client is
 *        attached. The idle byte is 0xFF.
 * @param p_owner Pointer to the stream.
 * @param id Logical ID of the stream, or PIF_ID_AUTO.
 * @param p_port SPI port the device is on.
 * @param transfer_size Length of every transfer. rx_size and max_transfer_size of the device also limit it.
 * @param rx_size Most bytes taken from the device in one transfer.
 * @param tx_size Most bytes the client may hand over at a time. 0 for a client that never sends.
 * @param period1ms How often the device is polled when it has nothing, in ms.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifSpiFdStream_Init(PifSpiFdStream *p_owner, PifId id, PifSpiPort *p_port, uint16_t transfer_size,
		uint16_t rx_size, uint16_t tx_size, uint16_t period1ms);

/**
 * @brief Sets the byte sent when there is nothing to send, which an empty device answers with too.
 * @param p_owner Pointer to the stream.
 * @param idle_byte The idle byte of the device.
 */
void pifSpiFdStream_SetIdleByte(PifSpiFdStream *p_owner, uint8_t idle_byte);

/**
 * @brief Detaches the client, waits for a transfer that is running, then removes the task and the device and
 *        frees the buffers.
 * @param p_owner Pointer to the stream.
 */
void pifSpiFdStream_Clear(PifSpiFdStream *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_SPI_FD_STREAM_H
