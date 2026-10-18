// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_msp.h
 * @brief MultiWii Serial Protocol (MSPv1) over a PifTransport.
 *
 * Parses $M frames, hands each complete packet to evt_receive and sends the answers made with the
 * pifMsp_MakeAnswer functions. Bytes that are not part of a frame go to evt_other_packet, so MSP can share a
 * port with a command line or another protocol. pif_msp_v2 adds MSPv2 on top of it.
 */
#ifndef PIF_MSP_H
#define PIF_MSP_H


#include "communication/pif_transport.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer_manager.h"


/// Size of the receive buffer of one packet. The MSPv1 parser of pif_msp keeps the length, command and
/// checksum bytes in it too, so its payload can be 3 bytes shorter than this.
/// 0: nothing is allocated in pifMsp_Init(); the client gives a buffer with pifMsp_AssignRxBuffer().
#ifndef PIF_MSP_RX_PACKET_SIZE
#define PIF_MSP_RX_PACKET_SIZE		128
#endif

/// Size of the ring buffer that holds the answers waiting to be sent.
/// 0: nothing is allocated in pifMsp_Init(); the client gives a buffer with pifMsp_AssignAnswerBuffer().
#ifndef PIF_MSP_TX_ANSWER_SIZE
#define PIF_MSP_TX_ANSWER_SIZE		128
#endif

/// Timeout used to receive one complete packet.
/// 0: no timeout limit.
/// 1 or greater: multiplied by the timer unit configured in pifMsp_Init().
/// Default is 200 ticks in this header; at 1 ms timer unit, that equals 200 ms.
#ifndef PIF_MSP_RECEIVE_TIMEOUT
#define PIF_MSP_RECEIVE_TIMEOUT		200
#endif

//#define __DEBUG_PACKET__


/**
 * @brief Receive state of the MSPv1 parser.
 */
typedef enum EnPifMspRxState
{
	MRS_IDLE			= 0,	///< Waiting for '$'
	MRS_HEADER_CHAR 	= 1,	///< Waiting for 'M'
	MRS_DIRECTION		= 2,	///< Waiting for the direction
	MRS_LENGTH			= 3,	///< Waiting for the size
	MRS_MESSAGE_TYPE	= 4,	///< Waiting for the command
	MRS_DATA        	= 5,	///< Receiving the payload and the checksum
	MRS_DONE			= 6	///< A packet is complete
} PifMspRxState;

/**
 * @brief Transmit state.
 */
typedef enum EnPifMspTxState
{
	MTS_IDLE			= 0,	///< Nothing being sent
	MTS_SENDING			= 1		///< An answer is being sent
} PifMspTxState;

/**
 * @brief Frame format of a packet.
 */
typedef enum EnPifMspVersion
{
	MV_V1				= 0,	///< $M
	MV_V2_OVER_V1		= 1,	///< $M with command 255 and an MSPv2 frame as payload
	MV_V2_NATIVE		= 2		///< $X
} PifMspVersion;

/**
 * @brief Direction of a packet, from its direction byte.
 */
typedef enum EnPifMspPacketType
{
	MPT_COMMAND			= 0,	///< '<'
	MPT_REPLY			= 1		///< '>'
} PifMspPacketType;

/**
 * @brief What a byte given to pifMsp_ParsingPacket() did.
 */
typedef enum EnPifMspFrame
{
	MF_NONE				= 0,	///< Part of a packet that is not complete yet, or of one that was dropped.
	MF_OTHER			= 1,	///< Not part of an MSP packet; it has also gone to evt_other_packet.
	MF_PACKET			= 2		///< Completed a packet, which has gone to evt_receive.
} PifMspFrame;


/**
 * @class StPifMspPacket
 * @brief One received MSP packet.
 */
typedef struct StPifMspPacket
{
	uint16_t command;			///< Command of the packet
	uint16_t data_count;		///< Payload size in bytes
	uint8_t* p_data;			///< Payload
	uint8_t* p_pointer;			///< Read position of the pifMsp_ReadData functions in p_data
	uint8_t flags;				///< MSPv2 flags, 0 for MSPv1
	PifMspVersion version;		///< Frame the packet came in, which is the one its answer goes out in
	PifMspPacketType type;		///< Command or reply
} PifMspPacket;


struct StPifMsp;
typedef struct StPifMsp PifMsp;

/**
 * @brief Gets a complete packet. p_issuer is the pointer given to pifMsp_AttachEvtReceive().
 */
