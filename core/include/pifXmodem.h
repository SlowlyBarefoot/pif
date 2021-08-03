#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "pifComm.h"
#include "pifPulse.h"


typedef enum _PIF_enXmodemType
{
	XT_enOriginal	= 1,
	XT_enCRC		= 2
} PIF_enXmodemType;

typedef enum _PIF_enXmodemRxState
{
	XRS_enIdle			= 0,
	XRS_enC				= 'C',
	XRS_enGetHeader		= 10,
	XRS_enGetData		= 11,
	XRS_enGetCrc		= 12,
	XRS_enSOH			= ASCII_SOH,	// 1
	XRS_enEOT			= ASCII_EOT,	// 4
	XRS_enCAN			= ASCII_CAN		// 24
} PIF_enXmodemRxState;

typedef enum _PIF_enXmodemTxState
{
	XTS_enIdle			= 0,
	XTS_enSendC			= 10,
	XTS_enDelayC		= 11,
	XTS_enSending		= 12,
	XTS_enWaitResponse	= 13,
	XTS_enEOT			= ASCII_EOT,	// 4
	XTS_enACK			= ASCII_ACK,	// 6
	XTS_enNAK			= ASCII_NAK,	// 21
	XTS_enCAN			= ASCII_CAN		// 24
} PIF_enXmodemTxState;


/**
 * @class _PIF_stXmodemPacket
 * @brief
 */
typedef struct _PIF_stXmodemPacket
{
	uint8_t aucPacketNo[2];
	uint8_t *pucData;
} PIF_stXmodemPacket;


typedef void (*PIF_evtXmodemTxReceive)(uint8_t ucCode, uint8_t ucPacketNo);
typedef void (*PIF_evtXmodemRxReceive)(uint8_t ucCode, PIF_stXmodemPacket *pstPacket);


/**
 * @class _PIF_stXmodemTx
 * @brief
 */
typedef struct _PIF_stXmodemTx
{
	union {
		uint8_t ucState;
		PIF_enXmodemTxState enState;
	} ui;
	uint16_t usDataPos;
	uint16_t usTimeout;
	PIF_stPulseItem *pstTimer;
	PIF_evtXmodemTxReceive evtReceive;
} PIF_stXmodemTx;

/**
 * @class _PIF_stXmodemRx
 * @brief
 */
typedef struct _PIF_stXmodemRx
{
	PIF_enXmodemRxState enState;
	PIF_stXmodemPacket stPacket;
	uint16_t usCount;
	uint16_t usCrc;
	uint16_t usTimeout;
	PIF_stPulseItem *pstTimer;
	PIF_evtXmodemRxReceive evtReceive;
} PIF_stXmodemRx;

/**
 * @class _PIF_stXmodem
 * @brief
 */
typedef struct _PIF_stXmodem
{
	// Public Member Variable

	// Read-only Member Variable
	PIF_usId _usPifId;

	// Private Member Variable
	PIF_stComm *__pstComm;
	PIF_enXmodemType __enType;
	uint16_t __usPacketSize;
	PIF_stXmodemTx __stTx;
	PIF_stXmodemRx __stRx;
    uint8_t *__pucData;
} PIF_stXmodem;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifXmodem_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifXmodem_Exit();

PIF_stXmodem *pifXmodem_Add(PIF_usId usPifId, PIF_enXmodemType enType);

void pifXmodem_SetResponseTimeout(PIF_stXmodem *pstOwner, uint16_t usResponseTimeout);
void pifXmodem_SetReceiveTimeout(PIF_stXmodem *pstOwner, uint16_t usReceiveTimeout);

void pifXmodem_AttachComm(PIF_stXmodem *pstOwner, PIF_stComm *pstComm);
void pifXmodem_AttachEvtTxReceive(PIF_stXmodem *pstOwner, PIF_evtXmodemTxReceive evtTxReceive);
void pifXmodem_AttachEvtRxReceive(PIF_stXmodem *pstOwner, PIF_evtXmodemRxReceive evtRxReceive);

BOOL pifXmodem_SendData(PIF_stXmodem *pstOwner, uint8_t ucPacketNo, uint8_t *pucData, uint16_t usDataSize);
void pifXmodem_SendEot(PIF_stXmodem *pstOwner);
void pifXmodem_SendCancel(PIF_stXmodem *pstOwner);

void pifXmodem_ReadyReceive(PIF_stXmodem *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
