/**
 * @file pif_link.h
 * @brief Request/answer link protocol over a PifUart, point-to-point or on a shared bus.
 *
 * Every frame is made of bytes of 0x20 or above except STX, ETX and DLE, so a receiver can always find
 * the start of the next frame and the stream can be dumped as text.
 *
 * Frame layout:
 * @code
 * STX | flags | command | packet_id | [src_id | dst_id] | length | data | CRC (3 bytes) | ETX | [padding]
 * @endcode
 * src_id and dst_id are present only in PLT_MULTI. See "Frame header" for the byte positions.
 * - flags: LF_* bits, always with LF_ALWAYS set.
 * - command: 0x20 or above.
 * - packet_id: 0x20 to 0xFF, incremented for every request and copied into its answer.
 * - src_id, dst_id: addresses of the sender and the receiver, sent as 0x80 | address.
 * - length: 2 bytes of 7 bits each with bit 7 set, low byte first. Data size is 0 to 16383.
 * - data: a byte below 0x20 is sent as DLE followed by the byte with bit 7 set.
 * - CRC: CRC-16/CCITT-FALSE over everything from flags to the end of the escaped data, sent as
 *   3 bytes of 7, 7 and 2 bits, LSB first, each with bit 7 set.
 * - padding: zeros up to a multiple of the UART frame size when it is above 1.
 *
 * A single ACK byte is accepted in place of a response frame and a NAK byte makes the sender retry.
 * A receiver sends NAK when it drops a broken frame while it has no request of its own in progress.
 * ACK and NAK carry no address, so PLT_MULTI neither sends nor accepts them and relies on the request timeout.
 *
 * Addressing (PLT_MULTI): each node has an address from 0x00 to PIF_LINK_MAX_ADDRESS. A node handles only the
 * frames sent to its own address or to PIF_LINK_BROADCAST and drops the others after their header. An answer
 * goes back to the src_id of its question. A broadcast request is never answered, so it is always sent with
 * LF_RESPONSE_NO. The link does not arbitrate the bus: the product has to keep two nodes from sending at the
 * same time, for example with a single master that polls the others.
 *
 * Retransmission: a request sent again carries LF_RETRY. When the receiver gets a question with LF_RETRY and
 * the same command, packet_id and src_id as the last question it handled, the question is not
 * handled again. The answer already made for it is sent again instead, so a lost answer never makes the
 * request run twice.
 *
 * Errors: an answer with LF_ERROR carries one PifLinkError code as its data. The link answers a question it
 * has no handler for with LE_UNSUPPORTED, and a handler can answer with pifLink_MakeError().
 *
 * Payload encoding: the link carries the data as bytes. Products built on it encode the data with these rules
 * so both ends agree:
 * - Fields are packed in order with no padding.
 * - Integers are little-endian; signed integers are two's complement of the same width.
 * - float is IEEE 754 single precision, little-endian.
 * - A string is one length byte followed by its characters, with no terminating NUL.
 * pifLink_PutU16(), pifLink_PutU32(), pifLink_PutFloat() and their Get pairs follow these rules.
 */
#ifndef PIF_LINK_H
#define PIF_LINK_H


#include "communication/pif_uart.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer_manager.h"


// Largest data size of one received packet. The buffer allocated in pifLink_Init() is
// PIF_LINK_MULTI_HEADER_SIZE + 3 bytes larger to also hold the header and CRC. It can be changed later with pifLink_ResizeRxPacket().
#ifndef PIF_LINK_RX_PACKET_SIZE
#define PIF_LINK_RX_PACKET_SIZE		32
#endif

// Size of the ring buffer that holds the requests waiting to be sent or answered.
// Each request takes its encoded frame plus 2 + sizeof(void*) bytes of bookkeeping.
#ifndef PIF_LINK_TX_REQUEST_SIZE
#define PIF_LINK_TX_REQUEST_SIZE	64
#endif

// Size of the ring buffer that holds the answers and NAKs waiting to be sent.
#ifndef PIF_LINK_TX_ANSWER_SIZE
#define PIF_LINK_TX_ANSWER_SIZE		32
#endif

