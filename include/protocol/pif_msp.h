#ifndef PIF_MSP_H
#define PIF_MSP_H


#include "core/pif_comm.h"
#include "core/pif_ring_buffer.h"
#include "core/pif_timer.h"


#ifndef PIF_MSP_RX_PACKET_SIZE
#define PIF_MSP_RX_PACKET_SIZE		128
#endif

#ifndef PIF_MSP_TX_ANSWER_SIZE
#define PIF_MSP_TX_ANSWER_SIZE		128
#endif

// 한 packet을 전부 받는 시간 제한
// 0 : 제한없음
// 1이상 : pifMsp_Init에서 받은 타이머의 단위를 곱한 시간
//         기본값은 50이고 타이머 단위가 1ms이면 50 * 1ms = 50ms이다.
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
 * @brief
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
 * @brief
 */
struct StPifMsp
{
	// Public Member Variable

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifComm* __p_comm;
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
 * @brief
 * @param p_owner
 * @param p_timer
 * @param id
 * @return
 */
BOOL pifMsp_Init(PifMsp* p_owner, PifTimerManager* p_timer, PifId id);

/**
 * @fn pifMsp_Clear
 * @brief
 * @param p_owner
 */
void pifMsp_Clear(PifMsp* p_owner);

/**
 * @fn pifMsp_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifMsp_AttachComm(PifMsp* p_owner, PifComm* p_comm);

/**
 * @fn pifMsp_DetachComm
 * @brief
 * @param p_owner
 */
void pifMsp_DetachComm(PifMsp* p_owner);

/**
 * @fn pifMsp_AttachEvtReceive
 * @brief
 * @param p_owner PifMsp 포인터
 * @param evt_receive
 * @param evt_other_packet
 * @param p_issuer 이벤트 발생시 전달할 발행자
 */
void pifMsp_AttachEvtReceive(PifMsp* p_owner, PifEvtMspReceive evt_receive, PifEvtMspOtherPacket evt_other_packet, PifIssuerP p_issuer);

/**
 * @fn pifMsp_ReadData8
 * @brief
 * @param p_packet
 * @return
 */
uint8_t pifMsp_ReadData8(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData16
 * @brief
 * @param p_packet
 * @return
 */
uint16_t pifMsp_ReadData16(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData32
 * @brief
 * @param p_packet
 * @return
 */
uint32_t pifMsp_ReadData32(PifMspPacket* p_packet);

/**
 * @fn pifMsp_ReadData
 * @brief
 * @param p_packet
 * @param p_data
 * @param size
 */
void pifMsp_ReadData(PifMspPacket* p_packet, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeAnswer
 * @brief
 * @param p_owner
 * @param p_question
 * @return
 */
BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_AddAnswer8
 * @brief
 * @param p_owner
 * @param data
 * @return
 */
BOOL pifMsp_AddAnswer8(PifMsp* p_owner, uint8_t data);

/**
 * @fn pifMsp_AddAnswer16
 * @brief
 * @param p_owner
 * @param data
 * @return
 */
BOOL pifMsp_AddAnswer16(PifMsp* p_owner, uint16_t data);

/**
 * @fn pifMsp_AddAnswer32
 * @brief
 * @param p_owner
 * @param data
 * @return
 */
BOOL pifMsp_AddAnswer32(PifMsp* p_owner, uint32_t data);

/**
 * @fn pifMsp_AddAnswer
 * @brief
 * @param p_owner
 * @param p_data
 * @param size
 * @return
 */
BOOL pifMsp_AddAnswer(PifMsp* p_owner, uint8_t* p_data, uint16_t size);

/**
 * @fn pifMsp_MakeError
 * @brief
 * @param p_owner
 * @param p_question
 * @return
 */
BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question);

/**
 * @fn pifMsp_SendAnswer
 * @brief
 * @param p_owner
 * @return
 */
BOOL pifMsp_SendAnswer(PifMsp* p_owner);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_H
