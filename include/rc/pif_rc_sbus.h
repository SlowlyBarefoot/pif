#ifndef PIF_RC_SBUS_H
#define PIF_RC_SBUS_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SBUS_CHANNEL_COUNT 	18

#define SBUS_FRAME_SIZE			25


/**
 * @class StPifRcSbus
 * @brief
 */
typedef struct StPifRcSbus
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Public Member Variable

	// Read-only Member Variable

	// Private Member Variable
	PifUart* __p_uart;
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
 * @fn pifRcSbus_AttachUart
 * @brief
 * @param p_owner
 * @param p_uart
 */
void pifRcSbus_AttachUart(PifRcSbus* p_owner, PifUart* p_uart);

/**
 * @fn pifRcSbus_DetachUart
 * @brief
 * @param p_owner
 */
void pifRcSbus_DetachUart(PifRcSbus* p_owner);

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
