// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_xmodem.h
 * @brief XMODEM file transfer over a PifTransport, as the sender or the receiver.
 *
 * Supports the original XMODEM with a checksum and XMODEM-CRC, with 128 byte packets. The application gives the
 * data packet by packet and gets it back the same way through the events.
 */
#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "communication/pif_transport.h"
#include "communication/pif_uart.h"
#include "core/pif_task_manager.h"


/**
 * @brief Variant of XMODEM.
 */
typedef enum EnPifXmodemType
{
	XT_ORIGINAL			= 1,	///< 8 bit checksum, started with NAK
	XT_CRC				= 2	///< CRC-16, started with 'C'
} PifXmodemType;

/**
 * @brief Receive state.
 */
typedef enum EnPifXmodemRxState
{
	XRS_IDLE			= 0,			///< Nothing being received
	XRS_C				= 'C',			///< A start request came
	XRS_GET_HEADER		= 10,			///< Receiving the header of a packet
	XRS_GET_DATA		= 11,			///< Receiving the data of a packet
	XRS_GET_CRC			= 12,			///< Receiving the checksum or CRC
	XRS_PURGE			= 13,			///< Bad packet: waiting for the line to go quiet before NAK
	XRS_SOH				= ASCII_SOH,	///< 1
	XRS_EOT				= ASCII_EOT,	///< 4
	XRS_CAN				= ASCII_CAN		///< 24
} PifXmodemRxState;

/**
 * @brief Transmit state.
 */
typedef enum EnPifXmodemTxState
{
	XTS_IDLE			= 0,			///< Nothing being sent
	XTS_SEND_C			= 10,			///< Sending the start request
	XTS_WAIT_START		= 11,			///< Start request sent: sent again unless a packet comes
	XTS_SENDING			= 12,			///< Sending a packet
	XTS_WAIT_RESPONSE	= 13,			///< Waiting for ACK or NAK
	XTS_EOT				= ASCII_EOT,	///< 4
	XTS_ACK				= ASCII_ACK,	///< 6
	XTS_NAK				= ASCII_NAK,	///< 21
	XTS_CAN				= ASCII_CAN		///< 24
} PifXmodemTxState;


/**
 * @class StPifXmodemPacket
 * @brief One packet of a transfer.
 */
typedef struct StPifXmodemPacket
{
	uint8_t packet_no[2];	///< Packet number and its complement
	uint8_t* p_data;	///< 128 bytes of data
} PifXmodemPacket;


/**
 * @brief Sender event.
 * @details code is one of:
 *          'C'       The receiver asks to start ('C' in XT_CRC, NAK in XT_ORIGINAL). packet_no is 0.
 *          ASCII_ACK The packet packet_no was received.
 *          ASCII_NAK The packet packet_no was rejected or not answered in time: send it again.
 *          ASCII_CAN The receiver cancelled, or the EOT was not acknowledged after
 *                    PIF_XMODEM_EOT_RETRY retries. The transfer is over.
 *          A NAK or no answer after pifXmodem_SendEot() is not reported: the EOT is sent again.
 */
typedef void (*PifEvtXmodemTxReceive)(uint8_t code, uint8_t packet_no);

/**
 * @brief Receiver event.
 * @details code is one of:
 *          ASCII_SOH A new packet is in p_packet. Its data is overwritten by the next packet.
 *                    A packet sent again because the ACK was lost is acknowledged but not passed on.
 *          ASCII_EOT The sender finished. p_packet is NULL.
 *          ASCII_CAN The sender cancelled, or a packet came out of sequence and the transfer was
 *                    cancelled. p_packet is NULL.
 */
typedef void (*PifEvtXmodemRxReceive)(uint8_t code, PifXmodemPacket* p_packet);

/**
 * @brief Called when the transfer has finished successfully.
 * @param delay Time in us until the last byte (the receiver's ACK) has left the transport, 0 on the sender.
 */
typedef void (*PifEvtXmodemFinish)(uint16_t delay);


/**
 * @class StPifXmodemTx
 * @brief Sender side state.
 */
typedef struct StPifXmodemTx
{
	PifXmodemTxState state;			///< Transmit state
	uint16_t data_pos;				///< Bytes of the packet sent so far
	uint16_t timeout_ms;			///< Time in ms to wait for an answer
	uint8_t ctrl;					///< Control byte being sent, kept here as the transport may send from the pointer later
	uint8_t retry;					///< EOT retries left
	PifTaskTimeout timeout;			///< Timeout of the answer
	PifEvtXmodemTxReceive evt_receive;	///< Called for each answer of the receiver. May be NULL.
	PifEvtXmodemFinish evt_finish;	///< Called when the transfer is over. May be NULL.
} PifXmodemTx;

