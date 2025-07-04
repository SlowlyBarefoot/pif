#ifndef PIF_UART_H
#define PIF_UART_H


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

#define PIF_UART_SEND_DATA_STATE_INIT		0
#define PIF_UART_SEND_DATA_STATE_DATA		1
#define PIF_UART_SEND_DATA_STATE_EMPTY		2


typedef enum EnPifUartFlowControl
{
	UFC_NONE			= 0,
	UFC_HOST_SOFTWARE	= 2,	// Xon / Xoff
	UFC_HOST_HARDWARE	= 3,	// RTS / CTS, DSR / DTR
	UFC_DEVICE_SOFTWARE	= 4,	// Xon / Xoff
	UFC_DEVICE_HARDWARE	= 5,	// RTS / CTS, DSR / DTR

	UFC_HOST_MASK		= 2,
	UFC_DEVICE_MASK		= 4
} PifUartFlowControl;

typedef enum EnPifUartRxState
{
	URS_IDLE		= 0,
	URS_FIRST		= 1,
	URS_NEXT		= 2
} PifUartRxState;

typedef enum EnPifUartTxState
{
	UTS_IDLE		= 0,
	UTS_SENDING		= 1
} PifUartTxState;

typedef enum EnPifUartDirection
{
	UD_RX		= 0,
	UD_TX		= 1
} PifUartDirection;


struct StPifUart;
typedef struct StPifUart PifUart;

