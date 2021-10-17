#ifndef PIF_PROTOCOL_H
#define PIF_PROTOCOL_H


#include "pifComm.h"
#include "pifPulse.h"
#include "pifRingBuffer.h"


#define __DEBUG_PACKET__


typedef enum EnPifProtocolType
{
    PT_SMALL			= 1,
    PT_MEDIUM			= 2,
} PifProtocolType;

typedef enum EnPifProtocolFlags
{
	PF_DEFAULT			= 0x00,

	// 명령의 종류
	PF_TYPE_MASK		= 0x01,
	PF_TYPE_REQUEST		= 0x00,	// Default
	PF_TYPE_RESPONSE	= 0x01,
	PF_TYPE_QUESTION	= 0x00,	// Default
	PF_TYPE_ANSWER		= 0x01,

	// 요청 명령에 응답 요구 선택 여부
	PF_RESPONSE_MASK	= 0x02,
	PF_RESPONSE_YES		= 0x00,	// Default
	PF_RESPONSE_NO		= 0x02,

	// 질문 명령에 답변 요구 선택 여부
	PF_ANSWER_MASK		= 0x02,
	PF_ANSWER_YES		= 0x00,	// Default
	PF_ANSWER_NO		= 0x02,

	// 명령 송수신시 로그 출력 사용 여부
	PF_LOG_PRINT_MASK	= 0x08,
	PF_LOG_PRINT_NO		= 0x00,	// Default
	PF_LOG_PRINT_YES	= 0x08,

	PF_ALWAYS			= 0x80
} PifProtocolFlags;

typedef enum EnPifProtocolRxState
{
	PRS_IDLE			= 0,
	PRS_GET_HEADER		= 1,
	PRS_GET_DATA		= 2,
	PRS_GET_CRC			= 3,
	PRS_GET_TAILER		= 4,
	PRS_DONE			= 5,
	PRS_ACK				= 6,
	PRS_ERROR			= 7
} PifProtocolRxState;

typedef enum EnPifProtocolTxState
{
	PTS_IDLE			= 0,
	PTS_SENDING			= 1,
	PTS_WAIT_SENDED		= 2,
	PTS_WAIT_RESPONSE	= 3,
	PTS_RETRY_DELAY		= 4,
	PTS_RETRY			= 5
} PifProtocolTxState;


/**
 * @class StPifProtocolPacket
 * @brief
 */
typedef struct StPifProtocolPacket
{
	PifProtocolFlags flags;
	uint8_t command;
	uint16_t length;
	uint8_t crc;
	uint8_t packet_id;		// PT_Medium
	uint8_t* p_data;
	uint16_t data_count;
} PifProtocolPacket;

typedef void (*PifEvtProtocolFinish)(PifProtocolPacket* p_packet);
typedef void (*PifEvtProtocolError)(PifId id);

/**
 * @class StPifProtocolRequest
 * @brief
 */
typedef struct StPifProtocolRequest
{
    uint8_t command;
    uint8_t flags;					// PIF_enProtocolFlags
    PifEvtProtocolFinish evt_response;
    uint8_t retry;
    uint16_t timeout;
} PifProtocolRequest;

/**
 * @class StPifProtocolQuestion
 * @brief
 */
typedef struct StPifProtocolQuestion
{
    uint8_t command;
    uint8_t flags;					// PIF_enProtocolFlags
    PifEvtProtocolFinish evt_answer;
} PifProtocolQuestion;

/**
 * @class StPifProtocolRx
 * @brief
 */
typedef struct StPifProtocolRx
{
	PifProtocolRxState state;
	uint8_t* p_packet;
	uint16_t packet_size;
	uint8_t header_count;
	BOOL data_link_escape;
	PifProtocolPacket packet;
#if PIF_PROTOCOL_RECEIVE_TIMEOUT
	PifPulseItem* p_timer;
#endif
} PifProtocolRx;

/**
 * @class StPifProtocolTx
 * @brief
 */
typedef struct StPifProtocolTx
{
    PifRingBuffer request_buffer;
    PifRingBuffer answer_buffer;
	const PifProtocolRequest* p_request;
	uint8_t* p_data;
	uint16_t data_size;
	PifProtocolTxState state;
	union {
		uint8_t info[9];
		struct {
			uint16_t length;
			uint16_t timeout;
			uint8_t retry;
			uint8_t stx;
			uint8_t flags;
			uint8_t command;
			uint8_t packet_id;
		} st;
	} ui;
	uint16_t pos;
	PifPulseItem* p_timer;
} PifProtocolTx;

/**
 * @class StPifProtocol
 * @brief
 */
typedef struct StPifProtocol
{
	// Public Member Variable

	// Public Event Function
    PifEvtProtocolError evt_error;

	// Read-only Member Variable
    PifId _id;
    PifProtocolType _type;
    uint8_t _frame_size;

	// Private Member Variable
    PifPulse* __p_timer;
	PifComm* __p_comm;
    const PifProtocolQuestion* __p_questions;
    PifProtocolRx __rx;
    PifProtocolTx __tx;
	uint8_t __header_size;
	uint8_t __packet_id;
} PifProtocol;


#ifdef __cplusplus
extern "C" {
#endif

PifProtocol* pifProtocol_Create(PifId id, PifPulse* p_timer, PifProtocolType type,
		const PifProtocolQuestion* p_questions);
void pifProtocol_Destroy(PifProtocol** pp_owner);

BOOL pifProtocol_SetFrameSize(PifProtocol* p_owner, uint8_t frame_size);

BOOL pifProtocol_ResizeRxPacket(PifProtocol* p_owner, uint16_t rx_packet_size);
BOOL pifProtocol_ResizeTxRequest(PifProtocol* p_owner, uint16_t tx_request_size);
BOOL pifProtocol_ResizeTxResponse(PifProtocol* p_owner, uint16_t tx_response_size);

void pifProtocol_AttachComm(PifProtocol* p_owner, PifComm* p_comm);

BOOL pifProtocol_MakeRequest(PifProtocol* p_owner, const PifProtocolRequest* p_request, uint8_t* p_data, uint16_t data_size);
BOOL pifProtocol_MakeAnswer(PifProtocol* p_owner, PifProtocolPacket* p_question, uint8_t flags,
		uint8_t *p_data, uint16_t data_size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PROTOCOL_H
