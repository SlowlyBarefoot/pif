// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rc_sumd.h
 * @brief Graupner SUMD receiver on a UART.
 *
 * Decodes frames of up to PIF_SUMD_CHANNEL_COUNT channels with their CRC-16 and the failsafe state of the
 * receiver.
 */
#ifndef PIF_RC_SUMD_H
#define PIF_RC_SUMD_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SUMD_CHANNEL_COUNT  32  ///< Must be less than or equal to 32.

/// SUMD protocol constants.
#define SUMD_HEADER_SIZE        3
#define SUMD_CRC_SIZE           2	///< Bytes of the CRC

/// Receive buffer size: a frame with the most channels, plus one spare byte.
#define SUMD_FRAME_SIZE         (SUMD_HEADER_SIZE + PIF_SUMD_CHANNEL_COUNT * 2 + SUMD_CRC_SIZE + 1)


/**
 * @class StPifRcSumd
 * @brief Runtime state for SUMD frame decoding.
 */
typedef struct StPifRcSumd
{
	/// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Private Member Variable
    uint8_t __p_buffer[SUMD_FRAME_SIZE];
    uint8_t __index;    ///< Current message length.
    uint32_t __last_time;   ///< Time the last byte was received
    PifUart* __p_uart;
} PifRcSumd;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a SUMD receiver instance.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSumd_Init(PifRcSumd* p_owner, PifId id);

/**
 * @brief Attaches a UART interface to the SUMD parser.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSumd_AttachUart(PifRcSumd* p_owner, PifUart* p_uart);

/**
 * @brief Detaches the UART interface from the SUMD parser.
 * @param p_owner Pointer to the SUMD receiver object.
 */
void pifRcSumd_DetachUart(PifRcSumd* p_owner);

/**
 * @brief Encodes and transmits one SUMD frame.
 * @param p_owner Pointer to the SUMD receiver object.
 * @param p_channel Pointer to channel values in 1/8 us steps (12000 is 1500 us), not the pulse
 *                  widths in us that evt_receive gets.
 * @param count Number of channels to send, 2 to PIF_SUMD_CHANNEL_COUNT.
 * @return TRUE if the whole frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSumd_SendFrame(PifRcSumd* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SUMD_H
