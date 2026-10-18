// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rc_sbus.h
 * @brief Futaba SBUS receiver on a UART (100000 baud, 8E2, inverted).
 *
 * Decodes 16 proportional and 2 digital channels and the failsafe and frame lost flags of the receiver.
 */
#ifndef PIF_RC_SBUS_H
#define PIF_RC_SBUS_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SBUS_CHANNEL_COUNT 	18	///< 16 proportional and 2 digital channels

#define SBUS_FRAME_SIZE			25	///< Bytes of a frame


/**
* @class StPifRcSbus
* @brief Runtime state for SBUS decoding.
 */
typedef struct StPifRcSbus
{
	/// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
    uint8_t __index;                    ///< Bytes of the frame received so far
    uint8_t __buffer[SBUS_FRAME_SIZE];
    uint32_t __last_time;               ///< Time the last byte was received
    PifUart* __p_uart;
} PifRcSbus;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes an SBUS receiver instance.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSbus_Init(PifRcSbus* p_owner, PifId id);

/**
 * @brief Attaches a UART interface to the SBUS parser.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSbus_AttachUart(PifRcSbus* p_owner, PifUart* p_uart);

/**
 * @brief Detaches the UART interface from the SBUS parser.
 * @param p_owner Pointer to the SBUS receiver object.
 */
void pifRcSbus_DetachUart(PifRcSbus* p_owner);

/**
 * @brief Encodes and transmits one SBUS frame.
 * @param p_owner Pointer to the SBUS receiver object.
 * @param p_channel Pointer to raw 11-bit S.BUS channel values (0 to 2047, one step is 0.625 us
 *                  and 1024 is 1520 us), not the pulse widths in us that evt_receive gets.
 * @param count Number of channel values available in p_channel. Only the first 16 are sent, and
 *              channels from count on are sent as 0.
 * @return TRUE if the whole frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSbus_SendFrame(PifRcSbus* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SBUS_H
