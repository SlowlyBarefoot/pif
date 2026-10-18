// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_transport.h
 * @brief Byte stream interface between a protocol and whatever carries its bytes.
 *
 * A driver such as PifUart, PifCanStream or PifDeviceStream embeds a PifTransport as its first member. A
 * protocol such as Modbus, MSP or a GPS parser attaches to it as the only client and is called back to parse
 * received bytes and to queue bytes to send, so the same protocol runs on every medium.
 */
#ifndef PIF_TRANSPORT_H
#define PIF_TRANSPORT_H


#include "core/pif.h"


struct StPifTransport;
typedef struct StPifTransport PifTransport;

/**
 * @brief Takes up to size received bytes into p_data.
 *
 * Given to the client in PifEvtTransportParsing. Returns the number of bytes taken.
 */
typedef uint16_t (*PifActTransportReceive)(PifTransport *p_transport, uint8_t *p_data, uint16_t size);
/**
 * @brief Queues up to size bytes from p_data to send.
 *
 * Given to the client in PifEvtTransportSending. Returns the number of bytes queued.
 */
typedef uint16_t (*PifActTransportSend)(PifTransport *p_transport, uint8_t *p_data, uint16_t size);

/**
 * @brief Lets the client parse received bytes, taken with act_receive.
 *
 * Returns TRUE while it took bytes, so the transport keeps polling soon.
 */
typedef BOOL (*PifEvtTransportParsing)(void *p_client, PifActTransportReceive act_receive);
/**
 * @brief Lets the client queue bytes to send with act_send.
 *
 * Returns the time in microseconds after which it is to be called again, or 0 for no need.
 */
typedef uint16_t (*PifEvtTransportSending)(void *p_client, PifActTransportSend act_send);
/**
 * @brief Tells the client that received bytes were dropped, so it starts its frame over.
 */
typedef void (*PifEvtTransportAbortRx)(void *p_client);

/**
 * @class StPifTransportOps
 * @brief Functions a driver gives to implement a PifTransport. Only attach_client and detach_client are
 *        required. Any other left NULL takes the default noted on its pifTransport_ function.
 */
typedef struct StPifTransportOps
{
	/// Makes p_client the only client. Required.
	void (*attach_client)(PifTransport *p_transport, void *p_client, PifEvtTransportParsing evt_parsing,
			PifEvtTransportSending evt_sending, PifEvtTransportAbortRx evt_abort_rx);
	void (*detach_client)(PifTransport *p_transport);	///< Removes the client. Required.
	void (*trigger_send)(PifTransport *p_transport);	///< Calls evt_sending soon. See pifTransport_TriggerSend().
	SWITCH (*get_flow_state)(PifTransport *p_transport);	///< Gets the flow state. See pifTransport_GetFlowState().
	uint8_t (*get_frame_size)(PifTransport *p_transport);	///< Gets the send unit. See pifTransport_GetFrameSize().
	uint16_t (*get_byte_time)(PifTransport *p_transport);	///< Gets the time of one byte. See pifTransport_GetByteTime().
} PifTransportOps;

/**
 * @class StPifTransport
 * @brief Byte stream that a protocol sends and receives through, whatever carries it.
 *
 * A driver embeds a PifTransport as its first member, named parent, and calls pifTransport_Init() from its
 * own init. A protocol attaches to it as its client. The driver then calls evt_parsing when bytes may have
 * arrived and evt_sending when it can take bytes, each with the action that reads or writes them. When it
 * loses received bytes, it calls evt_abort_rx so the client can drop the frame it was parsing.
 */
struct StPifTransport
{
	// Private Member Variable
	const PifTransportOps *__p_ops;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets the functions that implement the transport. A driver calls it from its own init.
 * @param p_owner Pointer to the transport embedded in the driver.
 * @param p_ops Functions of the driver. They must stay valid while the transport is used.
 */
void pifTransport_Init(PifTransport *p_owner, const PifTransportOps *p_ops);

/**
 * @brief Makes p_client the only client of the transport. A client attached before is replaced.
 * @param p_owner Pointer to the transport.
 * @param p_client Pointer passed back to both events.
 * @param evt_parsing Called to read the bytes that arrived.
 * @param evt_sending Called to write bytes when the transport can take them.
 * @param evt_abort_rx Optional. Called when received bytes were lost, so the frame being parsed is broken.
 *        The bytes it has not read yet are gone too by then. A transport that never loses bytes this way
 *        does not call it.
 */
void pifTransport_AttachClient(PifTransport *p_owner, void *p_client, PifEvtTransportParsing evt_parsing,
		PifEvtTransportSending evt_sending, PifEvtTransportAbortRx evt_abort_rx);

/**
 * @brief Removes the client. Bytes the transport still holds in either direction are dropped.
 * @param p_owner Pointer to the transport.
 */
void pifTransport_DetachClient(PifTransport *p_owner);

/**
 * @brief Asks the transport to call evt_sending soon, for a client that has something new to send.
 *        Without trigger_send it does nothing, and evt_sending is called only when the driver chooses.
 * @param p_owner Pointer to the transport.
 */
void pifTransport_TriggerSend(PifTransport *p_owner);

/**
 * @brief Tells whether flow control lets data move. Without get_flow_state it is always ON.
 * @param p_owner Pointer to the transport.
 * @return OFF while flow control holds the transfer, otherwise ON.
 */
SWITCH pifTransport_GetFlowState(PifTransport *p_owner);

/**
 * @brief Gets the unit that the transport sends in. A client pads what it sends to a multiple of it.
 *        Without get_frame_size it is 1.
 * @param p_owner Pointer to the transport.
 * @return Frame size in bytes.
 */
uint8_t pifTransport_GetFrameSize(PifTransport *p_owner);

/**
 * @brief Gets the time one byte takes on the medium, which a client adds to its timeouts.
 *        Without get_byte_time it is 0.
 * @param p_owner Pointer to the transport.
 * @return Time in us.
 */
uint16_t pifTransport_GetByteTime(PifTransport *p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_TRANSPORT_H
