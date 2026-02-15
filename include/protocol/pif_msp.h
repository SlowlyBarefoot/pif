#ifndef PIF_MSP_H
#define PIF_MSP_H


#include "communication/pif_uart.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer.h"


#ifndef PIF_MSP_RX_PACKET_SIZE
#define PIF_MSP_RX_PACKET_SIZE		128
#endif

#ifndef PIF_MSP_TX_ANSWER_SIZE
#define PIF_MSP_TX_ANSWER_SIZE		128
#endif

// Timeout used to receive one complete packet.
// 0: no timeout limit.
// 1 or greater: multiplied by the timer unit configured in pifMsp_Init().
// Default is 200 ticks in this header; at 1 ms timer unit, that equals 200 ms.
#ifndef PIF_MSP_RECEIVE_TIMEOUT
#define PIF_MSP_RECEIVE_TIMEOUT		200
#endif

//#define __DEBUG_PACKET__


typedef enum EnPifMspRxState
{
	MRS_IDLE			= 0,
	MRS_HEADER_CHAR_1	= 1,
	MRS_HEADER_CHAR_2	= 2,
	MRS_DIRECTION		= 3,
	MRS_LENGTH			= 4,
	MRS_MESSAGE_TYPE	= 5,
	MRS_DONE			= 6
} PifMspRxState;

typedef enum EnPifMspTxState
{
	MTS_IDLE			= 0,
	MTS_SENDING			= 1
} PifMspTxState;


/**
 * @class StPifMspPacket
 * @brief Represents the StPifMspPacket data structure.
 */
typedef struct StPifMspPacket
{
	uint8_t command;
	uint8_t data_count;
	uint8_t* p_data;
	uint8_t* p_pointer;
} PifMspPacket;


struct StPifMsp;
typedef struct StPifMsp PifMsp;

typedef void (*PifEvtMspReceive)(PifMsp* p_owner, PifMspPacket* p_packet, PifIssuerP p_issuer);
typedef void (*PifEvtMspError)(PifId id);
typedef void (*PifEvtMspOtherPacket)(PifMsp* p_owner, uint8_t data, PifIssuerP p_issuer);

typedef struct StPifMspRx
{
	PifMspRxState state;
	uint8_t* p_packet;
	uint8_t packet_count;
	PifMspPacket packet;
#if PIF_MSP_RECEIVE_TIMEOUT
	PifTimer* p_timer;
#endif
} PifMspRx;

typedef struct StPifMspTx
{
    PifRingBuffer answer_buffer;
    PifMspTxState state;
	uint16_t length;
	uint16_t pos;
} PifMspTx;

/**
 * @class StPifMsp
 * @brief Performs the StPifMsp operation.
 */
struct StPifMsp
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifUart* __p_uart;
    PifMspRx __rx;
    PifMspTx __tx;
	uint8_t __check_xor;
	uint16_t __data_size;
	PifIssuerP __p_issuer;

    // Private Event Function
	PifEvtMspReceive __evt_receive;
	PifEvtMspOtherPacket __evt_other_packet;
};


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMsp_Init
 * @brief Initializes the instance and required runtime resources.
 * @param p_owner Pointer to the protocol instance.
 * @param p_timer Timer manager used by the protocol instance.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_Init(PifMsp* p_owner, PifTimerManager* p_timer, PifId id);

/**
 * @fn pifMsp_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_Clear(PifMsp* p_owner);

/**
 * @fn pifMsp_AttachUart
 * @brief Attaches an interface or callback to the instance.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifMsp_AttachUart(PifMsp* p_owner, PifUart* p_uart);

/**
 * @fn pifMsp_DetachUart
 * @brief Detaches a previously attached interface or callback.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMsp_DetachUart(PifMsp* p_owner);

/**
 * @fn pifMsp_AttachEvtReceive
 * @brief Registers callbacks for normal MSP packets and non-MSP bytes.
 * @param p_owner Pointer to the MSP instance.
 * @param evt_receive Callback invoked when a valid MSP packet is parsed.
 * @param evt_other_packet Callback invoked for bytes that do not belong to an MSP packet.
 * @param p_issuer Issuer object forwarded to callback handlers.
 */
void pifMsp_AttachEvtReceive(PifMsp* p_owner, PifEvtMspReceive evt_receive, PifEvtMspOtherPacket evt_other_packet, PifIssuerP p_issuer);

/**
 * @fn pifMsp_ReadData8
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 8-bit value.
 */
uint8_t pifMsp_ReadData8(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData16
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 16-bit value.
 */
uint16_t pifMsp_ReadData16(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData32
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @return Computed or decoded 32-bit value.
 */
uint32_t pifMsp_ReadData32(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData
 * @brief Reads data from the protocol context.
 * @param p_packet Packet structure used by this operation.
 * @param p_data Pointer to a payload buffer.
 * @param size Number of bytes to process.
 */
void pifMsp_ReadData(PifMspPacket* p_packet, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeAnswer
 * @brief Builds a protocol frame from the provided input.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_AddAnswer8
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer8(PifMsp* p_owner, uint8_t data);

/**
 * @fn pifMsp_AddAnswer16
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer16(PifMsp* p_owner, uint16_t data);

/**
 * @fn pifMsp_AddAnswer32
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param data Input argument used by this API.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer32(PifMsp* p_owner, uint32_t data);

/**
 * @fn pifMsp_AddAnswer
 * @brief Appends data to the current packet buffer.
 * @param p_owner Pointer to the protocol instance.
 * @param p_data Pointer to a payload buffer.
 * @param size Number of bytes to process.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_AddAnswer(PifMsp* p_owner, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeError
 * @brief Builds a protocol frame from the provided input.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_SendAnswer
 * @brief Sends or queues a protocol frame.
 * @param p_owner Pointer to the protocol instance.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMsp_SendAnswer(PifMsp* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_H
