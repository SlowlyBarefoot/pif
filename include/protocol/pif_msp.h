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
} PifMspPacket;


struct StPifMsp;
typedef struct StPifMsp PifMsp;

typedef void (*PifEvtMspReceive)(PifMsp* p_owner, PifMspPacket* p_packet);
typedef void (*PifEvtMspError)(PifId id);
typedef void (*PifEvtMspOtherPacket)(PifMsp* p_owner, uint8_t data);

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
typedef struct StPifMsp
{
	// Public Member Variable

    // Public Event Function
	PifEvtMspReceive evt_receive;
	PifEvtMspOtherPacket evt_other_packet;

	// Read-only Member Variable
    PifId _id;

	// Private Member Variable
	PifComm* __p_comm;
    PifMspRx __rx;
    PifMspTx __tx;
} PifMsp;


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
 * @fn pifMsp_MakeAnswer
 * @brief
 * @param p_owner
 * @param p_question
 * @param p_data
 * @param data_size
 * @return
 */
BOOL pifMsp_MakeAnswer(PifMsp* p_owner, PifMspPacket* p_question, uint8_t* p_data, uint16_t data_size);

/**
 * @fn pifMsp_MakeError
 * @brief
 * @param p_owner
 * @param p_question
 * @return
 */
BOOL pifMsp_MakeError(PifMsp* p_owner, PifMspPacket* p_question);

#ifdef __cplusplus
}
#endif


#endif  // PIF_MSP_H