typedef void (*PifEvtMspReceive)(PifMsp* p_owner, PifMspPacket* p_packet, PifIssuerP p_issuer);
/**
 * @brief Reports an error of the instance id. Not called by pif_msp itself.
 */
typedef void (*PifEvtMspError)(PifId id);
/**
 * @brief Gets a byte that is not part of an MSP frame.
 */
typedef void (*PifEvtMspOtherPacket)(PifMsp* p_owner, uint8_t data, PifIssuerP p_issuer);

/// Parser of one received byte. pif_msp has the MSPv1 one; a derived protocol replaces it.
typedef PifMspFrame (*PifActMspParsing)(void* p_parser, uint8_t data);

/**
 * @brief Receive side state of an MSP instance.
 */
typedef struct StPifMspRx
{
	uint8_t state;				///< PifMspRxState, or the state of the parser that replaced it. 0 is idle.
	uint8_t* p_packet;			///< Receive buffer
	uint16_t packet_size;		///< Size of p_packet
	uint16_t packet_count;		///< Bytes in p_packet
	BOOL packet_static;			///< p_packet is the caller's, given with pifMsp_AssignRxBuffer()
	PifMspPacket packet;		///< Packet being received
	uint8_t pre_error;			///< Last error logged, so the same one is not logged again
#if PIF_MSP_RECEIVE_TIMEOUT
	PifTimer* p_timer;
#endif
} PifMspRx;

/**
 * @brief Transmit side state of an MSP instance.
 */
typedef struct StPifMspTx
{
    PifRingBuffer answer_buffer;	///< Answers waiting to be sent
    PifMspTxState state;	///< Transmit state
	uint16_t length;	///< Length of the answer being sent
	uint16_t pos;	///< Bytes of it sent so far
} PifMspTx;

/**
 * @class StPifMsp
 * @brief MSP instance on a transport.
 */
struct StPifMsp
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;	///< Instance ID

	// Private Member Variable
	PifTransport *__p_transport;
    PifMspRx __rx;
    PifMspTx __tx;
	uint8_t __check_xor;
	uint16_t __data_size;
	PifIssuerP __p_issuer;
	void* __p_parser;

	// Private Action Function
	PifActMspParsing __act_parsing;

    // Private Event Function
	PifEvtMspReceive __evt_receive;
	PifEvtMspOtherPacket __evt_other_packet;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up an MSP instance: allocates the receive and answer buffers of PIF_MSP_RX_PACKET_SIZE and
 *        PIF_MSP_TX_ANSWER_SIZE and the receive timer. Attach a transport next.
 * @param p_owner Pointer to the protocol instance.
 * @param p_timer Timer manager used for the receive timeout. May be NULL when PIF_MSP_RECEIVE_TIMEOUT is 0.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifMsp_Init(PifMsp* p_owner, PifTimerManager* p_timer, PifId id);

/**
 * @brief Frees the buffers the instance allocated and removes its timer.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_Clear(PifMsp* p_owner);

/**
 * @brief Gives the instance a caller-owned buffer for the payload of a received packet, in place of the one
 *        pifMsp_Init() allocated. A packet that does not fit is dropped.
 * @param p_owner Pointer to the protocol instance.
 * @param size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AssignRxBuffer(PifMsp* p_owner, uint16_t size, uint8_t* p_buffer);

/**
 * @brief Gives the instance a caller-owned ring buffer for the answers, in place of the one pifMsp_Init()
 *        allocated. It has to hold a whole answer frame.
 * @param p_owner Pointer to the protocol instance.
 * @param size Buffer capacity in bytes. One byte of it stays unused, as in any PifRingBuffer.
 * @param p_buffer Caller-provided buffer memory.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AssignAnswerBuffer(PifMsp* p_owner, uint16_t size, uint8_t* p_buffer);

/**
 * @brief Attaches the protocol to a transport as its client.
 * @param p_owner Pointer to the protocol instance.
 * @param p_transport Transport that the received bytes come from and the answers go to, e.g. &uart.parent.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AttachTransport(PifMsp* p_owner, PifTransport* p_transport);

/**
 * @brief Detaches the transport attached with pifMsp_AttachTransport().
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_DetachTransport(PifMsp* p_owner);

/**
 * @brief Registers callbacks for normal MSP packets and non-MSP bytes.
 * @param p_owner Pointer to the MSP instance.
 * @param evt_receive Callback invoked when a valid MSP packet is parsed.
 * @param evt_other_packet Callback invoked for bytes that do not belong to an MSP packet.
 * @param p_issuer Issuer object forwarded to callback handlers.
 */
