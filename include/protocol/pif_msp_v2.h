#ifndef PIF_MSP_V2_H
#define PIF_MSP_V2_H


#include "protocol/pif_msp.h"


// MSPv1 command that carries an MSPv2 frame as its payload (MSPv2 over MSPv1).
#define PIF_MSP_V2_FRAME_ID			255

// MSPv1 size byte of a jumbo frame. The real size follows in 16 bits.
#define PIF_MSP_V1_JUMBO_SIZE		255

// Most bytes a frame adds to its payload: '$', 'M', direction, the MSPv1 size and command, the jumbo size,
// the MSPv2 header and both checksums of an MSPv2 over MSPv1 jumbo frame.
#define PIF_MSP_V2_MAX_OVERHEAD		14


typedef enum EnPifMspV2RxState
{
	MV2RS_IDLE					= 0,
	MV2RS_HEADER_START			= 1,
	MV2RS_HEADER_M				= 2,
	MV2RS_HEADER_X				= 3,

	MV2RS_HEADER_V1				= 4,
	MV2RS_PAYLOAD_V1			= 5,
	MV2RS_CHECKSUM_V1			= 6,

	MV2RS_HEADER_V2_OVER_V1		= 7,
	MV2RS_PAYLOAD_V2_OVER_V1	= 8,
	MV2RS_CHECKSUM_V2_OVER_V1	= 9,

	MV2RS_HEADER_V2_NATIVE		= 10,
	MV2RS_PAYLOAD_V2_NATIVE		= 11,
	MV2RS_CHECKSUM_V2_NATIVE	= 12
} PifMspV2RxState;


/**
 * @class StPifMspV2
 * @brief MSP on top of pif_msp that also takes MSPv2 frames, either native ($X) or carried in an MSPv1 frame,
 *        and answers in the frame each question came in, with a jumbo frame when an MSPv1 payload does not fit
 *        in 254 bytes.
 *
 * Received packets, the non-MSP bytes, the buffers and the sending all stay with the pif_msp parent in _msp:
 * pifMsp_AttachEvtReceive(), pifMsp_AssignRxBuffer(), pifMsp_AssignAnswerBuffer(), pifMsp_GetAnswer() and
 * the pifMsp_ReadData*() functions are used on &_msp. Only the parser and the frame builder are replaced.
 * The payload of a received packet goes to the receive buffer of _msp without the header, so the whole
 * buffer is payload. Answer with pifMspV2_MakeAnswer() rather than the pifMsp_MakeAnswer() family, which
 * builds MSPv1 frames only.
 */
typedef struct StPifMspV2
{
	// Public Member Variable

	// Read-only Member Variable
	PifMsp _msp;

	// Private Member Variable
	uint8_t __header[7];		// MSPv1 size and command, then the MSPv2 flags, command and size
	uint16_t __offset;
	uint8_t __checksum1;		// MSPv1 XOR
	uint8_t __checksum2;		// MSPv2 CRC-8/DVB-S2
} PifMspV2;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifMspV2_Init
 * @brief Initializes the pif_msp parent and puts the MSPv1/MSPv2 parser in place of its own.
 * @param p_owner Pointer to the protocol instance.
 * @param p_timer Timer manager used for the receive timeout. May be NULL when PIF_MSP_RECEIVE_TIMEOUT is 0.
 * @param id Instance identifier. Use PIF_ID_AUTO for automatic assignment.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMspV2_Init(PifMspV2* p_owner, PifTimerManager* p_timer, PifId id);

/**
 * @fn pifMspV2_Clear
 * @brief Releases resources owned by the instance.
 * @param p_owner Pointer to the protocol instance.
 */
void pifMspV2_Clear(PifMspV2* p_owner);

/**
 * @fn pifMspV2_AttachUart
 * @brief Attaches a UART that the received bytes come from and the answers go to.
 * @param p_owner Pointer to the protocol instance.
 * @param p_uart UART interface bound to this protocol instance.
 */
void pifMspV2_AttachUart(PifMspV2* p_owner, PifUart* p_uart);

/**
 * @fn pifMspV2_DetachUart
 * @brief Detaches the UART attached with pifMspV2_AttachUart().
 * @param p_owner Pointer to the protocol instance.
 */
void pifMspV2_DetachUart(PifMspV2* p_owner);

/**
 * @fn pifMspV2_ParsingPacket
 * @brief Feeds one received byte to the parser, for a client that gets the bytes some other way than through
 *        an attached PifUart. Same as pifMsp_ParsingPacket() on &_msp.
 * @param p_owner Pointer to the protocol instance.
 * @param data Received byte.
 * @return What the byte did.
 */
PifMspFrame pifMspV2_ParsingPacket(PifMspV2* p_owner, uint8_t data);

/**
 * @fn pifMspV2_GetFrameSize
 * @brief Gives the size of the frame that carries a payload, header and checksums included.
 * @param version Frame the payload goes out in.
 * @param data_size Payload length in bytes.
 * @return Frame length in bytes.
 */
uint16_t pifMspV2_GetFrameSize(PifMspVersion version, uint16_t data_size);

/**
 * @fn pifMspV2_MakePacket
 * @brief Queues one frame in the answer buffer and wakes the TX task of the attached UART. The frame is
 *        queued whole or not at all.
 * @param p_owner Pointer to the protocol instance.
 * @param version Frame to send the packet in.
 * @param direction '<' for a command, '>' for a reply, '!' for an error reply.
 * @param flags MSPv2 flags. Not sent in an MSPv1 frame.
 * @param command Command. Only the low 8 bits are sent in an MSPv1 frame.
 * @param p_data Payload. May be NULL when data_size is 0.
 * @param data_size Payload length in bytes.
 * @return TRUE on success; FALSE if the frame does not fit in the answer buffer.
 */
BOOL pifMspV2_MakePacket(PifMspV2* p_owner, PifMspVersion version, uint8_t direction, uint8_t flags, uint16_t command,
		uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifMspV2_MakeAnswer
 * @brief Queues the reply to a received command, in the frame the command came in.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @param p_data Payload. May be NULL when data_size is 0.
 * @param data_size Payload length in bytes.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMspV2_MakeAnswer(PifMspV2* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifMspV2_MakeError
 * @brief Queues an error reply to a received command, in the frame the command came in.
 * @param p_owner Pointer to the protocol instance.
 * @param p_question Question packet that should be answered.
 * @param p_data Payload. May be NULL when data_size is 0.
 * @param data_size Payload length in bytes.
 * @return TRUE on success; otherwise FALSE.
 */
BOOL pifMspV2_MakeError(PifMspV2* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_V2_H
