// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file pif_rc_spektrum.h
 * @brief Spektrum satellite receiver (DSM2, DSMX) on a UART.
 *
 * Decodes the frames of 1024 and 2048 step receivers. pifRcSpektrum_SendFrame() encodes channels the other way,
 * for a device that acts as the receiver.
 */
#ifndef PIF_RC_SPEKTRUM_H
#define PIF_RC_SPEKTRUM_H


#include "communication/pif_uart.h"
#include "rc/pif_rc.h"


#define PIF_SPEKTRUM_CHANNEL_COUNT  				8	///< Channels of a frame

/** @name Protocol IDs
 *  Protocol ID in the second byte of a frame: frame period, steps and modulation.
 *  @{
 */
#define PIF_SPEKTRUM_PROTOCOL_ID_22MS_1024_DSM2		0x01	///< 22 ms, 1024 steps, DSM2
#define PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSM2		0x12	///< 11 ms, 2048 steps, DSM2
#define PIF_SPEKTRUM_PROTOCOL_ID_22MS_2048_DSMS		0xA2	///< 22 ms, 2048 steps, DSMX (named DSMS here)
#define PIF_SPEKTRUM_PROTOCOL_ID_11MS_2048_DSMX		0xB2	///< 11 ms, 2048 steps, DSMX
/** @} */

#define SPEKTRUM_FRAME_SIZE   						16	///< Bytes of a frame


/**
 * @class StPifRcSpektrum
 * @brief Runtime state for Spektrum frame decoding.
 */
typedef struct StPifRcSpektrum
{
	/// The parent variable must be at the beginning of this structure.
	PifRc parent;

	// Read-only Member Variable
	uint8_t _protocol_id;	///< Protocol ID the receiver was set up with
	uint8_t _pos_shift;		///< Right shift that brings a raw position down to 1024 steps (us above 988)

	// Private Member Variable
    uint8_t __index;    ///< Current message length.
    uint8_t __id_mask;
    uint8_t __id_shift;
    uint8_t __pos_mask;
    uint8_t __last_ch;      ///< Channel sent in the last slot of the next frame
    uint8_t __fades;        ///< Fade count of the last frame
    BOOL __hunting;         ///< A frame start is being searched for after a bad system byte
    uint8_t __p_buffer[SPEKTRUM_FRAME_SIZE];
    uint16_t __channel[PIF_SPEKTRUM_CHANNEL_COUNT];
	uint32_t __last_time;   ///< Time the last byte was received
	PifUart* __p_uart;
} PifRcSpektrum;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes a Spektrum receiver instance.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param id Object identifier. Use PIF_ID_AUTO to allocate automatically.
 * @param protocol_id Spektrum protocol identifier.
 * @return TRUE if initialization succeeds, otherwise FALSE.
 */
BOOL pifRcSpektrum_Init(PifRcSpektrum* p_owner, PifId id, uint8_t protocol_id);

/**
 * @brief Attaches a UART interface to the Spektrum parser.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param p_uart UART instance to attach.
 */
void pifRcSpektrum_AttachUart(PifRcSpektrum* p_owner, PifUart* p_uart);

/**
 * @brief Detaches the UART interface from the Spektrum parser.
 * @param p_owner Pointer to the Spektrum receiver object.
 */
void pifRcSpektrum_DetachUart(PifRcSpektrum* p_owner);

/**
 * @brief Encodes and transmits one Spektrum frame.
 * @param p_owner Pointer to the Spektrum receiver object.
 * @param p_channel Pointer to raw servo positions (0 to 1023 for the 1024 protocol, 0 to 2047
 *                  for the others), not the pulse widths in us that evt_receive gets.
 * @param count Number of channel values available in p_channel. The first 6 channels go in every
 *              frame, and the last slot carries channel 7, or channels 7 and 8 in turn. Slots
 *              without a channel are sent empty.
 * @return TRUE if the whole frame is queued for transmission, otherwise FALSE.
 */
BOOL pifRcSpektrum_SendFrame(PifRcSpektrum* p_owner, uint16_t* p_channel, uint8_t count);

#ifdef __cplusplus
}
#endif


#endif	// PIF_RC_SPEKTRUM_H
