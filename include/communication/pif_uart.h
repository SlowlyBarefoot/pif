#ifndef PIF_UART_H
#define PIF_UART_H


#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"


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
 * @brief UART abstraction that encapsulates buffers, flow control, and task hooks.
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
 * @brief Initializes a UART object with a logical ID and default baud rate.
 * @param p_owner Pointer to the UART object to initialize.
 * @param id Logical ID assigned to the UART instance.
 * @param baudrate Initial baud rate in bits per second.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifUart_Init(PifUart* p_owner, PifId id, uint32_t baudrate);

/**
 * @fn pifUart_Clear
 * @brief Releases resources owned by the UART object.
 * @param p_owner Pointer to an initialized UART object.
 */
void pifUart_Clear(PifUart* p_owner);

/**
 * @fn pifUart_AllocRxBuffer
 * @brief Allocates an internal ring buffer for RX data.
 * @param p_owner Pointer to the UART object.
 * @param rx_size Buffer capacity in bytes.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AllocRxBuffer(PifUart* p_owner, uint16_t rx_size);

/**
 * @fn pifUart_AssignRxBuffer
 * @brief Assigns an external memory block as the RX ring buffer.
 * @param p_owner Pointer to the UART object.
 * @param rx_size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return `TRUE` if assignment succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AssignRxBuffer(PifUart* p_owner, uint16_t rx_size, uint8_t* p_buffer);

/**
 * @fn pifUart_AllocTxBuffer
 * @brief Allocates an internal ring buffer for TX data.
 * @param p_owner Pointer to the UART object.
 * @param tx_size Buffer capacity in bytes.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AllocTxBuffer(PifUart* p_owner, uint16_t tx_size);

/**
 * @fn pifUart_AssignTxBuffer
 * @brief Assigns an external memory block as the TX ring buffer.
 * @param p_owner Pointer to the UART object.
 * @param tx_size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return `TRUE` if assignment succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AssignTxBuffer(PifUart* p_owner, uint16_t tx_size, uint8_t* p_buffer);

/**
 * @fn pifUart_SetFrameSize
 * @brief Sets the transfer frame size used for scheduling and timing.
 * @param p_owner Pointer to the UART object.
 * @param frame_size Number of bits per frame (data/parity/stop composition).
 * @return `TRUE` if the value is accepted, otherwise `FALSE`.
 */
BOOL pifUart_SetFrameSize(PifUart* p_owner, uint8_t frame_size);

/**
 * @fn pifUart_ChangeBaudrate
 * @brief Changes UART baud rate through the attached hardware callback.
 * @param p_owner Pointer to the UART object.
 * @param baudrate New baud rate in bits per second.
 * @return `TRUE` if the hardware update succeeds, otherwise `FALSE`.
 */
BOOL pifUart_ChangeBaudrate(PifUart* p_owner, uint32_t baudrate);

/**
 * @fn pifUart_AttachClient
 * @brief Attaches protocol callbacks used for RX parsing and TX payload generation.
 * @param p_owner Pointer to the UART object.
 * @param p_client User-defined client context passed to callbacks.
 * @param evt_parsing Callback invoked to parse incoming bytes.
 * @param evt_sending Callback invoked to prepare outgoing bytes.
 */
void pifUart_AttachClient(PifUart* p_owner, void* p_client, PifEvtUartParsing evt_parsing, PifEvtUartSending evt_sending);

/**
 * @fn pifUart_AttachActDirection
 * @brief Attaches a callback used to control external RX/TX direction hardware.
 * @param p_owner Pointer to the UART object.
 * @param act_direction Callback that switches the external data direction.
 * @param init_state Initial direction state applied after attaching.
 */
void pifUart_AttachActDirection(PifUart* p_owner, PifActUartDirection act_direction, PifUartDirection init_state);

/**
 * @fn pifUart_DetachClient
 * @brief Detaches previously attached client callbacks and context.
 * @param p_owner Pointer to the UART object.
 */
void pifUart_DetachClient(PifUart* p_owner);

/**
 * @fn pifUart_ResetFlowControl
 * @brief Resets internal flow-control state to its default condition.
 * @param p_owner Pointer to the UART object.
 */
void pifUart_ResetFlowControl(PifUart* p_owner);

/**
 * @fn pifUart_SetFlowControl
 * @brief Configures UART flow-control mode and host-state notification callback.
 * @param p_owner Pointer to the UART object.
 * @param flow_control Flow-control mode to apply.
 * @param evt_host_flow_state Callback invoked when host flow state changes.
 */
void pifUart_SetFlowControl(PifUart* p_owner, PifUartFlowControl flow_control, PifEvtUartHostFlowState evt_host_flow_state);

/**
 * @fn pifUart_ChangeRxFlowState
 * @brief Changes local RX flow state and notifies the configured control path.
 * @param p_owner Pointer to the UART object.
 * @param state New receive permission state.
 * @return `TRUE` if the state transition is accepted, otherwise `FALSE`.
 */
BOOL pifUart_ChangeRxFlowState(PifUart* p_owner, SWITCH state);

/**
 * @fn pifUart_SigTxFlowState
 * @brief Applies remote TX flow state to local transmission control.
 * @param p_owner Pointer to the UART object.
 * @param state Remote flow state signaled by the counterpart.
 */
