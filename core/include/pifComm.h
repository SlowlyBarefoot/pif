#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "pifRingBuffer.h"
#include "pifTask.h"


#ifndef PIF_COMM_RX_BUFFER_SIZE
#define PIF_COMM_RX_BUFFER_SIZE		32
#endif
#ifndef PIF_COMM_TX_BUFFER_SIZE
#define PIF_COMM_TX_BUFFER_SIZE		32
#endif


typedef void (*PIF_fnCommParsing)(void *pvProcessor, PIF_stRingBuffer *pstBuffer);
typedef BOOL (*PIF_fnCommSending)(void *pvProcessor, PIF_stRingBuffer *pstBuffer);
typedef void (*PIF_fnCommSended)(void *pvProcessor);


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

	// Private Member Variable
    void *__pvProcessor;

    PIF_stRingBuffer __stTxBuffer;
    PIF_enCommTxState __enState;

    PIF_stRingBuffer __stRxBuffer;

	PIF_enTaskLoop __enTaskLoop;

    // Private Member Function
    PIF_fnCommSending __fnSending;
    PIF_fnCommSended __fnSended;

    PIF_fnCommParsing __fnParing;
} PIF_stComm;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifComm_Init(uint8_t ucSize);
void pifComm_Exit();

PIF_stComm *pifComm_Add(PIF_unDeviceCode unDeviceCode);

BOOL pifComm_ResizeRxBuffer(PIF_stComm *pstOwner, uint16_t usRxSize);
BOOL pifComm_ResizeTxBuffer(PIF_stComm *pstOwner, uint16_t usTxSize);

BOOL pifComm_ReceiveData(PIF_stComm *pstOwner, uint8_t ucData);
BOOL pifComm_SendData(PIF_stComm *pstOwner, uint8_t *pucData);

// Task Function
void pifComm_taskAll(PIF_stTask *pstTask);
void pifComm_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_COMM_H