void pifMsp_AttachEvtReceive(PifMsp* p_owner, PifEvtMspReceive evt_receive, PifEvtMspOtherPacket evt_other_packet, PifIssuerP p_issuer);

/**
 * @brief Feeds one received byte to the parser, for a client that gets the bytes some other way than through
 *        an attached PifTransport. A completed packet goes to evt_receive before this returns, and a byte that is
 *        not part of a packet goes to evt_other_packet.
 * @param p_owner Pointer to the protocol instance.
 * @param data Received byte.
 * @return What the byte did.
 */
PifMspFrame pifMsp_ParsingPacket(PifMsp* p_owner, uint8_t data);

/**
 * @brief Reads the next byte of the payload of a received packet.
 * @param p_packet Packet given to evt_receive.
 * @return The byte.
 */
uint8_t pifMsp_ReadData8(PifMspPacket* p_packet);

/**
 * @brief Reads the next 16 bit value, low byte first, of the payload of a received packet.
 * @param p_packet Packet given to evt_receive.
 * @return The value.
 */
uint16_t pifMsp_ReadData16(PifMspPacket* p_packet);

/**
 * @brief Reads the next 32 bit value, low byte first, of the payload of a received packet.
 * @param p_packet Packet given to evt_receive.
 * @return The value.
 */
uint32_t pifMsp_ReadData32(PifMspPacket* p_packet);

/**
 * @brief Reads the next size bytes of the payload of a received packet.
 * @param p_packet Packet given to evt_receive.
 * @param p_data Where the bytes go.
 * @param size Number of bytes.
 */
void pifMsp_ReadData(PifMspPacket* p_packet, uint8_t* p_data, uint16_t size);

/**
 * @brief Starts the MSPv1 answer to a question in the answer buffer. Add its payload with the pifMsp_AddAnswer
 *        functions and send it with pifMsp_SendAnswer().
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @brief Adds a byte to the payload of the answer being made.
 * @param p_owner Pointer to the protocol instance.
 * @param data Byte to add.
 * @return TRUE on success, FALSE when the answer buffer is full. The answer is then dropped.
 */
BOOL pifMsp_AddAnswer8(PifMsp* p_owner, uint8_t data);

/**
 * @brief Adds a 16 bit value, in the byte order of the CPU (low byte first on most MCUs), to the payload of the
 *        answer being made.
 * @param p_owner Pointer to the protocol instance.
 * @param data Value to add.
 * @return TRUE on success, FALSE when the answer buffer is full. The answer is then dropped.
 */
BOOL pifMsp_AddAnswer16(PifMsp* p_owner, uint16_t data);

/**
 * @brief Adds a 32 bit value, in the byte order of the CPU (low byte first on most MCUs), to the payload of the
 *        answer being made.
 * @param p_owner Pointer to the protocol instance.
 * @param data Value to add.
 * @return TRUE on success, FALSE when the answer buffer is full. The answer is then dropped.
 */
BOOL pifMsp_AddAnswer32(PifMsp* p_owner, uint32_t data);

/**
 * @brief Adds bytes to the payload of the answer being made.
 * @param p_owner Pointer to the protocol instance.
 * @param p_data Bytes to add.
 * @param size Number of bytes.
 * @return TRUE on success, FALSE when the answer buffer is full. The answer is then dropped.
 */
BOOL pifMsp_AddAnswer(PifMsp* p_owner, uint8_t* p_data, uint16_t size);

/**
 * @brief Builds a protocol frame from the provided input.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @brief Finishes the answer being made with its size and checksum and queues it to be sent.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success, FALSE when the answer buffer is full. The answer is then dropped.
 */
BOOL pifMsp_SendAnswer(PifMsp* p_owner);

/**
 * @brief Gives the contiguous part of the queued answers, for a client that sends them itself instead of
 *        through an attached PifTransport. Remove what was sent with pifMsp_RemoveAnswer() and call again until it
 *        returns 0, since the answers may wrap around the end of the ring buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param pp_data Receives the address of the first queued byte.
 * @return Number of contiguous bytes at *pp_data, 0 when nothing is queued.
 */
uint16_t pifMsp_GetAnswer(PifMsp* p_owner, uint8_t** pp_data);

/**
 * @brief Removes bytes that pifMsp_GetAnswer() gave and that have been sent.
 * @param p_owner Pointer to the protocol instance.
 * @param length Number of bytes sent.
 */
void pifMsp_RemoveAnswer(PifMsp* p_owner, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_H
