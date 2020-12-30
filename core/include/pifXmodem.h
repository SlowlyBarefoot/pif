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


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifXmodem_Init(PIF_stPulse *pstTimer1ms, PIF_XmodemType enType);
void pifXmodem_Exit();

void pifXmodem_SetResponseTimeout(uint16_t usResponseTimeout);
void pifXmodem_SetReceiveTimeout(uint16_t usReceiveTimeout);

void pifXmodem_AttachComm(PIF_stComm *pstComm);
void pifXmodem_AttachEvtTxReceive(PIF_evtXmodemTxReceive evtReceive);
void pifXmodem_AttachEvtRxReceive(PIF_evtXmodemRxReceive evtReceive);

BOOL pifXmodem_SendData(uint8_t ucPacketNo, uint8_t *pucData, uint16_t usDataSize);
void pifXmodem_SendEot();
void pifXmodem_SendCancel();

BOOL pifXmodem_ReadyReceive();

#ifdef __cplusplus
}
#endif


#endif  // PIF_XMODEM_H
