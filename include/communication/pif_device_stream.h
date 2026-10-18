// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_device_stream.h
 * @brief Byte stream polled from a device on a bus, as a PifTransport, whatever the bus and the device are.
 *
 * PifDeviceStream is the part every polled stream shares: the client, the buffers, the task and the order
 * of reads and writes. How the bytes are taken from and given to one kind of device is a protocol, an object
 * of its own that embeds a PifDeviceStreamProtocol as its first member, named parent. A new kind of device
 * takes only a new protocol, and the protocol of a stream can be changed.
 *
 * The stream does not add the device to its port, and a protocol works only on the bus it is written for,
 * which the stream cannot check. A wrapper puts the right device, stream and protocol together for an
 * application: gps/pif_ubx_ddc_stream for a device on I2C that tells how many bytes it holds, and
 * pif_spi_fd_stream for a device on SPI that exchanges bytes in full duplex.
 *
 * The task runs one transfer at a time in the background with the pifDeviceReg_ functions, so the port is
 * free for other devices between transfers. When the client has bytes to send, it is asked for them only
 * while no transfer is running, and the protocol sends them with the next transfer or with its write.
 */
#ifndef PIF_DEVICE_STREAM_H
#define PIF_DEVICE_STREAM_H


#include "communication/pif_device_reg.h"
#include "communication/pif_transport.h"
#include "core/pif_task_manager.h"


struct StPifDeviceStream;
typedef struct StPifDeviceStream PifDeviceStream;

struct StPifDeviceStreamProtocol;
typedef struct StPifDeviceStreamProtocol PifDeviceStreamProtocol;

/**
 * @brief What follows a transfer, as the protocol's finish tells it.
 */
typedef enum EnPifDeviceStreamNext
{
	DSN_WAIT,			///< Nothing until the next period.
	DSN_NEXT_RUN,		///< The device holds more, so the next transfer runs in the next release of the task.
	DSN_NOW				///< The next transfer belongs to this one, so it starts at once.
} PifDeviceStreamNext;

/**
 * @class StPifDeviceStreamProtocolOps
 * @brief Transfers of one kind of device. start and finish are required.
 */
typedef struct StPifDeviceStreamProtocolOps
{
	/// Starts the next transfer in the background, or returns FALSE when there is nothing to do. It is
	/// called only while no transfer is running, the port is free and the client has read every byte of
	/// the last one. It may read into __p_rx (__rx_size bytes) and send from __p_tx (__tx_len bytes), but
	/// has to copy what it sends: __p_tx may be emptied while the transfer runs, when the client changes.
	BOOL (*start)(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream);
	/// Ends the transfer that start began. ok is FALSE when it failed. It gives the bytes read to the client
	/// with pifDeviceStream_SetRx() and takes what it sent from __p_tx with pifDeviceStream_TakeTx(). A client
	/// hands over new bytes only after finish, so emptied bytes are never taken from the next ones.
	PifDeviceStreamNext (*finish)(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream, BOOL ok);
	/// Optional. Writes the __tx_len bytes of __p_tx, waiting until it is over, for a device whose reads
	/// cannot carry them. Without it start sends them.
	void (*write)(PifDeviceStreamProtocol *p_protocol, PifDeviceStream *p_stream);
	/// Optional. Forgets what the protocol knows of a device, when it is given to a stream.
	void (*reset)(PifDeviceStreamProtocol *p_protocol);
} PifDeviceStreamProtocolOps;

/**
 * @class StPifDeviceStreamProtocol
 * @brief Base of a protocol. A protocol serves one stream at a time.
 */
struct StPifDeviceStreamProtocol
{
	// Private Member Variable
	const PifDeviceStreamProtocolOps *__p_ops;
};

/**
 * @class StPifDeviceStream
 * @brief Byte stream polled from a device. Attach a protocol client, such as a GPS, to &stream.parent.
 */
struct StPifDeviceStream
{
	PifTransport parent;	///< Transport the client uses. Must stay the first member.

