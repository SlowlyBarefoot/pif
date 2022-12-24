#ifndef PIF_RC_SBUS_H
#define PIF_RC_SBUS_H


#include "core/pif_comm.h"
#include "rc/pif_rc.h"


#define PIF_SBUS_CHANNEL_COUNT 	18

#define SBUS_FRAME_SIZE			25


/**
 * @class StPifRcSbus
 * @brief
 */
typedef struct StPifRcSbus
{
	// Must be at the front
	PifRc parent;

	// Public Member Variable

	// Read-only Member Variable

	// Private Member Variable
	PifComm* __p_comm;
	uint8_t __index;                    // message length
	uint8_t __buffer[SBUS_FRAME_SIZE];	// message buffer
	uint32_t __last_time;
} PifRcSbus;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcSbus_Init
 * @brief
 * @param p_owner
 * @param id
 * @return
 */
BOOL pifRcSbus_Init(PifRcSbus* p_owner, PifId id);

/**
 * @fn pifRcSbus_AttachComm
 * @brief
 * @param p_owner
 * @param p_comm
 */
void pifRcSbus_AttachComm(PifRcSbus* p_owner, PifComm* p_comm);

/**
 * @fn pifRcSbus_DetachComm
 * @brief
 * @param p_owner
 */
void pifRcSbus_DetachComm(PifRcSbus* p_owner);

/**
 * @fn pifRcSbus_SendFrame
 * @brief
 * @param p_owner
 * @param p_channel
 * @param count
 * @return
 */
BOOL pifRcSbus_SendFrame(PifRcSbus* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SBUS_H
