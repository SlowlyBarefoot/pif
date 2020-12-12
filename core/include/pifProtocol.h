#ifndef PIF_PROTOCOL_H
#define PIF_PROTOCOL_H


#include "pifComm.h"
#include "pifPulse.h"
#include "pifRingBuffer.h"


//#define __DEBUG_PACKET__

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
//         기본값은 200이고 타이머 단위가 1ms이면 200 * 1ms = 200ms이다.
#ifndef PIF_PROTOCOL_RECEIVE_TIMEOUT
#define PIF_PROTOCOL_RECEIVE_TIMEOUT	200
#endif

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
#define ASCII_DC2	18	// Device Control 2
#define ASCII_DC3	19	// Device Control 3
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


typedef enum _PIF_enProtocolType
{
    PT_enSimple		= 1,
    PT_enMedium		= 2
} PIF_enProtocolType;

typedef enum _PIF_enProtocolFlags
{
	PF_enDefault			= 0x00,

	// 명령의 종류
	PF_enType_Mask			= 0x01,
	PF_enType_Request		= 0x00,	// Default
	PF_enType_Response		= 0x01,

	// 요청 명령의 응답 요구 선택 여부
	PF_enResponse_Mask		= 0x02,
	PF_enResponse_Yes		= 0x00,	// Default
	PF_enResponse_No		= 0x02,

	// 명령 송수신시 로그 출력 사용 여부
	PF_enLogPrint_Mask		= 0x04,
	PF_enLogPrint_No		= 0x00,	// Default
	PF_enLogPrint_Yes		= 0x04,

	// 대용량 데이타 전송시 사용
	PF_enMulti_Mask			= 0x30,
	PF_enMulti_No			= 0x00,	// Default
	PF_enMulti_First		= 0x10,
	PF_enMulti_Middle		= 0x20,
	PF_enMulti_End			= 0x30,

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
    PIF_evtProtocolFinish evtFinish;
    uint8_t ucRetry;
    uint16_t usTimeout;
} PIF_stProtocolRequest;

/**
 * @class _PIF_stProtocolResponse
 * @brief
 */
typedef struct _PIF_stProtocolResponse
{
    uint8_t ucCommand;
    uint8_t enFlags;					// PIF_enProtocolFlags
    PIF_evtProtocolFinish evtFinish;
} PIF_stProtocolResponse;

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
		const PIF_stProtocolRequest *pstRequest, const PIF_stProtocolResponse *pstResponse);

BOOL pifProtocol_ResizeRxPacket(PIF_stProtocol *pstOwner, uint16_t usRxPacketSize);
BOOL pifProtocol_ResizeTxRequest(PIF_stProtocol *pstOwner, uint16_t usTxRequestSize);
BOOL pifProtocol_ResizeTxResponse(PIF_stProtocol *pstOwner, uint16_t usTxResponseSize);

void pifProtocol_AttachComm(PIF_stProtocol *pstOwner, PIF_stComm *pstComm);

BOOL pifProtocol_SetOwnerId(PIF_stProtocol *pstOwner, uint8_t ucOwnerId);

void pifProtocol_Sended(void *pvOwner);

BOOL pifProtocol_MakeRequest(PIF_stProtocol *pstOwner, uint8_t ucCommand, uint8_t *pucData, uint16_t usDataSize);
BOOL pifProtocol_MakeResponse(PIF_stProtocol *pstOwner, uint8_t ucCommand, uint8_t *pucData, uint16_t usDataSize);

#ifdef __cplusplus
}
#endif


#endif  // PIF_PROTOCOL_H
