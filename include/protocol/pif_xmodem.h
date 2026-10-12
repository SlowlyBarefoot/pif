// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "communication/pif_uart.h"
#include "core/pif_task_manager.h"


typedef enum EnPifXmodemType
{
	XT_ORIGINAL			= 1,
	XT_CRC				= 2
} PifXmodemType;

typedef enum EnPifXmodemRxState
{
	XRS_IDLE			= 0,
	XRS_C				= 'C',
	XRS_GET_HEADER		= 10,
	XRS_GET_DATA		= 11,
	XRS_GET_CRC			= 12,
	XRS_PURGE			= 13,			// Bad packet: waiting for the line to go quiet before NAK
	XRS_SOH				= ASCII_SOH,	// 1
	XRS_EOT				= ASCII_EOT,	// 4
	XRS_CAN				= ASCII_CAN		// 24
} PifXmodemRxState;

typedef enum EnPifXmodemTxState
{
	XTS_IDLE			= 0,
	XTS_SEND_C			= 10,
	XTS_WAIT_START		= 11,			// Start request sent: sent again unless a packet comes
	XTS_SENDING			= 12,
	XTS_WAIT_RESPONSE	= 13,
	XTS_EOT				= ASCII_EOT,	// 4
	XTS_ACK				= ASCII_ACK,	// 6
	XTS_NAK				= ASCII_NAK,	// 21
	XTS_CAN				= ASCII_CAN		// 24
} PifXmodemTxState;


/**
 * @class StPifXmodemTimeout
 * @brief A timeout checked by the protocol task. time is 0 while it is not running.
 */
typedef struct StPifXmodemTimeout
{
	uint32_t start;					// pif_act_timer1us() reading when it started
	uint32_t time;					// Length in us
} PifXmodemTimeout;

/**
 * @class StPifXmodemPacket
 * @brief Represents the StPifXmodemPacket data structure.
 */
typedef struct StPifXmodemPacket
{
	uint8_t packet_no[2];
	uint8_t* p_data;
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
 * @param delay Time in us until the last byte (the receiver's ACK) has left the UART, 0 on the sender.
 */
typedef void (*PifEvtXmodemFinish)(uint16_t delay);


/**
 * @class StPifXmodemTx
 * @brief Represents the StPifXmodemTx data structure.
 */
typedef struct StPifXmodemTx
{
	PifXmodemTxState state;
	uint16_t data_pos;
	uint16_t timeout;
	uint8_t ctrl;					// Control byte being sent, kept here as the UART may send from the pointer later
	uint8_t retry;					// EOT retries left
	PifXmodemTimeout wait;
	PifEvtXmodemTxReceive evt_receive;
	PifEvtXmodemFinish evt_finish;
} PifXmodemTx;

/**
 * @class StPifXmodemRx
 * @brief Represents the StPifXmodemRx data structure.
 */
typedef struct StPifXmodemRx
{
	PifXmodemRxState state;
	PifXmodemPacket packet;
	uint16_t count;
	uint16_t crc;
	uint16_t timeout;
	uint8_t last_packet_no;			// Number of the last packet passed on, 0 before the first
	PifXmodemTimeout wait;
	PifEvtXmodemRxReceive evt_receive;
	PifEvtXmodemFinish evt_finish;
} PifXmodemRx;

/**
 * @class StPifXmodem
 * @brief Represents the StPifXmodem data structure.
 */
typedef struct StPifXmodem
{
	// Public Member Variable

	// Read-only Member Variable
	PifId _id;

	// Private Member Variable
	PifTask* __p_task;				// Checks the timeouts
	PifUart* __p_uart;
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
 * @fn pifXmodem_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param type Protocol type or mode selector.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifXmodemType type);

/**
 * @fn pifXmodem_Clear
 * @brief Releases resources owned by the instance. The UART is detached first if attached.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_Clear(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Time in ms to wait for the answer to a packet or EOT. The time to send
 *                         the packet at the current baud rate is added to it.
 */
void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout);

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Time in ms allowed to receive a packet. The time to receive the packet
 *                        at the current baud rate is added to it.
 */
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout);

/**
 * @fn pifXmodem_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_AttachUart(PifXmodem* p_owner, PifUart* p_uart);

/**
 * @fn pifXmodem_DetachUart
 * @brief Detaches the UART and stops any transfer in progress.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_DetachUart(PifXmodem* p_owner);

/**
 * @fn pifXmodem_AttachEvtTx
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param evt_tx_receive Event callback invoked by this API.
 * @param evt_finish Event callback invoked by this API.
 */
void pifXmodem_AttachEvtTx(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @fn pifXmodem_AttachEvtRx
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param evt_rx_receive Event callback invoked by this API.
 * @param evt_finish Event callback invoked by this API.
 */
void pifXmodem_AttachEvtRx(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @fn pifXmodem_SendData
 * @brief Sends or queues a protocol frame.
 * @param p_owner Pointer to the protocol instance.
 * @param packet_no XMODEM packet sequence number.
 * @param p_data Pointer to a payload buffer.
 * @param data_size Payload length in bytes.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifXmodem_SendEot
 * @brief Ends the transfer. It is sent again on a NAK or no answer, up to PIF_XMODEM_EOT_RETRY
 *        times. The finish event follows the receiver's ACK.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_SendEot(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SendCancel
 * @brief Cancels the transfer in progress by sending CAN twice. No answer is expected.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_SendCancel(PifXmodem* p_owner);

/**
 * @fn pifXmodem_ReadyReceive
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