	// Read-only Member Variable
	PifId _id;	///< Instance ID
	PifDeviceReg *_p_device;			///< Set its timeout and max_transfer_size as the device needs.
	PifDeviceStreamProtocol *_p_protocol;	///< Protocol that moves the bytes, changed with pifDeviceStream_SetProtocol()
	PifTask *_p_task;					///< Polls the device. Its period is the poll period.

	/// Private Member Variable, also used by the protocol
	void *__p_client;
	PifEvtTransportParsing __evt_parsing;
	PifEvtTransportSending __evt_sending;
	PifEvtTransportAbortRx __evt_abort_rx;
	uint8_t *__p_rx;					///< __rx_size bytes, followed by the __tx_size bytes of __p_tx.
	uint8_t *__p_tx;
	uint16_t __rx_size;
	uint16_t __tx_size;
	uint16_t __rx_pos;					///< Bytes of __p_rx the client has read.
	uint16_t __rx_len;					///< Bytes of __p_rx given to the client.
	uint16_t __tx_len;					///< Bytes of __p_tx not sent yet.
	BOOL __running;						///< A transfer from start is running.
	BOOL __more;						///< The last finish asked for DSN_NEXT_RUN.
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the transfers of a protocol. A protocol calls it from its own init.
 * @param p_owner Pointer to the protocol base embedded in the protocol.
 * @param p_ops Transfers of the protocol. They must stay valid while the protocol is used.
 */
void pifDeviceStreamProtocol_Init(PifDeviceStreamProtocol *p_owner, const PifDeviceStreamProtocolOps *p_ops);

/**
 * @brief Initializes a stream and adds its task, which waits until a client is attached.
 * @param p_owner Pointer to the stream.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param p_device Device the stream polls, for example &i2c_device->parent. It must stay valid while the
 *        stream is used, and stays on its port when the stream is cleared.
 * @param p_protocol Protocol of the device, for example &my_protocol.parent. It must stay valid while the stream
 *        uses it.
 * @param rx_size Most bytes taken from the device in one transfer.
 * @param tx_size Most bytes the client may hand over at a time. 0 for a stream that never sends.
 * @param period1ms How often the device is polled when it has nothing, in ms.
 * @param name Name of the task, or NULL for "DevStream".
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifDeviceStream_Init(PifDeviceStream *p_owner, PifId id, PifDeviceReg *p_device,
		PifDeviceStreamProtocol *p_protocol, uint16_t rx_size, uint16_t tx_size, uint16_t period1ms, const char *name);

/**
 * @brief Detaches the client, waits for a transfer that is running, then removes the task and frees the
 *        buffers. The device stays on its port.
 * @param p_owner Pointer to the stream.
 */
void pifDeviceStream_Clear(PifDeviceStream *p_owner);

/**
 * @brief Changes the protocol of the stream, for a device found to speak another one. The bytes the client
 *        has not read or sent yet are dropped.
 * @param p_owner Pointer to the stream.
 * @param p_protocol New protocol.
 * @return TRUE on success, otherwise FALSE with pif_error set to E_INVALID_STATE while a transfer is running
 *         or E_INVALID_PARAM for a protocol without start or finish.
 */
BOOL pifDeviceStream_SetProtocol(PifDeviceStream *p_owner, PifDeviceStreamProtocol *p_protocol);

/**
 * @brief For a protocol: gives the first size bytes of __p_rx to the client.
 * @param p_owner Pointer to the stream.
 * @param size Bytes, up to __rx_size.
 */
void pifDeviceStream_SetRx(PifDeviceStream *p_owner, uint16_t size);

/**
 * @brief For a protocol: removes the first size bytes of __p_tx, which have been sent.
 * @param p_owner Pointer to the stream.
 * @param size Bytes, up to __tx_len.
 */
void pifDeviceStream_TakeTx(PifDeviceStream *p_owner, uint16_t size);

/**
 * @brief For a protocol: tells the client that received bytes were lost, and drops the bytes it has not
 *        read yet.
 * @param p_owner Pointer to the stream.
 */
void pifDeviceStream_AbortRx(PifDeviceStream *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_DEVICE_STREAM_H
