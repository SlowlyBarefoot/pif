// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_TRANSPORT_H
#define PIF_TRANSPORT_H


#include "core/pif.h"


struct StPifTransport;
typedef struct StPifTransport PifTransport;

typedef uint16_t (*PifActTransportReceive)(PifTransport *p_transport, uint8_t *p_data, uint16_t size);
typedef uint16_t (*PifActTransportSend)(PifTransport *p_transport, uint8_t *p_data, uint16_t size);

typedef BOOL (*PifEvtTransportParsing)(void *p_client, PifActTransportReceive act_receive);
typedef uint16_t (*PifEvtTransportSending)(void *p_client, PifActTransportSend act_send);

/**
 * @class StPifTransportOps
 * @brief Functions a driver gives to implement a PifTransport. Only attach_client and detach_client are
 *        required. Any other left NULL takes the default noted on its pifTransport_ function.
 */
typedef struct StPifTransportOps
{
	void (*attach_client)(PifTransport *p_transport, void *p_client, PifEvtTransportParsing evt_parsing,
			PifEvtTransportSending evt_sending);
	void (*detach_client)(PifTransport *p_transport);
	void (*trigger_send)(PifTransport *p_transport);
	SWITCH (*get_flow_state)(PifTransport *p_transport);
	uint8_t (*get_frame_size)(PifTransport *p_transport);
	uint16_t (*get_byte_time)(PifTransport *p_transport);
} PifTransportOps;

/**
 * @class StPifTransport
 * @brief Byte stream that a protocol sends and receives through, whatever carries it.
 *
 * A driver embeds a PifTransport as its first member, named parent, and calls pifTransport_Init() from its
 * own init. A protocol attaches to it as its client. The driver then calls evt_parsing when bytes may have
 * arrived and evt_sending when it can take bytes, each with the action that reads or writes them.
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
 * @fn pifTransport_Init
 * @brief Sets the functions that implement the transport. A driver calls it from its own init.
 * @param p_owner Pointer to the transport embedded in the driver.
 * @param p_ops Functions of the driver. They must stay valid while the transport is used.
 */
void pifTransport_Init(PifTransport *p_owner, const PifTransportOps *p_ops);

/**
 * @fn pifTransport_AttachClient
 * @brief Makes p_client the only client of the transport. A client attached before is replaced.
 * @param p_owner Pointer to the transport.
 * @param p_client Pointer passed back to both events.
 * @param evt_parsing Called to read the bytes that arrived.
 * @param evt_sending Called to write bytes when the transport can take them.
 */
void pifTransport_AttachClient(PifTransport *p_owner, void *p_client, PifEvtTransportParsing evt_parsing,
		PifEvtTransportSending evt_sending);

/**
 * @fn pifTransport_DetachClient
 * @brief Removes the client. Bytes the transport still holds in either direction are dropped.
 * @param p_owner Pointer to the transport.
 */
void pifTransport_DetachClient(PifTransport *p_owner);

/**
 * @fn pifTransport_TriggerSend
 * @brief Asks the transport to call evt_sending soon, for a client that has something new to send.
 *        Without trigger_send it does nothing, and evt_sending is called only when the driver chooses.
 * @param p_owner Pointer to the transport.
 */
void pifTransport_TriggerSend(PifTransport *p_owner);

/**
 * @fn pifTransport_GetFlowState
 * @brief Tells whether flow control lets data move. Without get_flow_state it is always ON.
 * @param p_owner Pointer to the transport.
 * @return OFF while flow control holds the transfer, otherwise ON.
 */
SWITCH pifTransport_GetFlowState(PifTransport *p_owner);

/**
 * @fn pifTransport_GetFrameSize
 * @brief Gets the unit that the transport sends in. A client pads what it sends to a multiple of it.
 *        Without get_frame_size it is 1.
 * @param p_owner Pointer to the transport.
 * @return Frame size in bytes.
 */
uint8_t pifTransport_GetFrameSize(PifTransport *p_owner);

/**
 * @fn pifTransport_GetByteTime
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
