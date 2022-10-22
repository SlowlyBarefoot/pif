#ifndef PIF_RC_SUMD_H
#define PIF_RC_SUMD_H


#include "core/pif_comm.h"
#include "protocol/pif_rc.h"


#define PIF_SUMD_CHANNEL_COUNT  32  //shall be equal or smaller than 32

//SUMD protocol constant
#define SUMD_HEADER_SIZE        3
#define SUMD_CRC_SIZE           2

//SUMD decoding buffer declaration
#define SUMD_FRAME_SIZE   		(SUMD_HEADER_SIZE + PIF_SUMD_CHANNEL_COUNT * 2 + SUMD_CRC_SIZE + 1) //buffer size : 1 extra byte for easy empty/full detection using start and stop


/**
 * @class StPifRcSumd
 * @brief
 */
typedef struct StPifRcSumd
{
	PifRc parent;

	// Public Member Variable

    // Public Event Function
	PifEvtRcReceive evt_receive;

	// Read-only Member Variable

	// Private Member Variable
	PifComm* __p_comm;
    uint8_t __p_buffer[SUMD_FRAME_SIZE];  //buffer size : 2 Bytes per channel + synchro, failsafe, channel nb, chksum(2) and 1 extra byte for empty/full detection 
	uint8_t __index;                            // message length
	uint32_t __last_time;
} PifRcSumd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcSumd_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifRcSumd_Init(PifRcSumd* p_owner, PifId id);

/**
 * @fn pifRcSumd_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifRcSumd_AttachComm(PifRcSumd* p_owner, PifComm* p_comm);

/**
 * @fn pifRcSumd_DetachComm
 * @brief
 * @param p_owner
 */
void pifRcSumd_DetachComm(PifRcSumd* p_owner);

/**
 * @fn pifRcSumd_CheckFailSafe
 * @brief function used to check the failsafe status
 * @param p_owner
 * @return
 */
BOOL pifRcSumd_CheckFailSafe(PifRcSumd* p_owner);

/**
 * @fn pifRcSumd_SendFrame
 * @brief
 * @param p_owner
 * @param p_channel
 * @param count
 * @return
 */
BOOL pifRcSumd_SendFrame(PifRcSumd* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SUMD_H
