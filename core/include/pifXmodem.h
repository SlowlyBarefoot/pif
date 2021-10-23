#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "pifComm.h"
#include "pifPulse.h"


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
	XTS_DELAY_C			= 11,
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


/**
 * @class StPifXmodemTx
 * @brief
 */
typedef struct StPifXmodemTx
{
	PifXmodemTxState state;
	uint16_t data_pos;
	uint16_t timeout;
	PifPulseItem* p_timer;
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
	PifPulseItem* p_timer;
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
	PifPulse* __p_timer;
	PifComm* __p_comm;
	PifXmodemType __type;
	uint16_t __packet_size;
	PifXmodemTx __tx;
	PifXmodemRx __rx;
    uint8_t* __p_data;
} PifXmodem;


#ifdef __cplusplus
extern "C" {
#endif

PifXmodem* pifXmodem_Create(PifId id, PifPulse* p_timer, PifXmodemType type);
void pifXmodem_Destroy(PifXmodem** pp_owner);

BOOL pifXmodem_Init(PifXmodem* p_owner, PifId id, PifPulse* p_timer, PifXmodemType type);
void pifXmodem_Clear(PifXmodem* p_owner);

void pifXmodem_SetResponseTimeout(PifXmodem* p_owner, uint16_t response_timeout);
void pifXmodem_SetReceiveTimeout(PifXmodem* p_owner, uint16_t receive_timeout);

void pifXmodem_AttachComm(PifXmodem* p_owner, PifComm* p_comm);
void pifXmodem_AttachEvtTxReceive(PifXmodem* p_owner, PifEvtXmodemTxReceive evt_tx_receive);
void pifXmodem_AttachEvtRxReceive(PifXmodem* p_owner, PifEvtXmodemRxReceive evt_rx_receive);

BOOL pifXmodem_SendData(PifXmodem* p_owner, uint8_t packet_no, uint8_t* p_data, uint16_t data_size);
void pifXmodem_SendEot(PifXmodem* p_owner);
void pifXmodem_SendCancel(PifXmodem* p_owner);

void pifXmodem_ReadyReceive(PifXmodem* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
