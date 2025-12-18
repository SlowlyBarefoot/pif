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
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
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
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param type
 * @return
 */
BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifTimerManager* p_timer_manager, PifXmodemType type);

/**
 * @fn pifXmodem_Clear
 * @brief
 * @param p_owner
 */
void pifXmodem_Clear(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SetResponseTimeout
 * @brief
 * @param p_owner
 * @param response_timeout
 */
void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout);

/**
 * @fn pifXmodem_SetReceiveTimeout
 * @brief
 * @param p_owner
 * @param receive_timeout
 */
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout);

/**
 * @fn pifXmodem_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifXmodem_AttachUart(PifXmodem* p_owner, PifUart* p_uart);

/**
 * @fn pifXmodem_DetachUart
 * @brief
 * @param p_owner
 */
void pifXmodem_DetachUart(PifXmodem* p_owner);

/**
 * @fn pifXmodem_AttachEvtTx
 * @brief
 * @param p_owner
 * @param evt_tx_receive
 * @param evt_finish
 */
void pifXmodem_AttachEvtTx(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @fn pifXmodem_AttachEvtRx
 * @brief
 * @param p_owner
 * @param evt_rx_receive
 * @param evt_finish
 */
void pifXmodem_AttachEvtRx(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive, PifEvtXmodemFinish evt_finish);

/**
 * @fn pifXmodem_SendData
 * @brief
 * @param p_owner
 * @param packet_no
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifXmodem_SendEot
 * @brief
 * @param p_owner
 */
void pifXmodem_SendEot(PifXmodem* p_owner);

/**
 * @fn pifXmodem_SendCancel
 * @brief
 * @param p_owner
 */
void pifXmodem_SendCancel(PifXmodem* p_owner);

/**
 * @fn pifXmodem_ReadyReceive
 * @brief
 * @param p_owner
 */
void pifXmodem_ReadyReceive(PifXmodem* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
