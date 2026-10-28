// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_uart.h
 * @brief UART driver with ring buffers, tasks, flow control and half-duplex direction, as a PifTransport.
 *
 * The platform moves bytes between the hardware and the UART, either through the act_ functions or by calling
 * the pifUart_Put and pifUart_Get functions from its interrupts. A protocol attaches to &uart.parent.
 */
#ifndef PIF_UART_H
#define PIF_UART_H


#include "communication/pif_transport.h"
#include "core/pif_ascii.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_task_manager.h"
#if defined(PIF_COLLECT_SIGNAL) && defined(PIF_COLLECT_SIGNAL_UART)
	#define COLLECT_SIGNAL_UART	///< Defined when PIF_COLLECT_SIGNAL is on and PIF_COLLECT_SIGNAL_UART is too
	#include "core/pif_collect_signal.h"
#endif


#define PIF_UART_SEND_DATA_STATE_INIT		0	///< Nothing was taken from the TX buffer
#define PIF_UART_SEND_DATA_STATE_DATA		1	///< Bit set when a byte was taken
#define PIF_UART_SEND_DATA_STATE_EMPTY		2	///< Bit set when the TX buffer is empty after the call


#ifdef PIF_USE_UART_FLOW_CONTROL

/**
 * @brief Flow control of a UART, set with pifUart_SetFlowControl().
 *
 * HOST is the side that is told to stop sending, DEVICE the side that tells the other to stop.
 */
typedef enum EnPifUartFlowControl
{
	UFC_NONE			= 0,	///< No flow control
	UFC_HOST_SOFTWARE	= 2,	///< Xon / Xoff
	UFC_HOST_HARDWARE	= 3,	///< RTS / CTS, DSR / DTR
	UFC_DEVICE_SOFTWARE	= 4,	///< Xon / Xoff
	UFC_DEVICE_HARDWARE	= 5,	///< RTS / CTS, DSR / DTR

	UFC_HOST_MASK		= 2,	///< Bit of the HOST modes
	UFC_DEVICE_MASK		= 4	///< Bit of the DEVICE modes
} PifUartFlowControl;

#endif	// PIF_USE_UART_FLOW_CONTROL

/**
 * @brief Receive state of the RX task.
 */
typedef enum EnPifUartRxState
{
	URS_IDLE		= 0,	///< Waiting for a first byte
	URS_FIRST		= 1,	///< A first byte came, the task polls quickly
	URS_NEXT		= 2	///< Bytes keep coming
} PifUartRxState;

/**
 * @brief Transmit state.
 */
typedef enum EnPifUartTxState
{
	UTS_IDLE		= 0,	///< Nothing being sent
	UTS_SENDING		= 1	///< A transfer is running
} PifUartTxState;

#ifdef PIF_USE_UART_DIRECTION

/// Direction of a half-duplex line such as RS-485, given to the action attached with pifUart_AttachActDirection().
typedef enum EnPifUartDirection
{
	UD_RX		= 0,	///< Receive: the driver is off and the line is left to the other side, e.g. RS-485 DE low and /RE low
	UD_TX		= 1		///< Transmit: this side drives the line, e.g. RS-485 DE high (/RE high too when it is tied to DE)
} PifUartDirection;

#endif	// PIF_USE_UART_DIRECTION

#ifdef COLLECT_SIGNAL_UART

/**
 * @brief Signals of a UART for pifUart_SetCsFlag() and pifUart_ResetCsFlag().
 *
 * The _IDX values index the signals, the _BIT values select them.
 */
typedef enum EnPifUartCsFlag
{
	UA_CSF_OFF			= 0,

	UA_CSF_FC_IDX		= 0,	///< _fc_state: ON while the other side may send. Only with PIF_USE_UART_FLOW_CONTROL.
	UA_CSF_TX_IDX		= 1,	///< PifUartTxState
	UA_CSF_DIR_IDX		= 2,	///< _direction, e.g. RS-485 DE. Only with PIF_USE_UART_DIRECTION.

	UA_CSF_FC_BIT		= 1,
	UA_CSF_TX_BIT		= 2,
	UA_CSF_DIR_BIT		= 4,
	UA_CSF_ALL_BIT		= 7,

	UA_CSF_COUNT		= 3
} PifUartCsFlag;

#endif	// COLLECT_SIGNAL_UART


struct StPifUart;
typedef struct StPifUart PifUart;

/**
 * @brief Changes the baud rate of the hardware.
 *
 * Returns FALSE when the hardware cannot take it.
 */
typedef BOOL (*PifActUartSetBaudrate)(PifUart* p_uart, uint32_t baudrate);
/**
 * @brief Takes up to size received bytes from the hardware into p_data.
 *
 * Returns the number of bytes taken.
 */