// Timeout used to receive one complete packet.
// 0: no timeout limit.
// 1 or greater: multiplied by the timer unit of the timer manager given to pifLink_Init().
//               Default is 50; at 1 ms timer unit, that equals 50 ms.
#ifndef PIF_LINK_RECEIVE_TIMEOUT
#define PIF_LINK_RECEIVE_TIMEOUT	50
#endif

// Delay before a request is sent again after a NAK or a broken response.
// 0: retry at once.
// 1 or greater: multiplied by the timer unit of the timer manager given to pifLink_Init().
//               Default is 10; at 1 ms timer unit, that equals 10 ms.
#ifndef PIF_LINK_RETRY_DELAY
#define PIF_LINK_RETRY_DELAY		10
#endif

// Define to log the header of every packet sent and received.
//#define __DEBUG_PACKET__


/**
 * @brief Link type. All nodes on a link must use the same one.
 */
typedef enum EnPifLinkType
{
	PLT_SINGLE			= 1,	// Point-to-point. 6-byte header without addresses.
	PLT_MULTI			= 2		// Shared bus. 8-byte header with src_id and dst_id.
} PifLinkType;

/**
 * @name Frame header
 * @brief Layout of the header that starts every frame.
 *
 * PLT_SINGLE (6 bytes):
 * @code
 * [0] STX        0x02
 * [1] flags      LF_* bits | LF_ALWAYS
 * [2] command    0x20 to 0xFF
 * [3] packet_id  0x20 to 0xFF, incremented per request and copied into its answer
 * [4] length L   0x80 | (data size & 0x7F)
 * [5] length H   0x80 | ((data size >> 7) & 0x7F)   data size: 0 to 16383
 * @endcode
 *
 * PLT_MULTI (8 bytes):
 * @code
 * [0] STX        0x02
 * [1] flags      LF_* bits | LF_ALWAYS
 * [2] command    0x20 to 0xFF
 * [3] packet_id  0x20 to 0xFF, incremented per request and copied into its answer
 * [4] src_id     0x80 | sender address
 * [5] dst_id     0x80 | receiver address, or 0x80 | PIF_LINK_BROADCAST
 * [6] length L   0x80 | (data size & 0x7F)
 * [7] length H   0x80 | ((data size >> 7) & 0x7F)   data size: 0 to 16383
 * @endcode
 *
 * Both are followed by the escaped data, the 3-byte CRC and ETX.
 * @{
 */
#define PIF_LINK_SINGLE_HEADER_SIZE	6
#define PIF_LINK_MULTI_HEADER_SIZE	8
#define PIF_LINK_MAX_DATA_SIZE		0x3FFF

#define PIF_LINK_MAX_ADDRESS		0x7E	// Largest node address.
#define PIF_LINK_BROADCAST			0x7F	// dst_id that every node handles.
/** @} */

/**
 * @name Frame flags
 * @brief Bits of the flags byte of a frame.
 *
 * A request sent by pifLink_MakeRequest() arrives at the other end as a question, so REQUEST and QUESTION
 * share a value, as do RESPONSE and ANSWER.
 * @{
 */
#define LF_DEFAULT			0x00

// Kind of the frame.
#define LF_TYPE_MASK		0x01
#define LF_TYPE_REQUEST		0x00	// Default
#define LF_TYPE_RESPONSE	0x01
#define LF_TYPE_QUESTION	0x00	// Default
#define LF_TYPE_ANSWER		0x01

// Whether the sender of a request waits for a response.
#define LF_RESPONSE_MASK	0x02
#define LF_RESPONSE_YES		0x00	// Default
#define LF_RESPONSE_NO		0x02

// The same bit seen by the receiver: whether the question expects an answer.
#define LF_ANSWER_MASK		0x02
#define LF_ANSWER_YES		0x00	// Default
#define LF_ANSWER_NO		0x02