/**
 * @class StPifXmodemRx
 * @brief Receiver side state.
 */
typedef struct StPifXmodemRx
{
	PifXmodemRxState state;			///< Receive state
	PifXmodemPacket packet;			///< Packet being received
	uint16_t count;					///< Bytes of the packet received so far
	uint16_t crc;					///< CRC or checksum calculated so far
	uint16_t timeout_ms;			///< Time in ms to wait for the next byte
	uint8_t last_packet_no;			///< Number of the last packet passed on, 0 before the first
	PifTaskTimeout timeout;			///< Timeout of the next byte
	PifEvtXmodemRxReceive evt_receive;	///< Called for each packet, the end or a cancel. May be NULL.
	PifEvtXmodemFinish evt_finish;	///< Called when the transfer is over. May be NULL.
} PifXmodemRx;

/**
 * @class StPifXmodem
 * @brief XMODEM instance on a transport.
 */
typedef struct StPifXmodem
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;						///< Instance ID

	// Private Member Variable
	PifTask* __p_task;				///< Checks the timeouts
	PifTransport *__p_transport;
	PifXmodemType __type;
	uint16_t __packet_size;
	PifXmodemTx __tx;
	PifXmodemRx __rx;
    uint8_t* __p_data;
    BOOL __finish;
} PifXmodem;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sets up an XMODEM instance of a variant: allocates its packet buffer and adds its timeout task. Attach
 *        a transport and the events of the side it plays next.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param type XT_ORIGINAL or XT_CRC.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifXmodemType type);

/**
 * @brief Detaches the transport if attached, removes the task and frees the packet buffer.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_Clear(PifXmodem* p_owner);

/**
 * @brief Changes how long the sender waits for an answer.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Time in ms to wait for the answer to a packet or EOT. The time to send
 *                         the packet on the transport is added to it.
 */
void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout);

/**
 * @brief Changes how long the receiver allows for a packet.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Time in ms allowed to receive a packet. The time to receive the packet
 *                        on the transport is added to it.
 */
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout);

/**
 * @brief Attaches the protocol to a transport as its client.
 * @param p_owner Pointer to the XMODEM protocol instance.
 * @param p_transport Transport to send and receive with.
 * @return TRUE on success, FALSE with E_INVALID_PARAM otherwise.
 */
BOOL pifXmodem_AttachTransport(PifXmodem *p_owner, PifTransport *p_transport);

/**
 * @brief Detaches the transport and stops any transfer in progress.
 * @param p_owner Pointer to the XMODEM protocol instance.
 */
void pifXmodem_DetachTransport(PifXmodem *p_owner);

/**
 * @brief Sets the events of the sender.
 * @param p_owner Pointer to the protocol instance.
 * @param evt_tx_receive Gets each answer of the receiver. Required to send.
 * @param evt_finish Called when the transfer has finished. May be NULL.
 */
void pifXmodem_AttachEvtTx(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @brief Sets the events of the receiver.
 * @param p_owner Pointer to the protocol instance.
 * @param evt_rx_receive Gets each packet, the end and a cancel.
 * @param evt_finish Called when the transfer has finished. May be NULL.
 */
void pifXmodem_AttachEvtRx(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @brief Sends one packet of up to 128 bytes, padded with SUB (0x1A), and waits for its answer, which goes to
 *        evt_tx_receive.
 * @param p_owner Pointer to the protocol instance.
 * @param packet_no Number of the packet, 1 for the first, wrapping at 255.
 * @param p_data Data of the packet.
 * @param data_size Bytes of data, 1 to 128.
 * @return TRUE when the packet is on its way, FALSE with pif_error set otherwise.
 */
BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size);

/**
 * @brief Ends the transfer. It is sent again on a NAK or no answer, up to PIF_XMODEM_EOT_RETRY
 *        times. The finish event follows the receiver's ACK.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_SendEot(PifXmodem* p_owner);

/**
 * @brief Cancels the transfer in progress by sending CAN twice. No answer is expected.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_SendCancel(PifXmodem* p_owner);

/**
 * @brief Starts receiving: the start request ('C' in XT_CRC, NAK in XT_ORIGINAL) is sent every
 *        PIF_XMODEM_START_INTERVAL ms until the first packet comes.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_ReadyReceive(PifXmodem* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
