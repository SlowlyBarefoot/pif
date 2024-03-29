#ifndef PIF_PROTOCOL_H
#define PIF_PROTOCOL_H


#include "communication/pif_uart.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer.h"


#ifndef PIF_PROTOCOL_RX_PACKET_SIZE
#define PIF_PROTOCOL_RX_PACKET_SIZE		32
#endif

#ifndef PIF_PROTOCOL_TX_REQUEST_SIZE
#define PIF_PROTOCOL_TX_REQUEST_SIZE	64
#endif

#ifndef PIF_PROTOCOL_TX_ANSWER_SIZE
#define PIF_PROTOCOL_TX_ANSWER_SIZE		32
#endif

// 한 packet을 전부 받는 시간 제한
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 50이고 타이머 단위가 1ms이면 50 * 1ms = 50ms이다.
#ifndef PIF_PROTOCOL_RECEIVE_TIMEOUT
#define PIF_PROTOCOL_RECEIVE_TIMEOUT	50
#endif

// Retry하기 전 delay 시간
// 0 : 제한없음
// 1이상 : pifProtocol_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 10이고 타이머 단위가 1ms이면 10 * 1ms = 10ms이다.
#ifndef PIF_PROTOCOL_RETRY_DELAY
#define PIF_PROTOCOL_RETRY_DELAY		10
#endif

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
	PifTimer* p_timer;
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
	PifTimer* p_timer;
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

	// Private Member Variable
    PifTimerManager* __p_timer_manager;
	PifUart* __p_uart;
    const PifProtocolQuestion* __p_questions;
    PifProtocolRx __rx;
    PifProtocolTx __tx;
	uint8_t __header_size;
	uint8_t __packet_id;
} PifProtocol;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifProtocol_Init
 * @brief
 * @param p_owner
 * @param id
 * @param p_timer_manager
 * @param type
 * @param p_questions
 * @return
 */
BOOL pifProtocol_Init(PifProtocol* p_owner, PifId id, PifTimerManager* p_timer_manager, PifProtocolType type,
		const PifProtocolQuestion* p_questions);

/**
 * @fn pifProtocol_Clear
 * @brief
 * @param pp_owner
 */
void pifProtocol_Clear(PifProtocol* p_owner);

/**
 * @fn pifProtocol_ResizeRxPacket
 * @brief
 * @param pvOwner
 * @param rx_packet_size
 * @return
 */
BOOL pifProtocol_ResizeRxPacket(PifProtocol* p_owner, uint16_t rx_packet_size);

/**
 * @fn pifProtocol_ResizeTxRequest
 * @brief
 * @param pvOwner
 * @param tx_request_size
 * @return
 */
BOOL pifProtocol_ResizeTxRequest(PifProtocol* p_owner, uint16_t tx_request_size);

/**
 * @fn pifProtocol_ResizeTxResponse
 * @brief
 * @param pvOwner
 * @param tx_response_size
 * @return
 */
BOOL pifProtocol_ResizeTxResponse(PifProtocol* p_owner, uint16_t tx_response_size);

/**
 * @fn pifProtocol_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifProtocol_AttachUart(PifProtocol* p_owner, PifUart* p_uart);

/**
 * @fn pifMProtocol_DetachUart
 * @brief
 * @param p_owner
 */
void pifMProtocol_DetachUart(PifProtocol* p_owner);

/**
 * @fn pifProtocol_MakeRequest
 * @brief
 * @param p_owner
 * @param p_request
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifProtocol_MakeRequest(PifProtocol* p_owner, const PifProtocolRequest* p_request, uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifProtocol_MakeAnswer
 * @brief
 * @param p_owner
 * @param p_question
 * @param flags
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifProtocol_MakeAnswer(PifProtocol* p_owner, PifProtocolPacket* p_question, uint8_t flags,
		uint8_t *p_data, uint16_t data_size);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PROTOCOL_H