void pifUart_SigTxFlowState(PifUart* p_owner, SWITCH state);

/**
 * @fn pifUart_GetFillSizeOfRxBuffer
 * @brief Returns the number of bytes currently queued in the RX buffer.
 * @param p_owner Pointer to the UART object.
 * @return Number of unread bytes in the RX ring buffer.
 */
uint16_t pifUart_GetFillSizeOfRxBuffer(PifUart* p_owner);

/**
 * @fn pifUart_GetFillSizeOfTxBuffer
 * @brief Returns the number of bytes currently queued in the TX buffer.
 * @param p_owner Pointer to the UART object.
 * @return Number of pending bytes in the TX ring buffer.
 */
uint16_t pifUart_GetFillSizeOfTxBuffer(PifUart* p_owner);

/**
 * @fn pifUart_PutRxByte
 * @brief Pushes a single received byte into the RX processing path.
 * @param p_owner Pointer to the UART object.
 * @param data Byte received from hardware.
 * @return `TRUE` if the byte is accepted, otherwise `FALSE`.
 */
BOOL pifUart_PutRxByte(PifUart* p_owner, uint8_t data);

/**
 * @fn pifUart_PutRxData
 * @brief Pushes a block of received bytes into the RX processing path.
 * @param p_owner Pointer to the UART object.
 * @param p_data Input buffer containing received bytes.
 * @param length Number of bytes to process.
 * @return `TRUE` if all bytes are accepted, otherwise `FALSE`.
 */
BOOL pifUart_PutRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_GetTxByte
 * @brief Retrieves one byte from the TX queue.
 * @param p_owner Pointer to the UART object.
 * @param p_data Output pointer that receives the byte.
 * @return Non-zero when a byte is returned, zero when no data is available.
 */
uint8_t pifUart_GetTxByte(PifUart* p_owner, uint8_t* p_data);

/**
 * @fn pifUart_StartGetTxData
 * @brief Starts a contiguous TX-buffer read operation.
 * @param p_owner Pointer to the UART object.
 * @param pp_data Output pointer to the start address of available TX data.
 * @param p_length Output pointer to the contiguous data length.
 * @return Non-zero when data is available, zero otherwise.
 */
uint8_t pifUart_StartGetTxData(PifUart* p_owner, uint8_t** pp_data, uint16_t *p_length);

/**
 * @fn pifUart_EndGetTxData
 * @brief Finalizes a TX-buffer read operation after bytes are transmitted.
 * @param p_owner Pointer to the UART object.
 * @param length Number of bytes consumed from the TX buffer.
 * @return Remaining-status flag used by the TX state machine.
 */
uint8_t pifUart_EndGetTxData(PifUart* p_owner, uint16_t length);

/**
 * @fn pifUart_ReceiveRxData
 * @brief Pulls bytes from hardware through `act_receive_data` and stores them into RX flow.
 * @param p_owner Pointer to the UART object.
 * @param p_data Temporary buffer used to receive raw bytes from hardware.
 * @param length Requested maximum receive length.
 * @return Number of bytes actually received.
 */
uint16_t pifUart_ReceiveRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_SendTxData
 * @brief Sends bytes to hardware through `act_send_data`.
 * @param p_owner Pointer to the UART object.
 * @param p_data Buffer containing bytes to transmit.
 * @param length Number of bytes requested for transmission.
 * @return Number of bytes actually transmitted.
 */
uint16_t pifUart_SendTxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @fn pifUart_AbortRx
 * @brief Aborts current RX operation and triggers the abort callback if attached.
 * @param p_owner Pointer to the UART object.
 */
void pifUart_AbortRx(PifUart* p_owner);

/**
 * @fn pifUart_CheckTxTransfer
 * @brief Checks and progresses pending TX transfer state.
 * @param p_owner Pointer to the UART object.
 * @return `TRUE` while transfer is active or newly started, otherwise `FALSE`.
 */
BOOL pifUart_CheckTxTransfer(PifUart* p_owner);

/**
 * @fn pifUart_AttachRxTask
 * @brief Attaches a scheduler task that periodically services UART RX handling.
 * @param p_owner Pointer to the UART object.
 * @param id Task ID assigned by the task manager.
 * @param mode Task execution mode. The `period` unit depends on this mode.
 * @param period Task period value interpreted by the selected mode.
 * @param name Human-readable task name used for diagnostics.
 * @return Pointer to the created task, or `NULL` on failure.
 */
PifTask* pifUart_AttachRxTask(PifUart* p_owner, PifId id, PifTaskMode mode, uint32_t period, const char* name);

/**
 * @fn pifUart_AttachTxTask
 * @brief Attaches a scheduler task that periodically services UART TX handling.
 * @param p_owner Pointer to the UART object.
 * @param id Task ID assigned by the task manager.
 * @param mode Task execution mode. The `period` unit depends on this mode.
 * @param period Task period value interpreted by the selected mode.
 * @param name Human-readable task name used for diagnostics.
 * @return Pointer to the created task, or `NULL` on failure.
 */
PifTask* pifUart_AttachTxTask(PifUart* p_owner, PifId id, PifTaskMode mode, uint32_t period, const char* name);

#ifdef __cplusplus
}
#endif


#endif  // PIF_UART_H
