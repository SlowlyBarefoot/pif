#ifndef PIF_COMM_H
#define PIF_COMM_H


#include "core/pif_ring_buffer.h"
#include "core/pif_task.h"


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

typedef BOOL (*PifActCommSetBaudRate)(PifComm* p_comm, uint32_t baudrate);
typedef BOOL (*PifActCommReceiveData)(PifComm* p_comm, uint8_t* p_data);
typedef uint16_t (*PifActCommSendData)(PifComm* p_comm, uint8_t* p_buffer, uint16_t size);
typedef BOOL (*PifActCommStartTransfer)(PifComm* p_comm);

typedef void (*PifEvtCommParsing)(void* p_client, PifActCommReceiveData act_receive_data);
typedef BOOL (*PifEvtCommSending)(void* p_client, PifActCommSendData act_send_data);

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

	// Public Action Function
    PifActCommSetBaudRate act_set_baudrate;
	PifActCommReceiveData act_receive_data;
    PifActCommSendData act_send_data;
    PifActCommStartTransfer act_start_transfer;

	// Read-only Member Variable
    PifId _id;
    PifRingBuffer* _p_tx_buffer;
    PifRingBuffer* _p_rx_buffer;
    PifTask* _p_task;

	// Private Member Variable
    void* __p_client;
    PifCommTxState __state;

    // Public Event Function
    PifEvtCommParsing __evt_parsing;
    PifEvtCommSending __evt_sending;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifComm_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifComm_Init(PifComm* p_owner, PifId id);

/**
 * @fn pifComm_Clear
 * @brief
 * @param p_owner
 */
void pifComm_Clear(PifComm* p_owner);

/**
 * @fn pifComm_AllocRxBuffer
 * @brief
 * @param p_owner
 * @param rx_Size
 * @return
 */
BOOL pifComm_AllocRxBuffer(PifComm* p_owner, uint16_t rx_size);

/**
 * @fn pifComm_AllocTxBuffer
 * @brief
 * @param p_owner
 * @param tx_size
 * @return
 */
BOOL pifComm_AllocTxBuffer(PifComm* p_owner, uint16_t tx_size);

/**
 * @fn pifComm_AttachClient
 * @brief
 * @param p_owner
 * @param p_client
 * @param evt_parsing
 * @param evt_sending
 */
void pifComm_AttachClient(PifComm* p_owner, void* p_client, PifEvtCommParsing evt_parsing, PifEvtCommSending evt_sending);

/**
 * @fn pifComm_DetachClient
 * @brief
 * @param p_owner
 */
void pifComm_DetachClient(PifComm* p_owner);

/**
 * @fn pifComm_GetRemainSizeOfRxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifComm_GetRemainSizeOfRxBuffer(PifComm* p_owner);

/**
 * @fn pifComm_GetFillSizeOfTxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifComm_GetFillSizeOfTxBuffer(PifComm* p_owner);

/**
 * @fn pifComm_PutRxByte
 * @brief
 * @param p_owner
 * @param data
 * @return 
 */
BOOL pifComm_PutRxByte(PifComm* p_owner, uint8_t data);

/**
 * @fn pifComm_PutRxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
BOOL pifComm_PutRxData(PifComm* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifComm_GetTxByte
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
uint8_t pifComm_GetTxByte(PifComm* p_owner, uint8_t* p_data);

/**
 * @fn pifComm_StartGetTxData
 * @brief
 * @param p_owner
 * @param pp_data
 * @param p_length
 * @return
 */
uint8_t pifComm_StartGetTxData(PifComm* p_owner, uint8_t** pp_data, uint16_t *p_length);

/**
 * @fn pifComm_EndGetTxData
 * @brief
 * @param p_owner
 * @param length
 * @return
 */
uint8_t pifComm_EndGetTxData(PifComm* p_owner, uint16_t length);

/**
 * @fn pifComm_ReceiveRxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
uint16_t pifComm_ReceiveRxData(PifComm* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifComm_SendTxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
BOOL pifComm_SendTxData(PifComm* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifComm_FinishTransfer
 * @brief
 * @param p_owner
 */
void pifComm_FinishTransfer(PifComm* p_owner);

/**
 * @fn pifComm_ForceSendData
 * @brief
 * @param p_owner
 */
void pifComm_ForceSendData(PifComm* p_owner);

/**
 * @fn pifComm_AttachTask
 * @brief Task를 추가한다.
 * @param p_owner
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param start 즉시 시작할지를 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifComm_AttachTask(PifComm* p_owner, PifTaskMode mode, uint16_t period, BOOL start);

#ifdef __cplusplus
}
#endif


#endif  // PIF_COMM_H
