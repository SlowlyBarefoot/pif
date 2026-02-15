#ifndef PIF_RC_SBUS_H
#define PIF_RC_SBUS_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SBUS_CHANNEL_COUNT 	18

#define SBUS_FRAME_SIZE			25


/**
* @class StPifRcSbus
* @brief Runtime state for SBUS decoding.
 */
typedef struct StPifRcSbus
{
	// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
    PifUart* __p_uart;
    uint8_t __index;                    // Current message length.
    uint8_t __buffer[SBUS_FRAME_SIZE];
    uint32_t __last_time;
} PifRcSbus;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn pifRcSbus_Init
 * @brief Initializes an SBUS receiver instance.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSbus_Init(PifRcSbus* p_owner, PifId id);

/**
 * @fn pifRcSbus_AttachUart
 * @brief Attaches a UART interface to the SBUS parser.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSbus_AttachUart(PifRcSbus* p_owner, PifUart* p_uart);

/**
 * @fn pifRcSbus_DetachUart
 * @brief Detaches the UART interface from the SBUS parser.
 * @param p_owner Pointer to the SBUS receiver object.
 */
void pifRcSbus_DetachUart(PifRcSbus* p_owner);

/**
 * @fn pifRcSbus_DetachUart
 * @brief Encodes and transmits one SBUS frame.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param p_channel Pointer to channel values.
 * @param count Number of channel values available in p_channel.
 * @return TRUE if the frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSbus_SendFrame(PifRcSbus* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SBUS_H
