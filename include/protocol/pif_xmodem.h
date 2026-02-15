#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "communication/pif_uart.h"
#include "core/pif_timer_manager.h"


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
	XRS_SOH				= ASCII_SOH,	// 1
	XRS_EOT				= ASCII_EOT,	// 4
	XRS_CAN				= ASCII_CAN		// 24
} PifXmodemRxState;

typedef enum EnPifXmodemTxState
{
	XTS_IDLE			= 0,
	XTS_SEND_C			= 10,
	XTS_SENDING			= 12,
	XTS_WAIT_RESPONSE	= 13,
	XTS_EOT				= ASCII_EOT,	// 4
	XTS_ACK				= ASCII_ACK,	// 6
	XTS_NAK				= ASCII_NAK,	// 21
	XTS_CAN				= ASCII_CAN		// 24
} PifXmodemTxState;


/**
 * @class StPifXmodemPacket
 * @brief Represents the StPifXmodemPacket data structure.
 */
typedef struct StPifXmodemPacket
{
	uint8_t packet_no[2];
	uint8_t* p_data;
} PifXmodemPacket;


typedef void (*PifEvtXmodemTxReceive)(uint8_t code, uint8_t packet_no);
typedef void (*PifEvtXmodemRxReceive)(uint8_t code, PifXmodemPacket* p_packet);
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
	PifTimer* p_timer;
	PifEvtXmodemTxReceive evt_receive;
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
	PifTimer* p_timer;
	PifEvtXmodemRxReceive evt_receive;
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
	PifTimerManager* __p_timer_manager;
	PifUart* __p_uart;
	PifXmodemType __type;
	uint16_t __packet_size;
	PifXmodemTx __tx;
	PifXmodemRx __rx;
    uint8_t* __p_data;
    BOOL __finish;

    // Private Event Function
    PifEvtXmodemFinish __evt_finish;
} PifXmodem;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifXmodem_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @param p_timer_manager Timer manager used to allocate protocol timers.
 * @param type Protocol type or mode selector.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifTimerManager* p_timer_manager, PifXmodemType type);

/**
 * @fn pifXmodem_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_Clear(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param response_timeout Response timeout value in timer ticks.
 */
void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout);

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief Updates a runtime configuration value.
 * @param p_owner Pointer to the protocol instance.
 * @param receive_timeout Receive timeout value in timer ticks.
 */
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout);

/**
 * @fn pifXmodem_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifXmodem_AttachUart(PifXmodem* p_owner, PifUart* p_uart);

/**
 * @fn pifXmodem_DetachUart
 * @brief Detaches a previously attached interface or callback.
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
 * @brief Sends or queues a protocol frame.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_SendEot(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SendCancel
 * @brief Sends or queues a protocol frame.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_SendCancel(PifXmodem* p_owner);

/**
 * @fn pifXmodem_ReadyReceive
 * @brief Reads data from the protocol context.
 * @param p_owner Pointer to the protocol instance.
 */
void pifXmodem_ReadyReceive(PifXmodem* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