// Whether the frame is logged when it is sent or received.
#define LF_LOG_PRINT_MASK	0x08
#define LF_LOG_PRINT_NO		0x00	// Default
#define LF_LOG_PRINT_YES	0x08

// Set by the link on a request sent again after the first attempt.
#define LF_RETRY			0x04

// Set on an answer whose data is one PifLinkError code instead of the answer data.
#define LF_ERROR			0x20

// 0x10 and 0x40 are reserved.

// Always set in a frame so the flags byte is 0x20 or above.
#define LF_ALWAYS			0x80
/** @} */

/**
 * @brief Reason a request failed, passed to PifEvtLinkError.
 *
 * Codes below 0x80 travel in an LF_ERROR answer. Codes from 0x80 are found by the sender itself.
 */
typedef enum EnPifLinkError
{
	LE_NONE				= 0x00,

	// Sent by the receiver of a question.
	LE_UNSUPPORTED		= 0x01,	// No handler for the command.
	LE_INVALID_PARAM	= 0x02,	// The data of the question is not valid.
	LE_BUSY				= 0x03,	// The question cannot be handled now; it may be asked again later.
	LE_FAILED			= 0x04,	// The question was handled but failed.
	LE_USER				= 0x40,	// 0x40 to 0x7F are free for the application.

	// Found by the sender of a request.
	LE_TIMEOUT			= 0x80,	// No response after all retries.
	LE_NO_TIMER			= 0x81	// The response timer could not be started.
} PifLinkError;

/**
 * @brief States of the receive parser.
 */
typedef enum EnPifLinkRxState
{
	PRS_IDLE			= 0,	// Waiting for STX, ACK or NAK.
	PRS_GET_HEADER		= 1,
	PRS_GET_DATA		= 2,
	PRS_GET_CRC			= 3,
	PRS_GET_TAILER		= 4,	// Waiting for ETX.
	PRS_DONE			= 5,	// A complete frame is in the buffer.
	PRS_ACK				= 6,	// An ACK was received for the request in progress.
	PRS_ERROR			= 7
} PifLinkRxState;

/**
 * @brief States of the request sender.
 */
typedef enum EnPifLinkTxState
{
	PTS_IDLE			= 0,	// No request in progress.
	PTS_SENDING			= 1,	// Writing the request frame to the UART.
	PTS_WAIT_SENDED		= 2,	// The whole frame is handed to the UART.
	PTS_WAIT_RESPONSE	= 3,	// Waiting for the response, ACK or NAK until the request timeout.
	PTS_RETRY_DELAY		= 4,	// Waiting PIF_LINK_RETRY_DELAY before the next attempt.
	PTS_RETRY			= 5		// The next attempt is due.
} PifLinkTxState;


/**
 * @class StPifLinkPacket
 * @brief A received frame as passed to PifEvtLinkFinish.
 *
 * p_data points into the receive buffer and is valid only during the callback.
 */
typedef struct StPifLinkPacket
{
	uint8_t flags;			// LF_* bits as received. LF_ANSWER_MASK tells whether the question expects an answer.
	uint8_t command;		// Command byte.
	uint16_t length;		// Data size given in the header.
	uint16_t crc;			// Received CRC-16/CCITT-FALSE.
	uint8_t packet_id;		// Identifier of the request, copied into its answer.
	uint8_t src_id;			// PLT_MULTI: address of the sender. 0 for PLT_SINGLE.
	uint8_t dst_id;			// PLT_MULTI: own address or PIF_LINK_BROADCAST. 0 for PLT_SINGLE.
	uint8_t* p_data;		// Unescaped data, or NULL when length is 0.
	uint16_t data_count;	// Number of data bytes received.
} PifLinkPacket;

struct StPifLink;
typedef struct StPifLink PifLink;

struct StPifLinkRequest;
typedef struct StPifLinkRequest PifLinkRequest;

/**
 * @brief Called when a response arrives for a request or a question arrives.
 * @param p_owner Link that received the frame. pifLink_MakeAnswer() can be called with it.
 * @param p_packet Received frame, or NULL when the request was acknowledged with a single ACK.
 */
