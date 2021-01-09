#ifndef PIF_XMODEM_H
#define PIF_XMODEM_H


#include "pifComm.h"
#include "pifPulse.h"


typedef enum _PIF_XmodemType
{
	XT_Original	= 1,
	XT_CRC		= 2
} PIF_XmodemType;

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
 * @class _PIF_stXmodem
 * @brief
 */
typedef struct _PIF_stXmodem
{
	PIF_usId usPifId;
} PIF_stXmodem;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifXmodem_Init(PIF_stPulse *pstTimer1ms, uint8_t ucSize);
void pifXmodem_Exit();

PIF_stXmodem *pifXmodem_Add(PIF_usId usPifId, PIF_XmodemType enType);

void pifXmodem_SetResponseTimeout(PIF_stXmodem *pstOwner, uint16_t usResponseTimeout);
void pifXmodem_SetReceiveTimeout(PIF_stXmodem *pstOwner, uint16_t usReceiveTimeout);

void pifXmodem_AttachComm(PIF_stXmodem *pstOwner, PIF_stComm *pstComm);
void pifXmodem_AttachEvent(PIF_stXmodem *pstOwner, PIF_evtXmodemTxReceive evtTxReceive, PIF_evtXmodemRxReceive evtRxReceive);

BOOL pifXmodem_SendData(PIF_stXmodem *pstOwner, uint8_t ucPacketNo, uint8_t *pucData, uint16_t usDataSize);
void pifXmodem_SendEot(PIF_stXmodem *pstOwner);
void pifXmodem_SendCancel(PIF_stXmodem *pstOwner);

BOOL pifXmodem_ReadyReceive(PIF_stXmodem *pstOwner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
