// SPDX-License-Identifier: BSD-3-Clause
#ifndef PIF_UART_H
#define PIF_UART_H


#include "communication/pif_transport.h"
#include "core/pif_ascii.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"
#if defined(PIF_COLLECT_SIGNAL) && !defined(PIF_COLLECT_SIGNAL_NO_UART)
	#define COLLECT_SIGNAL_UART
	#include "core/pif_collect_signal.h"
#endif


#define PIF_UART_SEND_DATA_STATE_INIT		0
#define PIF_UART_SEND_DATA_STATE_DATA		1
#define PIF_UART_SEND_DATA_STATE_EMPTY		2


#ifndef PIF_UART_NO_FLOW_CONTROL

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

#endif	// PIF_UART_NO_FLOW_CONTROL

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

#ifndef PIF_UART_NO_DIRECTION

// Direction of a half-duplex line such as RS-485, given to the action attached with pifUart_AttachActDirection().
typedef enum EnPifUartDirection
{
	UD_RX		= 0,	// Receive: the driver is off and the line is left to the other side, e.g. RS-485 DE low and /RE low
	UD_TX		= 1		// Transmit: this side drives the line, e.g. RS-485 DE high (/RE high too when it is tied to DE)
} PifUartDirection;

#endif	// PIF_UART_NO_DIRECTION

#ifdef COLLECT_SIGNAL_UART

typedef enum EnPifUartCsFlag
{
	UA_CSF_OFF			= 0,

	UA_CSF_FC_IDX		= 0,	// _fc_state: ON while the other side may send. Not with PIF_UART_NO_FLOW_CONTROL.
	UA_CSF_TX_IDX		= 1,	// PifUartTxState
	UA_CSF_DIR_IDX		= 2,	// _direction, e.g. RS-485 DE. Not with PIF_UART_NO_DIRECTION.

	UA_CSF_FC_BIT		= 1,
	UA_CSF_TX_BIT		= 2,
	UA_CSF_DIR_BIT		= 4,
	UA_CSF_ALL_BIT		= 7,

	UA_CSF_COUNT		= 3
} PifUartCsFlag;

#endif	// COLLECT_SIGNAL_UART


struct StPifUart;
typedef struct StPifUart PifUart;

