// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_can.h
 * @brief CAN and CAN FD frame driver.
 *
 * PifCan queues frames between the hardware and the tasks. The hardware driver hands each received frame to
 * pifCan_PutRxFrame() and sends the frames that act_send_frame is given.
 *
 * A layer above, such as PifCanStream, joins as a PifCanClient: received frames are offered to the clients
 * in the order they were added, and the first that takes one keeps it. A frame no client takes goes to
 * evt_receive.
 */
#ifndef PIF_CAN_H
#define PIF_CAN_H


#include "core/pif_ring_data.h"
#include "core/pif_task_manager.h"


// Largest data of a frame. 8 is enough when only Classic CAN is used, and saves memory in every queue.
#ifndef PIF_CAN_MAX_DATA_SIZE
#define PIF_CAN_MAX_DATA_SIZE		64
#endif

#define PIF_CAN_STD_ID_MASK			0x7FFUL
#define PIF_CAN_EXT_ID_MASK			0x1FFFFFFFUL


typedef enum EnPifCanFlag
{
	CAN_F_NONE			= 0,
	CAN_F_EXTENDED		= 1,	// 29 bit ID, otherwise 11 bit
	CAN_F_RTR			= 2,	// Remote frame, Classic CAN only
	CAN_F_FD			= 4,	// CAN FD frame
	CAN_F_BRS			= 8		// Bit rate switch in the data phase, CAN FD only
} PifCanFlag;

typedef enum EnPifCanBusState
{
	CBS_ERROR_ACTIVE	= 0,
	CBS_ERROR_PASSIVE	= 1,
	CBS_BUS_OFF			= 2
} PifCanBusState;

/**
 * @class StPifCanFrame
 * @brief One CAN or CAN FD frame.
 */
typedef struct StPifCanFrame
{
	uint32_t id;
	uint8_t flags;		// PifCanFlag
	uint8_t length;		// 0 to 8, and with CAN_F_FD also 12, 16, 20, 24, 32, 48 or 64
	uint8_t data[PIF_CAN_MAX_DATA_SIZE];
} PifCanFrame;

struct StPifCan;
typedef struct StPifCan PifCan;

struct StPifCanClient;
typedef struct StPifCanClient PifCanClient;

typedef BOOL (*PifActCanSendFrame)(PifCan *p_can, const PifCanFrame *p_frame);

typedef void (*PifEvtCanReceive)(PifCan *p_can, const PifCanFrame *p_frame);
typedef void (*PifEvtCanBusState)(PifCan *p_can, PifCanBusState state);

typedef BOOL (*PifEvtCanClientReceive)(PifCanClient *p_client, const PifCanFrame *p_frame);
typedef uint32_t (*PifEvtCanClientTask)(PifCanClient *p_client);

/**
 * @class StPifCanClient
 * @brief Hooks of a layer above PifCan. Embed it in that layer and add it with pifCan_AddClient().
 *        Each event may be NULL. A client is not added or removed from inside these events.
 */
struct StPifCanClient
{
	// Public Event Function
	PifEvtCanClientReceive evt_receive;	// Offered a received frame in the RX task. TRUE takes it.
	PifEvtCanClientTask evt_rx_done;	// Called after the frames of each RX task run. Returns the time in us
										// after which the RX task is to run again, or 0 for no need.
	PifEvtCanClientTask evt_sending;	// Called in each TX task run while the bus is not off, to queue frames
										// with pifCan_SendFrame(). Returns the time in us after which it is to
										// be called again, or 0 for no need.

	// Private Member Variable
	PifCanClient *__p_next;
};

/**
 * @class StPifCan
 * @brief CAN controller with a receive queue, a send queue and the clients above it.
 */
struct StPifCan
{
	// Public Action Function
	PifActCanSendFrame act_send_frame;	// Hands one frame to the hardware. FALSE when it cannot take one now.

	// Public Event Function
	PifEvtCanReceive evt_receive;		// A received frame that no client takes.
	PifEvtCanBusState evt_bus_state;	// Called from pifCan_SigBusState(), in its context.

	// Read-only Member Variable
	PifId _id;
	uint32_t _bitrate;					// Nominal bit rate.
	uint32_t _data_bitrate;				// Bit rate of the data phase of a frame with CAN_F_BRS.
	PifCanBusState _bus_state;
	uint16_t _rx_lost;					// Frames dropped because the receive queue was full.
	PifTask *_p_rx_task;
	PifTask *_p_tx_task;