typedef BOOL (*PifActUartSetBaudrate)(PifUart* p_uart, uint32_t baudrate);
typedef uint16_t (*PifActUartReceiveData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
typedef uint16_t (*PifActUartSendData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
typedef BOOL (*PifActUartStartTransfer)(PifUart* p_uart);
typedef uint8_t (*PifActUartGetRate)(PifUart* p_uart);
typedef void (*PifActUartDeviceFlowState)(PifUart* p_uart, SWITCH state);
typedef void (*PifActUartDirection)(PifUartDirection direction);

typedef BOOL (*PifEvtUartParsing)(void* p_client, PifActUartReceiveData act_receive_data);
typedef uint16_t (*PifEvtUartSending)(void* p_client, PifActUartSendData act_send_data);
typedef void (*PifEvtUartAbortRx)(void* p_client);
typedef void (*PifEvtUartHostFlowState)(void* p_client, SWITCH state);


/**
 * @class StPifUart
 * @brief
 */
struct StPifUart
{
	// Public Member Variable
	uint8_t fc_limit;							// Range: 0% ~ 100%, default: 50%

	// Public Action Function
    PifActUartSetBaudrate act_set_baudrate;
	PifActUartReceiveData act_receive_data;
    PifActUartSendData act_send_data;
    PifActUartStartTransfer act_start_transfer;
    PifActUartGetRate act_get_tx_rate;
    PifActUartGetRate act_get_rx_rate;
    PifActUartDeviceFlowState act_device_flow_state;

    // Public Event Function
    PifEvtUartAbortRx evt_abort_rx;

	// Read-only Member Variable
    PifId _id;
    uint32_t _baudrate;
    PifUartFlowControl _flow_control;
    uint8_t _frame_size;
    uint16_t _transfer_time;
    PifRingBuffer* _p_tx_buffer;
    PifRingBuffer* _p_rx_buffer;
    PifTask* _p_tx_task;
    PifTask* _p_rx_task;
    SWITCH _fc_state;

	// Private Member Variable
    void* __p_client;
    volatile PifUartTxState __tx_state;
    volatile PifUartRxState __rx_state;

	// Private Action Function
    PifActUartDirection __act_direction;

    // Private Event Function
    PifEvtUartParsing __evt_parsing;
    PifEvtUartSending __evt_sending;
    PifEvtUartHostFlowState __evt_host_flow_state;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifUart_Init
 * @brief
 * @param p_owner
 * @param id
 * @param baudrate
 * @return
 */
BOOL pifUart_Init(PifUart* p_owner, PifId id, uint32_t baudrate);

/**
 * @fn pifUart_Clear
 * @brief
 * @param p_owner
 */
void pifUart_Clear(PifUart* p_owner);

/**
 * @fn pifUart_AllocRxBuffer
 * @brief
 * @param p_owner
 * @param rx_size
 * @return
 */
BOOL pifUart_AllocRxBuffer(PifUart* p_owner, uint16_t rx_size);

/**
 * @fn pifUart_AssignRxBuffer
 * @brief
 * @param p_owner
 * @param rx_size
 * @param p_buffer
 * @return
 */
BOOL pifUart_AssignRxBuffer(PifUart* p_owner, uint16_t rx_size, uint8_t* p_buffer);

/**
 * @fn pifUart_AllocTxBuffer
 * @brief
 * @param p_owner
 * @param tx_size
 * @return
 */
BOOL pifUart_AllocTxBuffer(PifUart* p_owner, uint16_t tx_size);

/**
 * @fn pifUart_AssignTxBuffer
 * @brief
 * @param p_owner
 * @param tx_size
 * @param p_buffer
 * @return
 */
BOOL pifUart_AssignTxBuffer(PifUart* p_owner, uint16_t tx_size, uint8_t* p_buffer);

/**
 * @fn pifUart_SetFrameSize
 * @brief
 * @param p_owner
 * @param frame_size
 * @return
 */
BOOL pifUart_SetFrameSize(PifUart* p_owner, uint8_t frame_size);

/**
 * @fn pifUart_ChangeBaudrate
 * @brief
 * @param p_owner
 * @param baudrate
 * @return
 */
BOOL pifUart_ChangeBaudrate(PifUart* p_owner, uint32_t baudrate);

/**
 * @fn pifUart_AttachClient
 * @brief
 * @param p_owner
 * @param p_client
 * @param evt_parsing
 * @param evt_sending
 */
void pifUart_AttachClient(PifUart* p_owner, void* p_client, PifEvtUartParsing evt_parsing, PifEvtUartSending evt_sending);

/**
 * @fn pifUart_AttachActDirection
 * @brief
 * @param p_owner
 * @param act_direction
 * @param init_state
 */
void pifUart_AttachActDirection(PifUart* p_owner, PifActUartDirection act_direction, PifUartDirection init_state);

/**
 * @fn pifUart_DetachClient
 * @brief
 * @param p_owner
 */
void pifUart_DetachClient(PifUart* p_owner);

/**
 * @fn pifUart_ResetFlowControl
 * @brief
 * @param p_owner
 */
void pifUart_ResetFlowControl(PifUart* p_owner);

/**
 * @fn pifUart_SetFlowControl
 * @brief
 * @param p_owner
 * @param flow_control
 * @param evt_host_flow_state
 */
void pifUart_SetFlowControl(PifUart* p_owner, PifUartFlowControl flow_control, PifEvtUartHostFlowState evt_host_flow_state);

/**
 * @fn pifUart_ChangeRxFlowState
 * @brief
 * @param p_owner
 * @param state
 * @return
 */
BOOL pifUart_ChangeRxFlowState(PifUart* p_owner, SWITCH state);

/**
 * @fn pifUart_SigTxFlowState
 * @brief
 * @param p_owner
 * @param state
 */
void pifUart_SigTxFlowState(PifUart* p_owner, SWITCH state);

/**
 * @fn pifUart_GetFillSizeOfRxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifUart_GetFillSizeOfRxBuffer(PifUart* p_owner);

/**
 * @fn pifUart_GetFillSizeOfTxBuffer
 * @brief
 * @param p_owner
 * @return
 */
uint16_t pifUart_GetFillSizeOfTxBuffer(PifUart* p_owner);

/**
 * @fn pifUart_PutRxByte
 * @brief
 * @param p_owner
 * @param data
 * @return 
 */
BOOL pifUart_PutRxByte(PifUart* p_owner, uint8_t data);

/**
 * @fn pifUart_PutRxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
BOOL pifUart_PutRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_GetTxByte
 * @brief
 * @param p_owner
 * @param p_data
 * @return
 */
uint8_t pifUart_GetTxByte(PifUart* p_owner, uint8_t* p_data);

/**
 * @fn pifUart_StartGetTxData
 * @brief
 * @param p_owner
 * @param pp_data
 * @param p_length
 * @return
 */
uint8_t pifUart_StartGetTxData(PifUart* p_owner, uint8_t** pp_data, uint16_t *p_length);

/**
 * @fn pifUart_EndGetTxData
 * @brief
 * @param p_owner
 * @param length
 * @return
 */
uint8_t pifUart_EndGetTxData(PifUart* p_owner, uint16_t length);

/**
 * @fn pifUart_ReceiveRxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
uint16_t pifUart_ReceiveRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_SendTxData
 * @brief
 * @param p_owner
 * @param p_data
 * @param length
 * @return
 */
uint16_t pifUart_SendTxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_AbortRx
 * @brief
 * @param p_owner
 */
void pifUart_AbortRx(PifUart* p_owner);

/**
 * @fn pifUart_CheckTxTransfer
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifUart_CheckTxTransfer(PifUart* p_owner);

/**
 * @fn pifUart_AttachRxTask
 * @brief Rx Task를 추가한다.
 * @param p_owner
 * @param id Task의 ID를 설정한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param name task의 이름을 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifUart_AttachRxTask(PifUart* p_owner, PifId id, PifTaskMode mode, uint32_t period, const char* name);

/**
 * @fn pifUart_AttachTxTask
 * @brief Tx Task를 추가한다.
 * @param p_owner
 * @param id Task의 ID를 설정한다.
 * @param mode Task의 Mode를 설정한다.
 * @param period Mode에 따라 주기의 단위가 변경된다.
 * @param name task의 이름을 지정한다.
 * @return Task 구조체 포인터를 반환한다.
 */
PifTask* pifUart_AttachTxTask(PifUart* p_owner, PifId id, PifTaskMode mode, uint32_t period, const char* name);

#ifdef __cplusplus
}
#endif


#endif  // PIF_UART_H
