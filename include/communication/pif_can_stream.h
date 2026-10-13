// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_can_stream.h
 * @brief Byte stream carried over CAN, as a PifTransport.
 *
 * PifCanStream uses one pair of IDs: the bytes a client writes are packed into frames with tx_id, and the
 * data of the frames with rx_id is read back as bytes. CAN keeps the frames of one ID in order and
 * retransmits a damaged one, so no other framing is added. Each ID must come from one node only, and nothing
 * stops the sender when the receiver's buffer is full: bytes that do not fit are dropped and counted in
 * _rx_lost, as on a UART without flow control.
 */
#ifndef PIF_CAN_STREAM_H
#define PIF_CAN_STREAM_H


#include "communication/pif_can.h"
#include "communication/pif_transport.h"
#include "core/pif_ring_buffer.h"


/**
 * @class StPifCanStream
 * @brief Byte stream over one pair of CAN IDs. Attach a protocol to &stream.parent.
 */
typedef struct StPifCanStream
{
	PifTransport parent;

	// Read-only Member Variable
	PifId _id;
	PifCan *_p_can;
	uint32_t _tx_id;
	uint32_t _rx_id;
	uint8_t _flags;						// CAN_F_EXTENDED, CAN_F_FD and CAN_F_BRS. Both IDs share CAN_F_EXTENDED.
	uint8_t _frame_size;				// Largest data put in one frame.
	uint16_t _rx_lost;					// Bytes dropped because the receive buffer was full.
	PifRingBuffer _rx_buffer;

	// Private Member Variable
	PifCanClient __can_client;			// Added to _p_can.
	void *__p_client;
	PifEvtTransportParsing __evt_parsing;
	PifEvtTransportSending __evt_sending;
	PifCanFrame __tx_frame;				// Bytes written but not queued yet.
	uint16_t __tx_written;				// Bytes written during the current evt_sending.
} PifCanStream;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifCanStream_Init
 * @brief Initializes a byte stream and adds it to the CAN controller as a client. Received frames with rx_id
 *        go to the stream from then on, unless a client added before it takes them.
 * @param p_owner Pointer to the stream.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param p_can CAN controller that carries the stream.
 * @param tx_id ID of the frames sent.
 * @param rx_id ID of the frames received.
 * @param flags CAN_F_EXTENDED for 29 bit IDs, CAN_F_FD to send CAN FD frames, CAN_F_BRS with it to switch
 *        the bit rate.
 * @param frame_size Largest data put in one frame: 1 to 8, or with CAN_F_FD a CAN FD length up to
 *        PIF_CAN_MAX_DATA_SIZE.
 * @param rx_size Bytes the receive buffer holds, up to 65534. A frame that does not fit is dropped whole.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifCanStream_Init(PifCanStream *p_owner, PifId id, PifCan *p_can, uint32_t tx_id, uint32_t rx_id,
		uint8_t flags, uint8_t frame_size, uint16_t rx_size);

/**
 * @fn pifCanStream_Clear
 * @brief Detaches the client, removes the stream from its CAN controller and frees the receive buffer.
 * @param p_owner Pointer to the stream.
 */
void pifCanStream_Clear(PifCanStream *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_CAN_STREAM_H