typedef void (*PifEvtLinkFinish)(PifLink* p_owner, PifLinkPacket* p_packet);

/**
 * @brief Called when a request fails. The request is removed from the queue before the call.
 * @param p_owner Link that sent the request.
 * @param p_request Request that failed.
 * @param error Reason of the failure. Below 0x80 it is the code the other end answered with.
 */
typedef void (*PifEvtLinkError)(PifLink* p_owner, const PifLinkRequest* p_request, PifLinkError error);

/**
 * @class StPifLinkRequest
 * @brief Describes a request sent with pifLink_MakeRequest().
 *
 * The link keeps a pointer to it until the request is finished, so it must stay valid until then
 * (usually a static const object).
 */
struct StPifLinkRequest
{
    uint8_t command;				// Command byte, 0x20 or above.
    uint8_t flags;					// LF_RESPONSE_* and LF_LOG_PRINT_* bits. Other bits are ignored.
    PifEvtLinkFinish evt_response;	// Called with the response. Can be NULL.
    uint8_t retry;					// Number of times the request is sent again after the first attempt.
    uint16_t timeout;				// Time to wait for the response per attempt, in timer ticks.
};

/**
 * @class StPifLinkQuestion
 * @brief One entry of the question table given to pifLink_Init().
 *
 * The table ends with an entry whose command is 0.
 */
typedef struct StPifLinkQuestion
{
    uint8_t command;				// Command byte, 0x20 or above.
    uint8_t flags;					// Reserved. Not used by the link.
    PifEvtLinkFinish evt_answer;	// Called with the question. Can be NULL.
} PifLinkQuestion;

/**
 * @class StPifLinkRx
 * @brief Receive side state of a link.
 */
typedef struct StPifLinkRx
{
	PifLinkRxState state;
	uint8_t* p_packet;				// Header, data and CRC of the frame being received.
	uint16_t packet_size;			// Size of p_packet.
	uint8_t header_count;
	uint8_t crc_count;
	uint16_t crc;					// CRC calculated over the bytes received so far.
	BOOL data_link_escape;			// The previous data byte was DLE.
	PifLinkPacket packet;
#if PIF_LINK_RECEIVE_TIMEOUT
	PifTimer* p_timer;
#endif
} PifLinkRx;

/**
 * @class StPifLinkLastQuestion
 * @brief The last question handled and the answer made for it, kept to answer a retransmission.
 */
typedef struct StPifLinkLastQuestion
{
	BOOL valid;
	uint8_t command;
	uint8_t packet_id;
	uint8_t src_id;
	uint8_t* p_answer;				// Copy of the answer frame.
	uint16_t answer_size;			// Size of p_answer.
	uint16_t answer_length;			// Length of the answer frame, 0 while it is not made yet.
} PifLinkLastQuestion;

/**
 * @class StPifLinkTx
 * @brief Transmit side state of a link.
 */
typedef struct StPifLinkTx
{
    PifRingBuffer request_buffer;	// Queued requests: [frame length (2)][CRC position (2)][PifLinkRequest*][frame].
    PifRingBuffer answer_buffer;	// Queued answer frames and NAK bytes.
	const PifLinkRequest* p_request;	// Request in progress.
	PifLinkTxState state;
	uint16_t length;				// Frame length of the request in progress.
	uint16_t pos;					// Send position in the request buffer.
	uint8_t retry;					// Remaining retries of the request in progress.
	uint8_t packet_id;				// packet_id of the request in progress.
	uint8_t flags;					// flags byte of the request in progress.
	uint8_t dst_id;					// dst_id of the request in progress.
	uint16_t crc_pos;				// Position of the CRC in the frame of the request in progress.
	uint16_t answer_length;			// Bytes of the answer buffer being sent, 0 when idle.
	uint16_t answer_pos;
	PifTimer* p_timer;				// Response timeout and retry delay.
} PifLinkTx;

/**
 * @class StPifLink
 * @brief Point-to-point link over a PifUart.
 */
