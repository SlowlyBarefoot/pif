#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "pifRingBuffer.h"
#include "pifTask.h"


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


struct StPifComm;
typedef struct StPifComm PifComm;

typedef BOOL (*PifActCommReceiveData)(PifComm* p_comm, uint8_t* p_data);
typedef uint16_t (*PifActCommSendData)(PifComm* p_comm, uint8_t* p_buffer, uint16_t size);

typedef void (*PifEvtCommParsing)(void* p_client, PifActCommReceiveData act_receive_data);
typedef BOOL (*PifEvtCommSending)(void* p_client, PifActCommSendData act_send_data);

typedef BOOL (*PifActCommStartTransfer)();

typedef enum EnPifCommTxState
{
	CTS_IDLE		= 0,
	CTS_SENDING		= 1
} PifCommTxState;

/**
 * @class StPifComm
 * @brief
 */
struct StPifComm
{
	// Public Member Variable

    // Public Event Function
    PifEvtCommParsing evt_parsing;
    PifEvtCommSending evt_sending;

	// Read-only Member Variable
    PifId _id;
    PifRingBuffer* _p_tx_buffer;
    PifRingBuffer* _p_rx_buffer;
    PifTask* _p_task;

	// Private Member Variable
    void* __p_client;
    PifCommTxState __state;

	// Private Action Function
	PifActCommReceiveData __act_receive_data;
    PifActCommSendData __act_send_data;
    PifActCommStartTransfer __act_start_transfer;
};


#ifdef __cplusplus
extern "C" {
#endif

PifComm* pifComm_Create(PifId id);
void pifComm_Destroy(PifComm** pp_owner);

BOOL pifComm_AllocRxBuffer(PifComm* p_owner, uint16_t rx_size);
BOOL pifComm_AllocTxBuffer(PifComm* p_owner, uint16_t tx_size);

void pifComm_AttachClient(PifComm* p_owner, void* p_client);
void pifComm_AttachActReceiveData(PifComm* p_owner, PifActCommReceiveData act_receive_data);
void pifComm_AttachActSendData(PifComm* p_owner, PifActCommSendData act_send_data);
void pifComm_AttachActStartTransfer(PifComm* p_owner, PifActCommStartTransfer act_start_transfer);

uint16_t pifComm_GetRemainSizeOfRxBuffer(PifComm* p_owner);
uint16_t pifComm_GetFillSizeOfTxBuffer(PifComm* p_owner);

BOOL pifComm_ReceiveData(PifComm* p_owner, uint8_t data);
BOOL pifComm_ReceiveDatas(PifComm* p_owner, uint8_t* p_data, uint16_t length);
uint8_t pifComm_SendData(PifComm* p_owner, uint8_t* p_data);
uint8_t pifComm_StartSendDatas(PifComm* p_owner, uint8_t** pp_data, uint16_t *p_length);
uint8_t pifComm_EndSendDatas(PifComm* p_owner, uint16_t length);
void pifComm_FinishTransfer(PifComm* p_owner);

void pifComm_ForceSendData(PifComm* p_owner);

// Task Function
PifTask* pifComm_AttachTask(PifComm* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_COMM_H
