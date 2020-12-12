#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "pifRingBuffer.h"
#include "pifTask.h"


#ifndef PIF_COMM_RX_BUFFER_SIZE
#define PIF_COMM_RX_BUFFER_SIZE		8
#endif
#ifndef PIF_COMM_TX_BUFFER_SIZE
#define PIF_COMM_TX_BUFFER_SIZE		8
#endif


typedef void (*PIF_evtCommParsing)(void *pvClient, PIF_stRingBuffer *pstBuffer);
typedef BOOL (*PIF_evtCommSending)(void *pvClient, PIF_stRingBuffer *pstBuffer);
typedef void (*PIF_evtCommSended)(void *pvClient);

typedef BOOL (*PIF_actCommSendData)(uint8_t ucData);


typedef enum _PIF_enCommTxState
{
	STS_enIdle		= 0,
	STS_enSending	= 1
} PIF_enCommTxState;

/**
 * @class _PIF_stComm
 * @brief
 */
typedef struct _PIF_stComm
{
	// Public Member Variable
    PIF_unDeviceCode unDeviceCode;

	// Public Action Function
    PIF_actCommSendData actSendData;
} PIF_stComm;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifComm_Init(uint8_t ucSize);
void pifComm_Exit();

PIF_stComm *pifComm_Add(PIF_unDeviceCode unDeviceCode);

BOOL pifComm_ResizeRxBuffer(PIF_stComm *pstOwner, uint16_t usRxSize);
BOOL pifComm_ResizeTxBuffer(PIF_stComm *pstOwner, uint16_t usTxSize);

void pifComm_AttachClient(PIF_stComm *pstOwner, void *pvClient);
void pifComm_AttachEvtParsing(PIF_stComm *pstOwner, PIF_evtCommParsing evtParsing);
void pifComm_AttachEvtSending(PIF_stComm *pstOwner, PIF_evtCommSending evtSending);
void pifComm_AttachEvtSended(PIF_stComm *pstOwner, PIF_evtCommSended evtSended);

BOOL pifComm_ReceiveData(PIF_stComm *pstOwner, uint8_t ucData);
BOOL pifComm_ReceiveDatas(PIF_stComm *pstOwner, uint8_t *pucData, uint16_t usLength);
BOOL pifComm_SendData(PIF_stComm *pstOwner, uint8_t *pucData);

// Task Function
void pifComm_taskAll(PIF_stTask *pstTask);
void pifComm_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_COMM_H
