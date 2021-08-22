#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "pifRingBuffer.h"
#include "pifTask.h"


#ifndef PIF_COMM_RX_BUFFER_SIZE
#define PIF_COMM_RX_BUFFER_SIZE		16
#endif
#ifndef PIF_COMM_TX_BUFFER_SIZE
#define PIF_COMM_TX_BUFFER_SIZE		16
#endif

#define ASCII_NUL	0	// Null Character
#define ASCII_SOH	1	// Start of Header
#define ASCII_STX	2	// Start of Text
#define ASCII_ETX	3	// End of Text
#define ASCII_EOT	4	// End of Transmission
#define ASCII_ENQ	5	// Enquiry
#define ASCII_ACK	6	// Acknowledgment
#define ASCII_BEL	7	// Bell
#define ASCII_BS	8	// Backspace
#define ASCII_HT	9	// Horizontal Tab
#define ASCII_LF	10	// Line feed
#define ASCII_VT	11	// Vertical Tab
#define ASCII_FF	12	// Form feed
#define ASCII_CR	13	// Carriage return
#define ASCII_SO	14	// Shift Out
#define ASCII_SI	15	// Shift In
#define ASCII_DLE	16	// Data Link Escape
#define ASCII_DC1	17	// Device Control 1
#define ASCII_XON	17	// Resume transmission
#define ASCII_DC2	18	// Device Control 2
#define ASCII_DC3	19	// Device Control 3
#define ASCII_XOFF	19	// Pause transmission
#define ASCII_DC4	20	// Device Control 4
#define ASCII_NAK	21	// Negative Acknowledgment
#define ASCII_SYN	22	// Synchronous idle
#define ASCII_ETB	23	// End of Transmission Block
#define ASCII_CAN	24	// Cancel
#define ASCII_EM	25	// End of Medium
#define ASCII_SUB	26	// Substitute
#define ASCII_ESC	27	// Escape
#define ASCII_FS	28	// File Separator
#define ASCII_GS	29	// Group Separator
#define ASCII_RS	30	// Record Separator
#define ASCII_US	31	// Unit Separator

#define PIF_COMM_SEND_DATA_STATE_INIT		0
#define PIF_COMM_SEND_DATA_STATE_DATA		1
#define PIF_COMM_SEND_DATA_STATE_EMPTY		2


typedef struct _PIF_stComm PIF_stComm;

typedef BOOL (*PIF_actCommReceiveData)(PIF_stComm *pstComm, uint8_t *pucData);
typedef uint16_t (*PIF_actCommSendData)(PIF_stComm *pstComm, uint8_t *pucBuffer, uint16_t usSize);

typedef void (*PIF_evtCommParsing)(void *pvClient, PIF_actCommReceiveData actReceiveData);
typedef BOOL (*PIF_evtCommSending)(void *pvClient, PIF_actCommSendData actSendData);

typedef void (*PIF_actCommStartTransfer)();


/**
 * @class _PIF_stComm
 * @brief
 */
struct _PIF_stComm
{
	// Public Member Variable

    // Public Event Function
    PIF_evtCommParsing evtParsing;
    PIF_evtCommSending evtSending;

	// Read-only Member Variable
    PIF_usId _usPifId;
    PIF_stRingBuffer *_pstTxBuffer;
    PIF_stRingBuffer *_pstRxBuffer;
    PIF_stTask *_pstTask;

	// Private Member Variable
    void *__pvClient;

	PIF_enTaskLoop __enTaskLoop;

	// Private Action Function
	PIF_actCommReceiveData __actReceiveData;
    PIF_actCommSendData __actSendData;
    PIF_actCommStartTransfer __actStartTransfer;
};


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifComm_Init(uint8_t ucSize);
void pifComm_Exit();

PIF_stComm *pifComm_Add(PIF_usId usPifId);

BOOL pifComm_AllocRxBuffer(PIF_stComm *pstOwner, uint16_t usRxSize);
BOOL pifComm_AllocTxBuffer(PIF_stComm *pstOwner, uint16_t usTxSize);

void pifComm_AttachClient(PIF_stComm *pstOwner, void *pvClient);
void pifComm_AttachActReceiveData(PIF_stComm *pstOwner, PIF_actCommReceiveData actReceiveData);
void pifComm_AttachActSendData(PIF_stComm *pstOwner, PIF_actCommSendData actSendData);
void pifComm_AttachActStartTransfer(PIF_stComm *pstOwner, PIF_actCommStartTransfer actStartTransfer);

uint16_t pifComm_GetRemainSizeOfRxBuffer(PIF_stComm *pstOwner);
uint16_t pifComm_GetFillSizeOfTxBuffer(PIF_stComm *pstOwner);

BOOL pifComm_ReceiveData(PIF_stComm *pstOwner, uint8_t ucData);
BOOL pifComm_ReceiveDatas(PIF_stComm *pstOwner, uint8_t *pucData, uint16_t usLength);
uint8_t pifComm_SendData(PIF_stComm *pstOwner, uint8_t *pucData);

void pifComm_ForceSendData(PIF_stComm *pstOwner);

// Task Function
uint16_t pifComm_taskAll(PIF_stTask *pstTask);
uint16_t pifComm_taskEach(PIF_stTask *pstTask);

#ifdef __cplusplus
}
#endif


#endif  // PIF_COMM_H
