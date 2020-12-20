#ifndef PIF_PROTOCOL_H
#define PIF_PROTOCOL_H


#include "pifComm.h"
#include "pifPulse.h"
#include "pifRingBuffer.h"


#ifndef PIF_PROTOCOL_RX_PACKET_SIZE
#define PIF_PROTOCOL_RX_PACKET_SIZE		32
#endif

#ifndef PIF_PROTOCOL_TX_REQUEST_SIZE
#define PIF_PROTOCOL_TX_REQUEST_SIZE	64
#endif

#ifndef PIF_PROTOCOL_TX_RESPONSE_SIZE
#define PIF_PROTOCOL_TX_RESPONSE_SIZE	32
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

//#define __DEBUG_PACKET__


typedef enum _PIF_enProtocolType
{
    PT_enSmall			= 1,
    PT_enMedium			= 2,
} PIF_enProtocolType;

typedef enum _PIF_enProtocolFlags
{
	PF_enDefault			= 0x00,

	// 명령의 종류
	PF_enType_Mask			= 0x01,
	PF_enType_Request		= 0x00,	// Default
	PF_enType_Response		= 0x01,
	PF_enType_Question		= 0x00,	// Default
	PF_enType_Answer		= 0x01,

	// 요청 명령의 응답 요구 선택 여부
	PF_enResponse_Mask		= 0x06,
	PF_enResponse_Yes		= 0x00,	// Default
	PF_enResponse_Ack		= 0x02,
	PF_enResponse_No		= 0x04,

	// 명령 송수신시 로그 출력 사용 여부
	PF_enLogPrint_Mask		= 0x08,
	PF_enLogPrint_No		= 0x00,	// Default
	PF_enLogPrint_Yes		= 0x08,

	PF_enAlways				= 0x80
} PIF_enProtocolFlags;

/**
 * @class _PIF_stProtocolPacket
 * @brief
 */
typedef struct _PIF_stProtocolPacket
{
	PIF_enProtocolFlags enFlags;
	uint8_t ucCommand;
	uint16_t usLength;
	uint8_t ucCrc;
	uint8_t ucPacketId;		// PT_Medium
	uint8_t *pucData;
	uint16_t usDataCount;
} PIF_stProtocolPacket;

typedef void (*PIF_evtProtocolFinish)(PIF_stProtocolPacket *pstPacket);
typedef void (*PIF_evtProtocolError)(PIF_unDeviceCode unDeviceCode);

/**
 * @class _PIF_stProtocolRequest
 * @brief
 */
typedef struct _PIF_stProtocolRequest
{
    uint8_t ucCommand;
    uint8_t enFlags;					// PIF_enProtocolFlags
    PIF_evtProtocolFinish evtResponse;
    uint8_t ucRetry;
    uint16_t usTimeout;
} PIF_stProtocolRequest;

/**
 * @class _PIF_stProtocolQuestion
 * @brief
 */
typedef struct _PIF_stProtocolQuestion
{
    uint8_t ucCommand;
    uint8_t enFlags;					// PIF_enProtocolFlags
    PIF_evtProtocolFinish evtQuestion;
} PIF_stProtocolQuestion;

/**
 * @class _PIF_stProtocol
 * @brief
 */
typedef struct _PIF_stProtocol
{
	// Public Member Variable
    PIF_unDeviceCode unDeviceCode;
    PIF_enProtocolType enType;
	uint8_t ucOwnerId;				// Default : 0xFF

	// Public Event Function
    PIF_evtProtocolError evtError;
} PIF_stProtocol;


#ifdef __cplusplus
extern "C" {
#endif

BOOL pifProtocol_Init(PIF_stPulse *pstTimer, uint8_t ucSize);
void pifProtocol_Exit();

PIF_stProtocol *pifProtocol_Add(PIF_unDeviceCode unDeviceCode, PIF_enProtocolType enType,
		const PIF_stProtocolQuestion *pstQuestions);

BOOL pifProtocol_ResizeRxPacket(PIF_stProtocol *pstOwner, uint16_t usRxPacketSize);
BOOL pifProtocol_ResizeTxRequest(PIF_stProtocol *pstOwner, uint16_t usTxRequestSize);
BOOL pifProtocol_ResizeTxResponse(PIF_stProtocol *pstOwner, uint16_t usTxResponseSize);

void pifProtocol_AttachComm(PIF_stProtocol *pstOwner, PIF_stComm *pstComm);

BOOL pifProtocol_SetOwnerId(PIF_stProtocol *pstOwner, uint8_t ucOwnerId);

BOOL pifProtocol_MakeRequest(PIF_stProtocol *pstOwner, const PIF_stProtocolRequest *pstRequest, uint8_t *pucData, uint16_t usDataSize);
BOOL pifProtocol_MakeAnswer(PIF_stProtocol *pstOwner, PIF_stProtocolPacket *pstQuestion, uint8_t enFlags,
		uint8_t *pucData, uint16_t usDataSize);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PROTOCOL_H