struct StPifLink
{
	// Public Member Variable

	// Public Event Function
    PifEvtLinkError evt_error;

	// Read-only Member Variable
    PifId _id;
    PifLinkType _type;
    uint8_t _address;				// Own address for PLT_MULTI.

	// Private Member Variable
    PifTimerManager* __p_timer_manager;
	PifUart* __p_uart;
    const PifLinkQuestion* __p_questions;
    PifLinkRx __rx;
    PifLinkTx __tx;
    PifLinkLastQuestion __last;
	uint8_t __header_size;
	uint8_t __packet_id;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifLink_Init
 * @brief Initializes a link and allocates its buffers and timers.
 * @param p_owner Pointer to the link to initialize.
 * @param id Identifier of the link, or PIF_ID_AUTO.
 * @param p_timer_manager Timer manager that provides the receive and response timers. Its tick is the unit
 *        of PIF_LINK_RECEIVE_TIMEOUT, PIF_LINK_RETRY_DELAY and PifLinkRequest.timeout.
 * @param type Link type.
 * @param address Own address, 0x00 to PIF_LINK_MAX_ADDRESS, for PLT_MULTI. Ignored for PLT_SINGLE.
 * @param p_questions Question table ending with a command of 0. It must stay valid while the link is used.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLink_Init(PifLink* p_owner, PifId id, PifTimerManager* p_timer_manager, PifLinkType type, uint8_t address,
		const PifLinkQuestion* p_questions);

/**
 * @fn pifLink_Clear
 * @brief Frees the buffers and timers of a link.
 * @param p_owner Pointer to the link to clear.
 */
void pifLink_Clear(PifLink* p_owner);

/**
 * @fn pifLink_ResizeRxPacket
 * @brief Changes the largest data size of a received packet. A packet being received is dropped.
 * @param p_owner Pointer to the link.
 * @param rx_packet_size New largest data size, 1 to PIF_LINK_MAX_DATA_SIZE (16383).
 * @return TRUE on success, FALSE with pif_error set otherwise. The old buffer is kept on failure.
 */
BOOL pifLink_ResizeRxPacket(PifLink* p_owner, uint16_t rx_packet_size);

/**
 * @fn pifLink_ResizeTxRequest
 * @brief Changes the size of the request buffer. Queued requests are discarded.
 * @param p_owner Pointer to the link.
 * @param tx_request_size New size in bytes.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLink_ResizeTxRequest(PifLink* p_owner, uint16_t tx_request_size);

/**
 * @fn pifLink_ResizeTxResponse
 * @brief Changes the size of the answer buffer and of the copy of the last answer.
 *        Queued answers and the copy are discarded.
 * @param p_owner Pointer to the link.
 * @param tx_response_size New size in bytes.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifLink_ResizeTxResponse(PifLink* p_owner, uint16_t tx_response_size);

/**
 * @fn pifLink_AttachUart
 * @brief Attaches the link to a UART as its client. It must be called before any frame is made.
 * @param p_owner Pointer to the link.
 * @param p_uart UART to send and receive with.
 */
void pifLink_AttachUart(PifLink* p_owner, PifUart* p_uart);

/**
 * @fn pifLink_DetachUart
 * @brief Detaches the link from its UART.
 * @param p_owner Pointer to the link.
 */
void pifLink_DetachUart(PifLink* p_owner);

/**
 * @fn pifLink_MakeRequest
 * @brief Encodes a request and queues it. Queued requests are sent one at a time, each after the previous
 *        one is finished.
 * @param p_owner Pointer to the link.
 * @param dst_id PLT_MULTI: address of the receiver, or PIF_LINK_BROADCAST to send without waiting for a
 *        response. Ignored for PLT_SINGLE.
 * @param p_request Request to send. The link keeps this pointer until the request is finished.
 * @param p_data Data to send. It is copied, so it can be reused after the call. Can be NULL when data_size is 0.
 * @param data_size Data size, up to PIF_LINK_MAX_DATA_SIZE (16383).
 * @return TRUE when queued, FALSE with pif_error set when a parameter is invalid, no UART is attached or the
 *         request buffer is full.
 */
BOOL pifLink_MakeRequest(PifLink* p_owner, uint8_t dst_id, const PifLinkRequest* p_request, uint8_t* p_data,
		uint16_t data_size);

/**
 * @fn pifLink_MakeAnswer
 * @brief Encodes an answer to a received question and queues it. It is usually called from evt_answer.
 * @param p_owner Pointer to the link.
 * @param p_question Question to answer. Its command and packet_id are copied into the answer, which goes to
 *        its src_id. A broadcast question cannot be answered.
 * @param flags LF_LOG_PRINT_* bits. Other bits are set by the link.
 * @param p_data Data to send. It is copied, so it can be reused after the call. Can be NULL when data_size is 0.
 * @param data_size Data size, up to PIF_LINK_MAX_DATA_SIZE (16383).
 * @return TRUE when queued, FALSE with pif_error set when a parameter is invalid, the question was broadcast,
 *         no UART is attached or the answer buffer is full.
 */
BOOL pifLink_MakeAnswer(PifLink* p_owner, PifLinkPacket* p_question, uint8_t flags,
		uint8_t *p_data, uint16_t data_size);

/**
 * @fn pifLink_MakeError
 * @brief Encodes an LF_ERROR answer to a received question and queues it. The sender gets the code through
 *        its evt_error instead of evt_response.
 * @param p_owner Pointer to the link.
 * @param p_question Question to answer. Its command and packet_id are copied into the answer, which goes to
 *        its src_id. A broadcast question cannot be answered.
 * @param flags LF_LOG_PRINT_* bits. Other bits are set by the link.
 * @param error Error code, 0x01 to 0x7F.
 * @return TRUE when queued, FALSE with pif_error set when a parameter is invalid, the question was broadcast,
 *         no UART is attached or the answer buffer is full.
 */
BOOL pifLink_MakeError(PifLink* p_owner, PifLinkPacket* p_question, uint8_t flags, PifLinkError error);

/**
 * @fn pifLink_PutU16
 * @brief Writes a 16-bit value in the payload byte order (little-endian).
 * @param p_buffer Destination of 2 bytes.
 * @param value Value to write.
 * @return Pointer just after the written bytes.
 */
uint8_t* pifLink_PutU16(uint8_t* p_buffer, uint16_t value);

/**
 * @fn pifLink_PutU32
 * @brief Writes a 32-bit value in the payload byte order (little-endian).
 * @param p_buffer Destination of 4 bytes.
 * @param value Value to write.
 * @return Pointer just after the written bytes.
 */
uint8_t* pifLink_PutU32(uint8_t* p_buffer, uint32_t value);

/**
 * @fn pifLink_PutFloat
 * @brief Writes an IEEE 754 single precision value in the payload byte order (little-endian).
 * @param p_buffer Destination of 4 bytes.
 * @param value Value to write.
 * @return Pointer just after the written bytes.
 */
uint8_t* pifLink_PutFloat(uint8_t* p_buffer, float value);

/**
 * @fn pifLink_GetU16
 * @brief Reads a 16-bit value in the payload byte order (little-endian).
 * @param p_buffer Source of 2 bytes.
 * @return Value read.
 */
uint16_t pifLink_GetU16(const uint8_t* p_buffer);

/**
 * @fn pifLink_GetU32
 * @brief Reads a 32-bit value in the payload byte order (little-endian).
 * @param p_buffer Source of 4 bytes.
 * @return Value read.
 */
uint32_t pifLink_GetU32(const uint8_t* p_buffer);

/**
 * @fn pifLink_GetFloat
 * @brief Reads an IEEE 754 single precision value in the payload byte order (little-endian).
 * @param p_buffer Source of 4 bytes.
 * @return Value read.
 */
float pifLink_GetFloat(const uint8_t* p_buffer);

#ifdef __cplusplus
}
#endif


#endif  // PIF_LINK_H