typedef BOOL (*PifActUartSetBaudrate)(PifUart* p_uart, uint32_t baudrate);
typedef uint16_t (*PifActUartReceiveData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
typedef uint16_t (*PifActUartSendData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
typedef BOOL (*PifActUartStartTransfer)(PifUart* p_uart);
typedef uint8_t (*PifActUartGetRate)(PifUart* p_uart);

#ifndef PIF_UART_NO_DIRECTION
typedef void (*PifActUartDirection)(PifUartDirection direction);
#endif

#ifndef PIF_UART_NO_FLOW_CONTROL
typedef void (*PifActUartDeviceFlowState)(PifUart* p_uart, SWITCH state);
typedef void (*PifEvtUartHostFlowState)(void* p_client, SWITCH state);
#endif


/**
 * @class StPifUart
 * @brief UART abstraction that encapsulates buffers, flow control, and task hooks.
 *
 * parent makes the UART a PifTransport. A protocol attaches to &uart.parent with pifTransport_AttachClient(),
 * and a UART has one client at a time.
 * With PIF_UART_NO_FLOW_CONTROL the flow control members and functions are left out of the build, and the
 * flow state of the transport is always ON.
 * With PIF_UART_NO_DIRECTION the direction control for a half-duplex line such as RS-485 is left out.
 */
struct StPifUart
{
	PifTransport parent;

	// Public Member Variable
#ifndef PIF_UART_NO_FLOW_CONTROL
	uint8_t fc_limit;							// Range: 0% ~ 100%, default: 50%
#endif
#ifndef PIF_UART_NO_DIRECTION
	uint8_t dir_delay;							// Byte times from the end of a transfer to UD_RX, default: 2
#endif

	// Public Action Function
    PifActUartSetBaudrate act_set_baudrate;
	PifActUartReceiveData act_receive_data;
    PifActUartSendData act_send_data;
    PifActUartStartTransfer act_start_transfer;
    PifActUartGetRate act_get_tx_rate;
#ifndef PIF_UART_NO_FLOW_CONTROL
    PifActUartGetRate act_get_rx_rate;
    PifActUartDeviceFlowState act_device_flow_state;
#endif

	// Read-only Member Variable
    PifId _id;
    uint32_t _baudrate;
#ifndef PIF_UART_NO_FLOW_CONTROL
    PifUartFlowControl _flow_control;
#endif
    uint8_t _frame_size;
    uint16_t _transfer_time;
    PifRingBuffer* _p_tx_buffer;
    PifRingBuffer* _p_rx_buffer;
    PifTask* _p_tx_task;
    PifTask* _p_rx_task;
#ifndef PIF_UART_NO_FLOW_CONTROL
    SWITCH _fc_state;
#endif
#ifndef PIF_UART_NO_DIRECTION
    PifUartDirection _direction;
#endif

	// Private Member Variable
    void* __p_client;
    volatile PifUartTxState __tx_state;
    volatile PifUartRxState __rx_state;
#ifndef PIF_UART_NO_DIRECTION
    BOOL __dir_auto;							// The UART drove the line for a transfer and gives it back
    BOOL __dir_wait;							// The transfer is done and the line waits for dir_delay
    uint32_t __tx_end_time;

	// Private Action Function
    PifActUartDirection __act_direction;
#endif

    // Private Event Function
    // Set by pifTransport_AttachClient() and cleared by pifTransport_DetachClient().
    PifEvtTransportParsing __evt_parsing;
    PifEvtTransportSending __evt_sending;
    PifEvtTransportAbortRx __evt_abort_rx;		// Called by pifUart_AbortRx() with the client.
#ifndef PIF_UART_NO_FLOW_CONTROL
    PifEvtUartHostFlowState __evt_host_flow_state;
#endif
#ifdef COLLECT_SIGNAL_UART
	PifCollectSignalChannel __cs[UA_CSF_COUNT];
#endif
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

#ifndef PIF_UART_NO_DIRECTION

/**
 * @fn pifUart_AttachActDirection
 * @brief Attaches a callback that drives the direction of a half-duplex line, e.g. the DE/RE pins of an
 *        RS-485 transceiver. From then on the UART switches it by itself while it is UD_RX: to UD_TX before
 *        the first byte of a transfer goes to the hardware or the TX buffer, and back to UD_RX dir_delay
 *        byte times after pifUart_CheckTxTransfer() reports the transfer done. Going back needs the TX task,
 *        and with act_send_data it needs act_get_tx_rate too, as the transfer counts as done once handed
 *        over without it.
 * @param p_owner Pointer to the UART object.
 * @param act_direction Callback that switches the external data direction.
 * @param init_state Initial direction state applied after attaching. Give UD_RX for the switching above,
 *        as UD_TX holds the line as pifUart_SetDirection() does.
 */
void pifUart_AttachActDirection(PifUart* p_owner, PifActUartDirection act_direction, PifUartDirection init_state);

/**
 * @fn pifUart_SetDirection
 * @brief Switches the external RX/TX direction hardware through the attached action, if any. A transfer
 *        switches it by itself, so this is only for holding the line: UD_TX keeps it driven, through
 *        transfers too, until UD_RX is set here again.
 * @param p_owner Pointer to the UART object.
 * @param direction Direction to apply.
 */
void pifUart_SetDirection(PifUart* p_owner, PifUartDirection direction);

#endif	// PIF_UART_NO_DIRECTION

#ifndef PIF_UART_NO_FLOW_CONTROL

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

#endif	// PIF_UART_NO_FLOW_CONTROL

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
 * @brief Empties the RX buffer and calls the evt_abort_rx that the client gave, if any, so the frame it
 *        was parsing is dropped.
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

#ifdef COLLECT_SIGNAL_UART

/**
 * @fn pifUart_SetCsFlag
 * @brief Adds the selected signals of the UART to pifCollectSignal as channels.
 * @param p_owner Pointer to the UART object.
 * @param flag Bit mask of the signals to add.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifUart_SetCsFlag(PifUart* p_owner, PifUartCsFlag flag);

/**
 * @fn pifUart_ResetCsFlag
 * @brief Removes the selected signals of the UART from pifCollectSignal.
 * @param p_owner Pointer to the UART object.
 * @param flag Bit mask of the signals to remove.
 */
void pifUart_ResetCsFlag(PifUart* p_owner, PifUartCsFlag flag);

#endif	// COLLECT_SIGNAL_UART

#ifdef __cplusplus
}
#endif


#endif  // PIF_UART_H
