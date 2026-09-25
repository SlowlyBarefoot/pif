#ifndef PIF_MSP_H
#define PIF_MSP_H


#include "communication/pif_uart.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer_manager.h"


// Size of the receive buffer of one packet. The MSPv1 parser of pif_msp keeps the length, command and
// checksum bytes in it too, so its payload can be 3 bytes shorter than this.
// 0: nothing is allocated in pifMsp_Init(); the client gives a buffer with pifMsp_AssignRxBuffer().
#ifndef PIF_MSP_RX_PACKET_SIZE
#define PIF_MSP_RX_PACKET_SIZE		128
#endif

// Size of the ring buffer that holds the answers waiting to be sent.
// 0: nothing is allocated in pifMsp_Init(); the client gives a buffer with pifMsp_AssignAnswerBuffer().
#ifndef PIF_MSP_TX_ANSWER_SIZE
#define PIF_MSP_TX_ANSWER_SIZE		128
#endif

// Timeout used to receive one complete packet.
// 0: no timeout limit.
// 1 or greater: multiplied by the timer unit configured in pifMsp_Init().
// Default is 200 ticks in this header; at 1 ms timer unit, that equals 200 ms.
#ifndef PIF_MSP_RECEIVE_TIMEOUT
#define PIF_MSP_RECEIVE_TIMEOUT		200
#endif

//#define __DEBUG_PACKET__


typedef enum EnPifMspRxState
{
	MRS_IDLE			= 0,
	MRS_HEADER_CHAR 	= 1,
	MRS_DIRECTION		= 2,
	MRS_LENGTH			= 3,
	MRS_MESSAGE_TYPE	= 4,
	MRS_DATA        	= 5,
	MRS_DONE			= 6
} PifMspRxState;

typedef enum EnPifMspTxState
{
	MTS_IDLE			= 0,
	MTS_SENDING			= 1
} PifMspTxState;

typedef enum EnPifMspVersion
{
	MV_V1				= 0,	// $M
	MV_V2_OVER_V1		= 1,	// $M with command 255 and an MSPv2 frame as payload
	MV_V2_NATIVE		= 2		// $X
} PifMspVersion;

typedef enum EnPifMspPacketType
{
	MPT_COMMAND			= 0,	// '<'
	MPT_REPLY			= 1		// '>'
} PifMspPacketType;

/**
 * @brief What a byte given to pifMsp_ParsingPacket() did.
 */
typedef enum EnPifMspFrame
{
	MF_NONE				= 0,	// Part of a packet that is not complete yet, or of one that was dropped.
	MF_OTHER			= 1,	// Not part of an MSP packet; it has also gone to evt_other_packet.
	MF_PACKET			= 2		// Completed a packet, which has gone to evt_receive.
} PifMspFrame;


/**
 * @class StPifMspPacket
 * @brief Represents the StPifMspPacket data structure.
 */
typedef struct StPifMspPacket
{
	uint16_t command;
	uint16_t data_count;
	uint8_t* p_data;
	uint8_t* p_pointer;
	uint8_t flags;				// MSPv2 flags, 0 for MSPv1
	PifMspVersion version;		// Frame the packet came in, which is the one its answer goes out in
	PifMspPacketType type;
} PifMspPacket;


struct StPifMsp;
typedef struct StPifMsp PifMsp;

typedef void (*PifEvtMspReceive)(PifMsp* p_owner, PifMspPacket* p_packet, PifIssuerP p_issuer);
typedef void (*PifEvtMspError)(PifId id);
typedef void (*PifEvtMspOtherPacket)(PifMsp* p_owner, uint8_t data, PifIssuerP p_issuer);

// Parser of one received byte. pif_msp has the MSPv1 one; a derived protocol replaces it.
typedef PifMspFrame (*PifActMspParsing)(void* p_parser, uint8_t data);

typedef struct StPifMspRx
{
	uint8_t state;				// PifMspRxState, or the state of the parser that replaced it. 0 is idle.
	uint8_t* p_packet;
	uint16_t packet_size;
	uint16_t packet_count;
	BOOL packet_static;
	PifMspPacket packet;
	uint8_t pre_error;
#if PIF_MSP_RECEIVE_TIMEOUT
	PifTimer* p_timer;
#endif
} PifMspRx;

typedef struct StPifMspTx
{
    PifRingBuffer answer_buffer;
    PifMspTxState state;
	uint16_t length;
	uint16_t pos;
} PifMspTx;

/**
 * @class StPifMsp
 * @brief Performs the StPifMsp operation.
 */