	// Private Member Variable
	PifRingData __rx_queue;				// Of PifCanFrame. Written by pifCan_PutRxFrame(), read by the RX task.
	PifRingData __tx_queue;				// Of PifCanFrame. Written and read in task context.
	PifCanClient *__p_clients;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifCan_Init
 * @brief Initializes a CAN controller and allocates its queues.
 * @param p_owner Pointer to the CAN object.
 * @param id Logical ID, or PIF_ID_AUTO.
 * @param bitrate Nominal bit rate in bit/s.
 * @param data_bitrate Bit rate of the CAN FD data phase in bit/s, or 0 when it is the nominal one.
 * @param rx_count Frames the receive queue holds.
 * @param tx_count Frames the send queue holds.
 * @return TRUE on success, FALSE with pif_error set otherwise.
 */
BOOL pifCan_Init(PifCan *p_owner, PifId id, uint32_t bitrate, uint32_t data_bitrate, uint16_t rx_count,
		uint16_t tx_count);

/**
 * @fn pifCan_Clear
 * @brief Removes the tasks and frees the queues. The clients must be removed first.
 * @param p_owner Pointer to the CAN object.
 */
void pifCan_Clear(PifCan *p_owner);

/**
 * @fn pifCan_SetBitrate
 * @brief Records new bit rates after the hardware has been changed to them. pifCan_GetFrameTime() uses them.
 * @param p_owner Pointer to the CAN object.
 * @param bitrate Nominal bit rate in bit/s.
 * @param data_bitrate Bit rate of the CAN FD data phase in bit/s, or 0 when it is the nominal one.
 * @return TRUE on success, FALSE when bitrate is 0.
 */
BOOL pifCan_SetBitrate(PifCan *p_owner, uint32_t bitrate, uint32_t data_bitrate);

/**
 * @fn pifCan_AttachRxTask
 * @brief Adds the task that takes received frames from the queue to the clients and evt_receive.
 * @param p_owner Pointer to the CAN object.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param mode Task mode. pifCan_PutRxFrame() triggers the task, so TM_EXTERNAL with no period is enough.
 * @param period Task period.
 * @param name Task name, or NULL for "CanRx".
 * @return The task, or NULL with pif_error set.
 */
PifTask *pifCan_AttachRxTask(PifCan *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name);

/**
 * @fn pifCan_AttachTxTask
 * @brief Adds the task that lets the clients queue frames and hands queued frames to act_send_frame.
 * @param p_owner Pointer to the CAN object.
 * @param id Task ID, or PIF_ID_AUTO.
 * @param mode Task mode. A client that never calls pifCan_TriggerTx() needs TM_PERIOD.
 * @param period Task period.
 * @param name Task name, or NULL for "CanTx".
 * @return The task, or NULL with pif_error set.
 */
PifTask *pifCan_AttachTxTask(PifCan *p_owner, PifId id, PifTaskMode mode, uint32_t period, const char *name);

/**
 * @fn pifCan_SendFrame
 * @brief Queues a frame to send. Not from an interrupt: the clients write the same queue from the TX task.
 * @param p_owner Pointer to the CAN object.
 * @param p_frame Frame to send. It is copied.
 * @return TRUE on success. FALSE with E_INVALID_PARAM for a frame that cannot exist, or with
 *         E_OVERFLOW_BUFFER when the send queue is full.
 */
BOOL pifCan_SendFrame(PifCan *p_owner, const PifCanFrame *p_frame);

/**
 * @fn pifCan_GetTxRemain
 * @brief Gets how many more frames the send queue can take.
 * @param p_owner Pointer to the CAN object.
 * @return Free frames in the send queue.
 */
uint16_t pifCan_GetTxRemain(PifCan *p_owner);

/**
 * @fn pifCan_TriggerTx
 * @brief Asks the TX task to run soon, for a client that has something new to send. Safe to call from an
 *        interrupt.
 * @param p_owner Pointer to the CAN object.
 */
void pifCan_TriggerTx(PifCan *p_owner);

/**
 * @fn pifCan_PutRxFrame
 * @brief Queues a received frame. The hardware driver calls it, from an interrupt if it likes.
 * @param p_owner Pointer to the CAN object.
 * @param p_frame Received frame. It is copied.
 * @return TRUE on success. FALSE when the frame is too long or the queue is full; a full queue counts the
 *         frame in _rx_lost.
 */
BOOL pifCan_PutRxFrame(PifCan *p_owner, const PifCanFrame *p_frame);

/**
 * @fn pifCan_SigTxComplete
 * @brief Tells that the hardware can take another frame, so that the send queue moves on without waiting
 *        for the period. Safe to call from an interrupt.
 * @param p_owner Pointer to the CAN object.
 */
void pifCan_SigTxComplete(PifCan *p_owner);

/**
 * @fn pifCan_SigBusState
 * @brief Records the error state reported by the hardware and calls evt_bus_state when it changes.
 *        The TX task neither sends nor calls the clients while the bus is off.
 * @param p_owner Pointer to the CAN object.
 * @param state New error state.
 */
void pifCan_SigBusState(PifCan *p_owner, PifCanBusState state);

/**
 * @fn pifCan_GetFrameTime
 * @brief Gets the time a frame takes on the bus. Bit stuffing is not counted.
 * @param p_owner Pointer to the CAN object.
 * @param flags PifCanFlag of the frame.
 * @param length Data length of the frame.
 * @return Time in us.
 */
uint32_t pifCan_GetFrameTime(PifCan *p_owner, uint8_t flags, uint8_t length);

/**
 * @fn pifCan_DlcToLength
 * @brief Converts a DLC code to a data length, by the CAN FD table. Codes above 8 mean 8 in Classic CAN.
 * @param dlc DLC code, 0 to 15.
 * @return Data length in bytes.
 */
uint8_t pifCan_DlcToLength(uint8_t dlc);

/**
 * @fn pifCan_LengthToDlc
 * @brief Converts a data length to the smallest DLC code that holds it. A hardware driver pads the data up to
 *        pifCan_DlcToLength() of the result.
 * @param length Data length in bytes, up to 64.
 * @return DLC code, 0 to 15.
 */
uint8_t pifCan_LengthToDlc(uint8_t length);

/**
 * @fn pifCan_AddClient
 * @brief Adds a client after those added before it, which are offered each received frame first.
 * @param p_owner Pointer to the CAN object.
 * @param p_client Client to add. Its events are set before or after, but before the tasks next run.
 * @return TRUE on success, FALSE with E_ALREADY_ATTACHED when it is already added.
 */
BOOL pifCan_AddClient(PifCan *p_owner, PifCanClient *p_client);

/**
 * @fn pifCan_RemoveClient
 * @brief Removes a client. Nothing happens when it is not added.
 * @param p_owner Pointer to the CAN object.
 * @param p_client Client to remove.
 */
void pifCan_RemoveClient(PifCan *p_owner, PifCanClient *p_client);

#ifdef __cplusplus
}
#endif


#endif  // PIF_CAN_H