typedef uint16_t (*PifActUartReceiveData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
/**
 * @brief Gives up to size bytes from p_data to the hardware.
 *
 * Returns the number of bytes the hardware took.
 */
typedef uint16_t (*PifActUartSendData)(PifUart* p_uart, uint8_t* p_data, uint16_t size);
/**
 * @brief Starts a transfer of the TX buffer by interrupts or DMA.
 *
 * The hardware then takes the bytes with pifUart_GetTxByte() or pifUart_StartGetTxData(). Returns FALSE when it
 * could not start.
 */
typedef BOOL (*PifActUartStartTransfer)(PifUart* p_uart);
/**
 * @brief Gets how full a hardware buffer is, in percent.
 */
typedef uint8_t (*PifActUartGetRate)(PifUart* p_uart);

#ifdef PIF_USE_UART_DIRECTION
/**
 * @brief Drives the direction of a half-duplex line, such as the DE and /RE pins of an RS-485 transceiver.
 */
typedef void (*PifActUartDirection)(PifUartDirection direction);
#endif

#ifdef PIF_USE_UART_FLOW_CONTROL
/**
 * @brief Tells the other side to stop (OFF) or to go on (ON) sending, by RTS or Xon/Xoff.
 */
typedef void (*PifActUartDeviceFlowState)(PifUart* p_uart, SWITCH state);
/**
 * @brief Tells the client that the other side lets this side send (ON) or not (OFF).
 */
typedef void (*PifEvtUartHostFlowState)(void* p_client, SWITCH state);
#endif


/**
 * @class StPifUart
 * @brief UART abstraction that encapsulates buffers, flow control, and task hooks.
 *
 * parent makes the UART a PifTransport. A protocol attaches to &uart.parent with pifTransport_AttachClient(),
 * and a UART has one client at a time.
 * The flow control members and functions are built only with PIF_USE_UART_FLOW_CONTROL. Without it the flow
 * state of the transport is always ON.
 * The direction control for a half-duplex line such as RS-485 is built only with PIF_USE_UART_DIRECTION.
 */
struct StPifUart
{
	PifTransport parent;	///< Transport the protocol uses. Must stay the first member.

	// Public Member Variable
#ifdef PIF_USE_UART_FLOW_CONTROL
	uint8_t fc_limit;							///< Range: 0% ~ 100%, default: 50%
#endif
#ifdef PIF_USE_UART_DIRECTION
	uint8_t dir_delay;							///< Byte times from the end of a transfer to UD_RX, default: 2
#endif

	// Public Action Function
    PifActUartSetBaudrate act_set_baudrate;	///< Changes the baud rate. Used by pifUart_SetBaudrate(). May be NULL.
	PifActUartReceiveData act_receive_data;	///< Polls received bytes from the hardware, when it has no RX interrupt. May be NULL.
    PifActUartSendData act_send_data;	///< Sends bytes straight to the hardware, without a TX buffer. May be NULL.
    PifActUartStartTransfer act_start_transfer;	///< Starts an interrupt or DMA transfer of the TX buffer. May be NULL.
    PifActUartGetRate act_get_tx_rate;	///< With act_send_data: free space of the hardware TX buffer, 100 when all is sent. May be NULL.
#ifdef PIF_USE_UART_FLOW_CONTROL
    PifActUartGetRate act_get_rx_rate;	///< Fill level of the hardware RX buffer, against fc_limit. NULL uses the RX buffer.
    PifActUartDeviceFlowState act_device_flow_state;	///< Signals the flow state to the other side in UFC_DEVICE_HARDWARE. May be NULL.
#endif

	// Read-only Member Variable
    PifId _id;	///< Instance ID
    uint32_t _baudrate;	///< Baud rate
#ifdef PIF_USE_UART_FLOW_CONTROL
    PifUartFlowControl _flow_control;	///< Flow control mode
#endif
    uint8_t _frame_size;	///< Bytes the transport sends at once
    uint16_t _transfer_time;	///< Time of one byte in microseconds, from the baud rate
    PifRingBuffer* _p_tx_buffer;	///< TX ring buffer, or NULL
    PifRingBuffer* _p_rx_buffer;	///< RX ring buffer, or NULL
    PifTask* _p_tx_task;	///< TX task, or NULL
    PifTask* _p_rx_task;	///< RX task, or NULL
#ifdef PIF_USE_UART_FLOW_CONTROL
    SWITCH _fc_state;	///< Flow state: ON while sending is allowed
#endif
#ifdef PIF_USE_UART_DIRECTION
    PifUartDirection _direction;	///< Current direction of a half-duplex line
#endif

	// Private Member Variable
    void* __p_client;
    volatile PifUartTxState __tx_state;
    volatile PifUartRxState __rx_state;
    BOOL __rx_taken;							///< Bytes were read in the current run of the RX task
#ifdef PIF_USE_UART_DIRECTION
    BOOL __dir_auto;							///< The UART drove the line for a transfer and gives it back
    BOOL __dir_wait;							///< The transfer is done and the line waits for dir_delay
    uint32_t __tx_end_time;

	// Private Action Function
    PifActUartDirection __act_direction;
#endif

    // Private Event Function
    /// Set by pifTransport_AttachClient() and cleared by pifTransport_DetachClient().
    PifEvtTransportParsing __evt_parsing;
    PifEvtTransportSending __evt_sending;
    PifEvtTransportAbortRx __evt_abort_rx;		///< Called by pifUart_AbortRx() with the client.
#ifdef PIF_USE_UART_FLOW_CONTROL
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
 * @brief Initializes a UART object with a logical ID and default baud rate.
 * @param p_owner Pointer to the UART object to initialize.
 * @param id Logical ID assigned to the UART instance.
 * @param baudrate Initial baud rate in bits per second.
 * @return `TRUE` if initialization succeeds, otherwise `FALSE`.
 */
BOOL pifUart_Init(PifUart* p_owner, PifId id, uint32_t baudrate);

/**
 * @brief Releases resources owned by the UART object.
 * @param p_owner Pointer to an initialized UART object.
 */
void pifUart_Clear(PifUart* p_owner);

/**
 * @brief Allocates an internal ring buffer for RX data.
 * @param p_owner Pointer to the UART object.
 * @param rx_size Buffer capacity in bytes.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AllocRxBuffer(PifUart* p_owner, uint16_t rx_size);

/**
 * @brief Assigns an external memory block as the RX ring buffer.
 * @param p_owner Pointer to the UART object.
 * @param rx_size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return `TRUE` if assignment succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AssignRxBuffer(PifUart* p_owner, uint16_t rx_size, uint8_t* p_buffer);

/**
 * @brief Allocates an internal ring buffer for TX data.
 * @param p_owner Pointer to the UART object.
 * @param tx_size Buffer capacity in bytes.
 * @return `TRUE` if allocation succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AllocTxBuffer(PifUart* p_owner, uint16_t tx_size);

/**
 * @brief Assigns an external memory block as the TX ring buffer.
 * @param p_owner Pointer to the UART object.
 * @param tx_size Buffer capacity in bytes.
 * @param p_buffer Caller-provided buffer memory.
 * @return `TRUE` if assignment succeeds, otherwise `FALSE`.
 */
BOOL pifUart_AssignTxBuffer(PifUart* p_owner, uint16_t tx_size, uint8_t* p_buffer);

/**
 * @brief Sets the transfer frame size used for scheduling and timing.
 * @param p_owner Pointer to the UART object.
 * @param frame_size Number of bits per frame (data/parity/stop composition).
 * @return `TRUE` if the value is accepted, otherwise `FALSE`.
 */
BOOL pifUart_SetFrameSize(PifUart* p_owner, uint8_t frame_size);

/**
 * @brief Changes UART baud rate through the attached hardware callback.
 * @param p_owner Pointer to the UART object.
 * @param baudrate New baud rate in bits per second.
 * @return `TRUE` if the hardware update succeeds, otherwise `FALSE`.
 */
BOOL pifUart_ChangeBaudrate(PifUart* p_owner, uint32_t baudrate);

#ifdef PIF_USE_UART_DIRECTION

/**
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
 * @brief Switches the external RX/TX direction hardware through the attached action, if any. A transfer
 *        switches it by itself, so this is only for holding the line: UD_TX keeps it driven, through
 *        transfers too, until UD_RX is set here again.
 * @param p_owner Pointer to the UART object.
 * @param direction Direction to apply.
 */
void pifUart_SetDirection(PifUart* p_owner, PifUartDirection direction);

#endif	// PIF_USE_UART_DIRECTION

#ifdef PIF_USE_UART_FLOW_CONTROL

/**
 * @brief Resets internal flow-control state to its default condition.
 * @param p_owner Pointer to the UART object.
 */
void pifUart_ResetFlowControl(PifUart* p_owner);

/**
 * @brief Configures UART flow-control mode and host-state notification callback.
 * @param p_owner Pointer to the UART object.
 * @param flow_control Flow-control mode to apply.
 * @param evt_host_flow_state Callback invoked when host flow state changes.
 */
void pifUart_SetFlowControl(PifUart* p_owner, PifUartFlowControl flow_control, PifEvtUartHostFlowState evt_host_flow_state);

/**
 * @brief Changes local RX flow state and notifies the configured control path.
 * @param p_owner Pointer to the UART object.
 * @param state New receive permission state.
 * @return `TRUE` if the state transition is accepted, otherwise `FALSE`.
 */
BOOL pifUart_ChangeRxFlowState(PifUart* p_owner, SWITCH state);

/**
 * @brief Applies remote TX flow state to local transmission control.
 * @param p_owner Pointer to the UART object.
 * @param state Remote flow state signaled by the counterpart.
 */
void pifUart_SigTxFlowState(PifUart* p_owner, SWITCH state);

#endif	// PIF_USE_UART_FLOW_CONTROL

/**
 * @brief Returns the number of bytes currently queued in the RX buffer.
 * @param p_owner Pointer to the UART object.
 * @return Number of unread bytes in the RX ring buffer.
 */
uint16_t pifUart_GetFillSizeOfRxBuffer(PifUart* p_owner);

/**
 * @brief Returns the number of bytes currently queued in the TX buffer.
 * @param p_owner Pointer to the UART object.
 * @return Number of pending bytes in the TX ring buffer.
 */
uint16_t pifUart_GetFillSizeOfTxBuffer(PifUart* p_owner);

/**
 * @brief Pushes a single received byte into the RX processing path.
 * @param p_owner Pointer to the UART object.
 * @param data Byte received from hardware.
 * @return `TRUE` if the byte is accepted, otherwise `FALSE`.
 */
BOOL pifUart_PutRxByte(PifUart* p_owner, uint8_t data);

/**
 * @brief Pushes a block of received bytes into the RX processing path.
 * @param p_owner Pointer to the UART object.
 * @param p_data Input buffer containing received bytes.
 * @param length Number of bytes to process.
 * @return `TRUE` if all bytes are accepted, otherwise `FALSE`.
 */
BOOL pifUart_PutRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @brief Retrieves one byte from the TX queue.
 * @param p_owner Pointer to the UART object.
 * @param p_data Output pointer that receives the byte.
 * @return Non-zero when a byte is returned, zero when no data is available.
 */
uint8_t pifUart_GetTxByte(PifUart* p_owner, uint8_t* p_data);

/**
 * @brief Starts a contiguous TX-buffer read operation.
 * @param p_owner Pointer to the UART object.
 * @param pp_data Output pointer to the start address of available TX data.
 * @param p_length Output pointer to the contiguous data length.
 * @return Non-zero when data is available, zero otherwise.
 */
uint8_t pifUart_StartGetTxData(PifUart* p_owner, uint8_t** pp_data, uint16_t *p_length);

/**
 * @brief Finalizes a TX-buffer read operation after bytes are transmitted.
 * @param p_owner Pointer to the UART object.
 * @param length Number of bytes consumed from the TX buffer.
 * @return Remaining-status flag used by the TX state machine.
 */
uint8_t pifUart_EndGetTxData(PifUart* p_owner, uint16_t length);

/**
 * @brief Pulls bytes from hardware through `act_receive_data` and stores them into RX flow.
 * @param p_owner Pointer to the UART object.
 * @param p_data Temporary buffer used to receive raw bytes from hardware.
 * @param length Requested maximum receive length.
 * @return Number of bytes actually received.
 */
uint16_t pifUart_ReceiveRxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @brief Sends bytes to hardware through `act_send_data`.
 * @param p_owner Pointer to the UART object.
 * @param p_data Buffer containing bytes to transmit.
 * @param length Number of bytes requested for transmission.
 * @return Number of bytes actually transmitted.
 */
uint16_t pifUart_SendTxData(PifUart* p_owner, uint8_t* p_data, uint16_t length);

/**
 * @brief Empties the RX buffer and calls the evt_abort_rx that the client gave, if any, so the frame it
 *        was parsing is dropped.
 * @param p_owner Pointer to the UART object.
 */
void pifUart_AbortRx(PifUart* p_owner);

/**
 * @brief Checks and progresses pending TX transfer state.
 * @param p_owner Pointer to the UART object.
 * @return `TRUE` while transfer is active or newly started, otherwise `FALSE`.
 */
BOOL pifUart_CheckTxTransfer(PifUart* p_owner);

/**
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
 * @brief Adds the selected signals of the UART to pifCollectSignal as channels.
 * @param p_owner Pointer to the UART object.
 * @param index Device of the instance for a module that controls several, as PifSolenoid; PIF_CS_INDEX_ALL
 *        or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to add.
 * @return `TRUE` on success, otherwise `FALSE`.
 */
BOOL pifUart_SetCsFlag(PifUart* p_owner, uint8_t index, PifUartCsFlag flag);

/**
 * @brief Removes the selected signals of the UART from pifCollectSignal.
 * @param p_owner Pointer to the UART object.
 * @param index Device of the instance for a module that controls several, as PifSolenoid; PIF_CS_INDEX_ALL
 *        or any value from its count on for all. Ignored by a module of one device.
 * @param flag Bit mask of the signals to remove.
 */
void pifUart_ResetCsFlag(PifUart* p_owner, uint8_t index, PifUartCsFlag flag);

#endif	// COLLECT_SIGNAL_UART

#ifdef __cplusplus
}
#endif


#endif  // PIF_UART_H