struct StPifMsp
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifUart* __p_uart;
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
 * @fn pifMsp_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param p_timer Timer manager used for the receive timeout. May be NULL when PIF_MSP_RECEIVE_TIMEOUT is 0.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_Init(PifMsp* p_owner, PifTimerManager* p_timer, PifId id);

/**
 * @fn pifMsp_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_Clear(PifMsp* p_owner);

/**
 * @fn pifMsp_AssignRxBuffer
 * @brief Gives the instance a caller-owned buffer for the payload of a received packet, in place of the one
 *        pifMsp_Init() allocated. A packet that does not fit is dropped.
 * @param p_owner Pointer to the protocol instance.
 * @param size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AssignRxBuffer(PifMsp* p_owner, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifMsp_AssignAnswerBuffer
 * @brief Gives the instance a caller-owned ring buffer for the answers, in place of the one pifMsp_Init()
 *        allocated. It has to hold a whole answer frame.
 * @param p_owner Pointer to the protocol instance.
 * @param size Buffer capacity in bytes. One byte of it stays unused, as in any PifRingBuffer.
 * @param p_buffer Caller-provided buffer memory.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AssignAnswerBuffer(PifMsp* p_owner, uint16_t size, uint8_t* p_buffer);

/**
 * @fn pifMsp_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifMsp_AttachUart(PifMsp* p_owner, PifUart* p_uart);

/**
 * @fn pifMsp_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_DetachUart(PifMsp* p_owner);

/**
 * @fn pifMsp_AttachEvtReceive
 * @brief Registers callbacks for normal MSP packets and non-MSP bytes.
 * @param p_owner Pointer to the MSP instance.
 * @param evt_receive Callback invoked when a valid MSP packet is parsed.
 * @param evt_other_packet Callback invoked for bytes that do not belong to an MSP packet.
 * @param p_issuer Issuer object forwarded to callback handlers.
 */
void pifMsp_AttachEvtReceive(PifMsp* p_owner, PifEvtMspReceive evt_receive, PifEvtMspOtherPacket evt_other_packet, PifIssuerP p_issuer);

/**
 * @fn pifMsp_ParsingPacket
 * @brief Feeds one received byte to the parser, for a client that gets the bytes some other way than through
 *        an attached PifUart. A completed packet goes to evt_receive before this returns, and a byte that is
 *        not part of a packet goes to evt_other_packet.
 * @param p_owner Pointer to the protocol instance.
 * @param data Received byte.
 * @return What the byte did.
 */
PifMspFrame pifMsp_ParsingPacket(PifMsp* p_owner, uint8_t data);

/**
 * @fn pifMsp_ReadData8
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 8-bit value.
 */
uint8_t pifMsp_ReadData8(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData16
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 16-bit value.
 */
uint16_t pifMsp_ReadData16(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData32
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 32-bit value.
 */
uint32_t pifMsp_ReadData32(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @param p_data Pointer to a payload buffer.
 * @param size Number of bytes to process.
 */
void pifMsp_ReadData(PifMspPacket* p_packet, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeAnswer
 * @brief Builds a protocol frame from the provided input.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_AddAnswer8
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer8(PifMsp* p_owner, uint8_t data);

/**
 * @fn pifMsp_AddAnswer16
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer16(PifMsp* p_owner, uint16_t data);

/**
 * @fn pifMsp_AddAnswer32
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer32(PifMsp* p_owner, uint32_t data);

/**
 * @fn pifMsp_AddAnswer
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param p_data Pointer to a payload buffer.
 * @param size Number of bytes to process.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer(PifMsp* p_owner, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeError
 * @brief Builds a protocol frame from the provided input.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_SendAnswer
 * @brief Sends or queues a protocol frame.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_SendAnswer(PifMsp* p_owner);

/**
 * @fn pifMsp_GetAnswer
 * @brief Gives the contiguous part of the queued answers, for a client that sends them itself instead of
 *        through an attached PifUart. Remove what was sent with pifMsp_RemoveAnswer() and call again until it
 *        returns 0, since the answers may wrap around the end of the ring buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param pp_data Receives the address of the first queued byte.
 * @return Number of contiguous bytes at *pp_data, 0 when nothing is queued.
 */
uint16_t pifMsp_GetAnswer(PifMsp* p_owner, uint8_t** pp_data);

/**
 * @fn pifMsp_RemoveAnswer
 * @brief Removes bytes that pifMsp_GetAnswer() gave and that have been sent.
 * @param p_owner Pointer to the protocol instance.
 * @param length Number of bytes sent.
 */
void pifMsp_RemoveAnswer(PifMsp* p_owner, uint16_t length);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_H
